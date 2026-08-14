#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Noise.hlsli"
#include "VolumetricCloudScapes.hlsli"

/**
* [EN]
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
* [JP]
* ボリューメトリック・クラウドスケープ(スクリーン空間レイマーチ、compute)。
* 視線を【曲率つきの雲シェル】(planet_radius_ の球殻。無限平板ではないので
* 遠方で下がって地平線に収束する)に沿ってマーチし、【ベイク済み】タイル可能
* Texture3D(Perlin-Worley 形状 128^3 + Worley ディテール 64^3、
* CloudNoiseShape/DetailBakeCS.hlsl が一度だけ焼く)をサンプルする。形状
* ボリュームの低周波スライスを weather map として使い、空に大きな雲の塊と
* 晴れ間を作る。ステップは適応的 — 空(密度0)は粗ステップで飛ばし、雲の中だけ
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

	/// [JP] このパスは【縮小解像度】で走る(VolumetricCloudScapesRenderer の
	///      resolutionDivisor_)。境界判定もレイ方向も画面サイズではなく出力
	///      テクスチャ自身のサイズを基準にする。
	uint cloud_width;
	uint cloud_height;
	output.GetDimensions(cloud_width, cloud_height);

	if (dtid.x >= cloud_width || dtid.y >= cloud_height)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	// シーンジオメトリがあるピクセルは雲(遠景)より必ず手前なのでスキップ。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];

	/// [JP] 深度は【フル解像度】なので、この1ピクセルが覆うブロック全体を見る。
	///      1つでも空があれば雲を計算する — 「4つとも空」を条件にすると、
	///      合成側でバイリニア補間したときにジオメトリのシルエット際で 0 を
	///      拾って暗いハローが出る。
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

	// 視線方向を復元(遠クリップ=reverse-Z の 0 に向かう方向ベクトル)。
	// UV は縮小解像度で取る — ピクセル中心が覆うブロックの中心に一致する。
	float2 uv = (float2(pixel) + 0.5) / float2(cloud_width, cloud_height);
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 far_clip = float4(ndc, 0.0, 1.0);
	float4 far_world = mul(far_clip, scene.inverse_view_projection_);
	float3 ray_direction = normalize(far_world.xyz / far_world.w - scene.camera_position_.xyz);
	float3 ray_origin = scene.camera_position_.xyz;

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

	// 雨天はアルベドを暗い灰色へ寄せる。
	float3 cloud_albedo = lerp(tuning.cloud_albedo_, tuning.cloud_albedo_ * 0.35, saturate(tuning.rain_));

	float curvature = CloudCurvature(ray_direction, tuning.planet_radius_);
	float layer_thickness = max(tuning.cloud_top_ - tuning.cloud_bottom_, 1.0);
	float cos_theta = dot(ray_direction, light_direction);

	/// [JP] 多重散乱の位相はレイ1本の間ずっと定数なので、ここで一度だけ作って
	///      マーチのループには持ち込まない(サンプルごとに HG を 10 回計算し
	///      直すのは丸ごと無駄になる)。
	CloudMultiScatter multi_scatter = CloudMultiScatterSetup(cos_theta, tuning);

	/// [JP] 開始位置のディザ。フレーム番号を混ぜると毎フレーム別パターンになって
	///      grain が這うので、既定では空間のみの interleaved gradient noise に
	///      する。TAA/DLSS で解決させたい場合だけ temporal_jitter_ を上げる。
	float jitter = frac(InterleavedGradientNoise(float2(pixel))
		+ tuning.temporal_jitter_ * float(tuning.frame_index_ & 63u) * 0.6180339887);

	/// [JP] ステップ長は雲層の厚みを基準に取り、距離に応じて伸ばす(遠景 LOD)。
	///      区間長をステップ数で割るだけだと、地平線側の区間が数万単位あるので
	///      1 ステップがノイズの特徴サイズを飛び越えてモヤになる。
	const uint max_iterations = 256;

	float fine_step_base = layer_thickness / float(max(tuning.step_count_, 1u));
	const float coarse_step_scale = 8.0;

	/// [JP] 反復回数の上限で打ち切ると空に境目が出るので、区間を必ず走り切れる
	///      下限をステップ長に与えて「打ち切り」ではなく「粗くなる」形で破綻を
	///      吸収する。
	float minimum_step = (t_exit - t_enter) / (float(max_iterations) * 1.5);

	float3 scattering = float3(0, 0, 0);
	float transmittance = 1.0;

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

		/// [JP] ディテールノイズは mip を持たないので、距離でフェードさせて
		///      遠景のエイリアスを抑える。粗ステップではそもそも読まない。
		float detail_strength = coarse ? 0.0 : saturate(1.0 - (t - tuning.lod_distance_) / max(tuning.lod_distance_ * 3.0, 1.0));

		float density = CloudDensity(sample_position, altitude, tuning, scene.total_time_,
			shape_noise, detail_noise, sampler_linear_wrap, detail_strength);

		if (density <= 0.0)
		{
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
			/// [JP] 粗ステップで雲に当たったら t を進めずに細ステップへ切り替え、
			///      境界を飛び越さないようにする。
			coarse = false;
			continue;
		}

		/// [JP] ライトマーチはこのパス最大のコスト(1サンプルあたりのフェッチの
		///      大半)。手前の雲で既に透過率が落ちているサンプルは最終ピクセルへの
		///      寄与が小さいので、そこはステップ数を半分に落とす。見た目は雲の
		///      奥側の影が少し粗くなるだけで、そこは元々ほとんど見えない。
		uint light_steps = transmittance > 0.3 ? tuning.light_step_count_ : max(tuning.light_step_count_ / 2u, 2u);

		float light_optical_depth = CloudLightOpticalDepth(sample_position, altitude, light_direction,
			tuning, scene.total_time_, shape_noise, detail_noise, sampler_linear_wrap,
			light_steps, jitter);

		float sample_extinction = density * step_length;

		float powder = CloudPowder(light_optical_depth, cos_theta, tuning.powder_strength_);
		float sun_light = CloudSunLight(light_optical_depth, powder, multi_scatter);

		/// [JP] 環境光。雲底ほど暗く、雲頂ほど天頂色寄りにして、単一散乱だけでは
		///      真っ黒に潰れる雲底を持ち上げる。
		float height01 = saturate((altitude - tuning.cloud_bottom_) / layer_thickness);
		float ambient_occlusion = lerp(0.3, 1.0, height01);
		float3 sky_ambient = lerp(tuning.sky_horizon_color_, tuning.sky_zenith_color_, height01)
			* tuning.sky_brightness_ * tuning.ambient_strength_ * ambient_occlusion;

		float3 in_scattering = cloud_albedo * (sun_radiance * sun_light + sky_ambient);

		/// [JP] 区間内で密度と光源項を一定と見なした解析解。透過率は exp で
		///      落としているのに散乱側を σds のまま足すと、σds が 1 を超える
		///      地平線側で発光量が指数的に過大になって白飛びする。
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

		/// [JP] 空気遠近。遠い雲ほど地平線色へ寄せて彩度とコントラストを落とす。
		///      これが無いと遠景の雲も近景と同じ濃さで出て、空が平らな貼り絵に
		///      見える。
		float aerial = exp(-mean_distance * tuning.aerial_density_);
		float3 horizon = tuning.sky_horizon_color_ * tuning.sky_brightness_;
		scattering = lerp(horizon * alpha, scattering, aerial);

		/// [JP] 雲層はマーチ上限で終わるので、そこへ向けて alpha をフェードさせ
		///      ないと空を横切る硬い線になる。
		float fade_start = tuning.max_march_distance_ * 0.6;
		float distance_fade = 1.0 - smoothstep(fade_start, tuning.max_march_distance_, mean_distance);

		alpha *= distance_fade;
		scattering *= distance_fade;
	}

	output[pixel] = float4(scattering, alpha);
}
