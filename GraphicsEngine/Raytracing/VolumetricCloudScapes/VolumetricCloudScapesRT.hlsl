#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Noise.hlsli"
#include "VolumetricCloudScapes.hlsli"

/**
* [EN]
* Reference:
* - https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf
*   (Schneider & Vos, "The Real-time Volumetric Cloudscapes of Horizon: Zero
*   Dawn", SIGGRAPH 2015 Advances in Real-Time Rendering - the curved shell +
*   weather-map + adaptive-stepping raymarch this pass is built on.)
* - https://advances.realtimerendering.com/s2017/Nubis%20-%20Authoring%20Realtime%20Volumetric%20Cloudscapes%20with%20the%20Decima%20Engine%20-%20Final%20.pdf
*   (Schneider, "Nubis: Authoring Real-Time Volumetric Cloudscapes with the
*   Decima Engine", SIGGRAPH 2017 - the follow-up covering the detail-erosion
*   and weather-authoring pipeline CloudDensity implements.)
* - https://history.siggraph.org/wp-content/uploads/2022/10/2015-Talks-Wrenninge_Art-Directable-Multiple-Volumetric-Scattering.pdf
*   (Wrenninge et al., "Art-Directable Multiple Volumetric Scattering",
*   SIGGRAPH 2015 - the octave-stack multiple-scattering approximation
*   CloudMultiScatterSetup/CloudSunLight implement, in VolumetricCloudScapes.hlsli.)
*
* Volumetric cloud scapes (screen-space raymarch, compute). Marches the view
* ray through a curved cloud SHELL (a layer wrapped around a planet of
* planet_radius_, so it bends down and converges at the horizon instead of
* extending forever as a flat slab), sampling PRE-BAKED tileable Texture3Ds
* (Perlin-Worley shape 128^3 + Worley detail 64^3, baked once by
* CloudNoiseShape/DetailBakeCS.hlsl). A low-frequency slice of the shape volume
* acts as a weather map so the sky gets large cloud masses and clear gaps.
* Stepping is adaptive: coarse strides skip empty space, fine strides integrate
* inside cloud, and both grow with distance in place of a mip chain. Lighting is
* a dual-lobe phase with a Wrenninge multiple-scattering octave stack, a
* Beer-powder term, a geometrically-stepped sun lightmarch that follows the real
* path length through the layer, and a sky-gradient ambient term. Distant clouds
* wash toward the horizon color (aerial perspective) and fade out at the march
* limit. Active only when no skymap is bound (the procedural sky replaces the
* skybox; DeferredLightingPS.hlsl draws sky+sun and blends this texture over
* it). Sky pixels only, no TLAS.
*
* ---------------------------------------------------------------------
*
* [JP]
* ボリューメトリック・クラウドスケープ(スクリーン空間レイマーチ、compute)。
* 視線を【曲率つきの雲シェル】(planet_radius_ の球殻。無限平板ではないので
* 遠方で下がって地平線に収束する)に沿ってマーチし、【ベイク済み】タイル可能
* Texture3D(Perlin-Worley 形状 128^3 + Worley ディテール 64^3、
* CloudNoiseShape/DetailBakeCS.hlsl が一度だけ焼く)をサンプルする。形状
* ボリュームの低周波スライスを weather map として使い、空に大きな雲の塊と
* 晴れ間を作る。ステップは適応的 - 空(密度0)は粗ステップで飛ばし、雲の中だけ
* 細ステップで積分し、どちらも距離に応じて伸ばして mip の代わりにする。
* ライティングはデュアルローブ位相 + Wrenninge の多重散乱オクターブ +
* Beer-powder + 層内の実光路長に沿った指数ステップのライトマーチ +
* 空グラデーション環境光。遠景の雲は地平線色へ寄せ(空気遠近)、マーチ上限へ
* 向けてフェードアウトする。スカイマップ未バインド時のみ有効(プロシージャル空
* がスカイボックスを置き換え、DeferredLightingPS.hlsl が空+太陽を描いた上に
* このテクスチャを合成する)。空ピクセル限定、TLAS 不使用。
*/

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.cloud_.output_uav_index_];

	/// [EN] This pass runs at REDUCED resolution
	///      (VolumetricCloudScapesRenderer's resolutionDivisor_). Both the
	///      bounds check and the ray direction are based on the output
	///      texture's own size, not the screen size.
	/// [JP] このパスは【縮小解像度】で走る(VolumetricCloudScapesRenderer の
	///      resolutionDivisor_)。境界判定もレイ方向も画面サイズではなく出力
	///      テクスチャ自身のサイズを基準にする。
	uint cloud_width;
	uint cloud_height;
	output.GetDimensions(cloud_width, cloud_height);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      8x8 thread group size, so threads past the actual (reduced)
	///      output texture's edge must bail out before touching any
	///      resource.
	/// [JP] 範囲外ガード: ディスパッチは 8x8 スレッドグループの倍数に
	///      切り上げられているので、実際の(縮小)出力テクスチャ端を超えた
	///      スレッドはどのリソースにも触れる前に抜ける必要がある。
	if (dtid.x >= cloud_width || dtid.y >= cloud_height)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	/// [EN] Any pixel with scene geometry is always in front of clouds
	///      (which are effectively at infinity), so it is skipped.
	/// [JP] シーンジオメトリがあるピクセルは雲(遠景)より必ず手前なので
	///      スキップ。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];

	/// [EN] Depth is at FULL resolution, so this looks at the whole block of
	///      full-res pixels this one reduced-res pixel covers. Clouds are
	///      computed if EVEN ONE of them is sky - requiring "all four are
	///      sky" would, once the composite side bilinearly upsamples this
	///      texture, pick up a 0 right at a geometry silhouette's edge and
	///      show up as a dark halo.
	/// [JP] 深度は【フル解像度】なので、この1ピクセルが覆うブロック全体を
	///      見る。1つでも空があれば雲を計算する - 「4つとも空」を条件に
	///      すると、合成側でバイリニア補間したときにジオメトリのシルエット
	///      際で 0 を拾って暗いハローが出る。
	uint2 screen_size = uint2(max(scene.screen_size_.x, 1.0), max(scene.screen_size_.y, 1.0));
	uint2 block_scale = max(screen_size / max(uint2(cloud_width, cloud_height), uint2(1, 1)), uint2(1, 1));
	uint2 block_origin = pixel * block_scale;

	bool any_sky = false;
	for (uint block_y = 0; block_y < block_scale.y; block_y++)
	{
		for (uint block_x = 0; block_x < block_scale.x; block_x++)
		{
			uint2 sample_pixel = min(block_origin + uint2(block_x, block_y), screen_size - uint2(1, 1));
			if (depth_texture.Load(int3(sample_pixel, 0)) == 0.0)
			{
				any_sky = true;
			}
		}
	}

	if (!any_sky)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];

	Texture3D<float> shape_noise = ResourceDescriptorHeap[structured_indices.cloud_.shape_noise_srv_index_];
	Texture3D<float> detail_noise = ResourceDescriptorHeap[structured_indices.cloud_.detail_noise_srv_index_];

	/// [EN] Reconstruct the view direction (the direction vector pointing
	///      toward the far clip = 0 under reverse-Z). UV is taken at the
	///      reduced resolution - the pixel center matches the center of the
	///      block it covers.
	/// [JP] 視線方向を復元(遠クリップ=reverse-Z の 0 に向かう方向ベクトル)。
	///      UV は縮小解像度で取る - ピクセル中心が覆うブロックの中心に
	///      一致する。
	float2 uv = (float2(pixel) + 0.5) / float2(cloud_width, cloud_height);
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 far_clip = float4(ndc, 0.0, 1.0);
	float4 far_world = mul(far_clip, scene.inverse_view_projection_);
	float3 ray_direction = normalize(far_world.xyz / far_world.w - scene.camera_position_.xyz);
	float3 ray_origin = scene.camera_position_.xyz;

	/// [EN] The segment where the view ray intersects the cloud shell.
	///      Already clamped to the ground and to the march limit.
	/// [JP] 視線と雲シェルの交差区間。地面とマーチ上限でクランプ済み。
	float t_enter;
	float t_exit;
	if (!CloudLayerInterval(ray_origin, ray_direction, tuning, t_enter, t_exit))
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
	float3 light_direction = normalize(-light.directional_direction_);
	float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;

	/// [EN] Rainy weather pulls the albedo toward a dark gray.
	/// [JP] 雨天はアルベドを暗い灰色へ寄せる。
	float3 cloud_albedo = lerp(tuning.cloud_albedo_, tuning.cloud_albedo_ * 0.35, saturate(tuning.rain_));

	float curvature = CloudCurvature(ray_direction, tuning.planet_radius_);
	float layer_thickness = max(tuning.cloud_top_ - tuning.cloud_bottom_, 1.0);
	float cos_theta = dot(ray_direction, light_direction);

	/// [EN] The multiple-scattering phase stays constant for the whole ray,
	///      so it is built once here rather than carried into the march
	///      loop (recomputing HG 10 times per sample would be pure waste).
	/// [JP] 多重散乱の位相はレイ1本の間ずっと定数なので、ここで一度だけ
	///      作ってマーチのループには持ち込まない(サンプルごとに HG を
	///      10 回計算し直すのは丸ごと無駄になる)。
	CloudMultiScatter multi_scatter = CloudMultiScatterSetup(cos_theta, tuning);

	/// [EN] Dither for the start offset. Mixing in the frame number would
	///      give a different pattern every frame and make the grain crawl,
	///      so by default this uses spatial-only interleaved gradient noise.
	///      Raise temporal_jitter_ only when relying on TAA/DLSS to resolve
	///      it.
	/// [JP] 開始位置のディザ。フレーム番号を混ぜると毎フレーム別パターンに
	///      なって grain が這うので、既定では空間のみの interleaved
	///      gradient noise にする。TAA/DLSS で解決させたい場合だけ
	///      temporal_jitter_ を上げる。
	float jitter = frac(InterleavedGradientNoise(float2(pixel))
		+ tuning.temporal_jitter_ * float(tuning.frame_index_ & 63u) * 0.6180339887);

	/// [EN] The step length is measured against the cloud layer's own
	///      thickness and grows with distance (distant LOD). Simply dividing
	///      the segment length by the step count would make one step
	///      overshoot the noise's feature size for the tens-of-thousands-
	///      unit-long segment toward the horizon, reading as haze.
	/// [JP] ステップ長は雲層の厚みを基準に取り、距離に応じて伸ばす
	///      (遠景 LOD)。区間長をステップ数で割るだけだと、地平線側の区間が
	///      数万単位あるので 1 ステップがノイズの特徴サイズを飛び越えて
	///      モヤになる。
	const uint max_iterations = 256;

	float fine_step_base = layer_thickness / float(max(tuning.step_count_, 1u));
	const float coarse_step_scale = 8.0;

	/// [EN] Cutting off at the iteration cap alone would leave a visible
	///      seam in the sky, so a lower bound is given to the step length
	///      that guarantees the segment is always walked to the end -
	///      absorbing the failure mode as "gets coarser" instead of "cuts
	///      off".
	/// [JP] 反復回数の上限で打ち切ると空に境目が出るので、区間を必ず
	///      走り切れる下限をステップ長に与えて「打ち切り」ではなく
	///      「粗くなる」形で破綻を吸収する。
	float minimum_step = (t_exit - t_enter) / (float(max_iterations) * 1.5);

	float3 scattering = float3(0, 0, 0);
	float transmittance = 1.0;

	/// [EN] For aerial perspective, accumulate the weighted-average distance
	///      of the positions that contributed to scattering.
	/// [JP] 空気遠近用に、散乱に寄与した位置の加重平均距離を貯めておく。
	float weighted_distance = 0.0;
	float weight_total = 0.0;

	float t = t_enter;
	bool coarse = true;
	uint empty_run = 0;

	for (uint iteration = 0; iteration < max_iterations; iteration++)
	{
		if (t >= t_exit)
		{
			break;
		}

		float lod = 1.0 + t / max(tuning.lod_distance_, 1.0);
		float fine_step = max(fine_step_base * lod, minimum_step);
		float step_length = coarse ? fine_step * coarse_step_scale : fine_step;

		float sample_t = t + step_length * (coarse ? 0.5 : jitter);
		if (sample_t >= t_exit)
		{
			break;
		}

		float altitude = CloudAltitudeAt(ray_origin.y, ray_direction.y, curvature, sample_t);
		float3 sample_position = ray_origin + ray_direction * sample_t;

		/// [EN] The detail noise has no mip chain, so it is faded out with
		///      distance to suppress distant aliasing. Coarse steps don't
		///      read it at all.
		/// [JP] ディテールノイズは mip を持たないので、距離でフェード
		///      させて遠景のエイリアスを抑える。粗ステップではそもそも
		///      読まない。
		float detail_strength = coarse ? 0.0 : saturate(1.0 - (t - tuning.lod_distance_) / max(tuning.lod_distance_ * 3.0, 1.0));

		float density = CloudDensity(sample_position, altitude, tuning, scene.total_time_,
			shape_noise, detail_noise, sampler_linear_wrap, detail_strength);

		if (density <= 0.0)
		{
			/// [EN] A run of empty space switches back to coarse stepping
			///      to skip it.
			/// [JP] 空が続いたら粗ステップに戻して空間をスキップする。
			empty_run++;
			if (!coarse && empty_run > 8)
			{
				coarse = true;
				empty_run = 0;
			}
			t += step_length;
			continue;
		}

		empty_run = 0;

		if (coarse)
		{
			/// [EN] A coarse step that hit cloud switches to fine stepping
			///      WITHOUT advancing t, so the entry boundary isn't
			///      overshot.
			/// [JP] 粗ステップで雲に当たったら t を進めずに細ステップへ
			///      切り替え、境界を飛び越さないようにする。
			coarse = false;
			continue;
		}

		/// [EN] The lightmarch is this pass's single biggest cost (most of
		///      the fetches per sample). A sample where transmittance has
		///      already dropped from clouds in front of it contributes
		///      little to the final pixel, so its step count is halved
		///      there. The visible cost is a slightly coarser shadow on the
		///      far side of a cloud, which is mostly invisible anyway.
		/// [JP] ライトマーチはこのパス最大のコスト(1サンプルあたりの
		///      フェッチの大半)。手前の雲で既に透過率が落ちている
		///      サンプルは最終ピクセルへの寄与が小さいので、そこは
		///      ステップ数を半分に落とす。見た目は雲の奥側の影が少し
		///      粗くなるだけで、そこは元々ほとんど見えない。
		uint light_steps = transmittance > 0.3 ? tuning.light_step_count_ : max(tuning.light_step_count_ / 2u, 2u);

		float light_optical_depth = CloudLightOpticalDepth(sample_position, altitude, light_direction,
			tuning, scene.total_time_, shape_noise, detail_noise, sampler_linear_wrap,
			light_steps, jitter);

		float sample_extinction = density * step_length;

		float powder = CloudPowder(light_optical_depth, cos_theta, tuning.powder_strength_);
		float sun_light = CloudSunLight(light_optical_depth, powder, multi_scatter);

		/// [EN] Ambient term. Darker toward the cloud base and closer to the
		///      zenith color toward the top, lifting the cloud base that
		///      single scattering alone would crush to pure black.
		/// [JP] 環境光。雲底ほど暗く、雲頂ほど天頂色寄りにして、単一散乱
		///      だけでは真っ黒に潰れる雲底を持ち上げる。
		float height01 = saturate((altitude - tuning.cloud_bottom_) / layer_thickness);
		float ambient_occlusion = lerp(0.3, 1.0, height01);
		float3 sky_ambient = lerp(tuning.sky_horizon_color_, tuning.sky_zenith_color_, height01)
			* tuning.sky_brightness_ * tuning.ambient_strength_ * ambient_occlusion;

		float3 in_scattering = cloud_albedo * (sun_radiance * sun_light + sky_ambient);

		/// [EN] The analytic solution treating density and the light term as
		///      constant within the segment. Transmittance is dropped via
		///      exp, but if the scattering side were added as plain sigma*ds
		///      instead, sigma*ds exceeding 1 toward the horizon would blow
		///      the emitted amount up exponentially and blow out to white.
		/// [JP] 区間内で密度と光源項を一定と見なした解析解。透過率は exp
		///      で落としているのに散乱側を σds のまま足すと、σds が 1 を
		///      超える地平線側で発光量が指数的に過大になって白飛びする。
		float sample_transmittance = exp(-sample_extinction);
		float segment_weight = (1.0 - sample_transmittance) * transmittance;

		scattering += in_scattering * segment_weight;

		weighted_distance += sample_t * segment_weight;
		weight_total += segment_weight;

		transmittance *= sample_transmittance;

		if (transmittance < 0.005)
		{
			break;
		}

		t += step_length;
	}

	float alpha = 1.0 - transmittance;

	if (alpha > 0.0)
	{
		float mean_distance = weight_total > 0.0 ? weighted_distance / weight_total : t_enter;

		/// [EN] Aerial perspective. The farther the cloud, the more it is
		///      pulled toward the horizon color, lowering saturation and
		///      contrast. Without this, distant clouds would render as dense
		///      as near ones and the sky would read as a flat cutout.
		/// [JP] 空気遠近。遠い雲ほど地平線色へ寄せて彩度とコントラストを
		///      落とす。これが無いと遠景の雲も近景と同じ濃さで出て、空が
		///      平らな貼り絵に見える。
		float aerial = exp(-mean_distance * tuning.aerial_density_);
		float3 horizon = tuning.sky_horizon_color_ * tuning.sky_brightness_;
		scattering = lerp(horizon * alpha, scattering, aerial);

		/// [EN] The cloud layer ends at the march limit, so alpha must be
		///      faded toward it - otherwise it shows up as a hard line
		///      cutting across the sky.
		/// [JP] 雲層はマーチ上限で終わるので、そこへ向けて alpha を
		///      フェードさせないと空を横切る硬い線になる。
		float fade_start = tuning.max_march_distance_ * 0.6;
		float distance_fade = 1.0 - smoothstep(fade_start, tuning.max_march_distance_, mean_distance);

		alpha *= distance_fade;
		scattering *= distance_fade;
	}

	output[pixel] = float4(scattering, alpha);
}
