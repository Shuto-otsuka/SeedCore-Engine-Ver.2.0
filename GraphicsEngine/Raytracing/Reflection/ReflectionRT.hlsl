#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Sky/SkyMath.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../../Shader/Denoiser.hlsli"
#include "Reflection.hlsli"
#include "ReflectionReSTIR.hlsli"

/**
* [EN]
* Reference:
* - https://jcgt.org/published/0007/04/01/paper.pdf
*   (Heitz, "Sampling the GGX Distribution of Visible Normals", JCGT 2018 -
*   the visible-normal-distribution sampling this raygen uses instead of the
*   older full-NDF ImportanceSampleGgx.)
* - https://arxiv.org/pdf/2306.05044
*   (Dupuy & Benyoub, "Sampling Visible GGX Normals with Spherical Caps",
*   HPG 2023 - the exact spherical-cap construction SampleGgxVisibleNormal
*   implements, SkyMath.hlsli.)
* - https://cs.dartmouth.edu/~wjarosz/publications/bitterli20spatiotemporal.html
*   (Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray
*   tracing with dynamic direct lighting", SIGGRAPH 2020 - the reservoir
*   reuse this raygen and the following spatial pass are built on.)
*
* Ray-traced glossy reflection (RTPSO / DispatchRays, raygen + miss +
* closesthit). Reflects the view ray off the G-Buffer surface and traces ONE
* ray per pixel per frame, its half vector drawn from the GGX distribution of
* VISIBLE normals (SkyMath.hlsli's SampleGgxVisibleNormal) rather than the
* full NDF - only microfacets the viewer can actually see are sampled, so the
* single-sample estimator needs no pdf division or cosine weight: its whole
* weight is the height-correlated Smith masking-shadowing ratio G2/G1
* (SkyMath.hlsli's SmithGgxG2OverG1), applied to the traced radiance below.
* A sample landing below the horizon (G2/G1 == 0, which VNDF sampling makes
* rare but not impossible near grazing angles) is simply not traced and
* contributes nothing - unlike the old full-NDF scheme, there is no need to
* redirect it to the mirror direction as a biased fallback. At roughness 0
* the half-vector always equals the normal, so this degenerates to the exact
* mirror ray with no noise. Cost was previously kept down by tracing 4 such
* rays and averaging them per pixel per frame; it is now kept down instead by
* a ReSTIR reservoir (ReflectionReSTIR.hlsli) that streams this frame's
* single candidate against the reprojected temporal history in the same
* dispatch, then against a few spatial neighbors in a following pass
* (ReflectionReservoirSpatialCS.hlsl) - trading the four redundant traces for
* reuse of already-traced neighboring work, the same trade
* GlobalIlluminationRT.hlsl already makes for GI. The resolved reservoir's
* hit distance is written into the output alpha (not discarded): the
* denoiser's dual reprojection (surface motion + hit-point virtual motion)
* reprojects a virtual position built by extending this hit distance along
* the surface's specular dominant direction. miss samples the environment
* cube; closesthit re-fetches the hit triangle's vertices through the
* per-instance table (InstanceID() -> ReflectionInstanceData) and relights
* the hit point with the directional/punctual lights + diffuse IBL.
*
* ---------------------------------------------------------------------
*
* [JP]
* レイトレ光沢反射(RTPSO / DispatchRays、raygen + miss + closesthit)。
* G-Buffer の面で視線を反射し、1ピクセル1フレームにつきレイを【1本】撃つ。
* ハーフベクトルは GGX の【可視】法線分布(SkyMath.hlsli の
* SampleGgxVisibleNormal)からサンプルする - 分布全体ではなく、視線から
* 実際に見えているマイクロファセットだけをサンプルするので、単一サンプル
* 推定量に pdf 除算もコサイン重みも要らない: 重みの全ては高さ相関する
* Smith のマスキング/シャドウイング比 G2/G1(SkyMath.hlsli の
* SmithGgxG2OverG1)だけで、下でトレース済み放射輝度に掛ける。地平線の下に
* 落ちたサンプル(G2/G1 == 0、VNDF サンプリングでは稀だがグレージング角
* 付近ではあり得る)はそもそもトレースせず寄与ゼロとする - 旧来の分布全体
* サンプリングと違い、これをミラー方向へ差し替えるバイアスありの
* フォールバックは要らない。roughness 0 では常にハーフベクトルが法線と
* 一致するため、ノイズ無しの厳密ミラーレイへ縮退する。以前はレイを4本
* トレースして平均することでコストを抑えていたが、今は ReSTIR reservoir
* (ReflectionReSTIR.hlsli)が今フレームの1候補をリプロジェクション済みの
* 時間的履歴と同じディスパッチ内でストリーム結合し、続くパス
* (ReflectionReservoirSpatialCS.hlsl)で近傍数点とも結合することでコストを
* 抑える - 冗長な4本トレースの代わりに、既にトレース済みの近傍の結果を
* 再利用する。GlobalIlluminationRT.hlsl が GI で行っているのと同じ
* トレードオフ。解決した reservoir のヒット距離は出力アルファへ書く(捨て
* ない) - デノイザの二重リプロジェクション(面モーション+ヒット点仮想
* モーション)がこのヒット距離を面のスペキュラ支配方向へ延長した仮想位置を
* リプロジェクションする。miss は環境キューブをサンプル。closesthit は
* インスタンステーブル(InstanceID() → ReflectionInstanceData)経由で
* ヒット三角形の頂点を引き直し、ディレクショナル/ポイント/スポット/矩形
* ライト+拡散IBLでヒット点を再ライティングする。
*/

/// radiance(12) + hit_distance(4) = 16 bytes, fits the default
/// maxPayloadSizeInBytes (16).
struct ReflectionPayload
{
	float3 radiance_;
	float hit_distance_;
};

[shader("raygeneration")]
void ReflectionRayGeneration()
{
	uint2 pixel = DispatchRaysIndex().xy;
	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.reflection_.output_uav_index_];

	/// [EN] Background (reverse-Z far plane = 0) has no reflection. a=0
	///      (zero hit distance) becomes the "nothing traced" marker
	///      downstream.
	/// [JP] 背景(reverse-Z 遠平面=0)は反射なし。a=0(ヒット距離ゼロ)が
	///      下流での「何もトレースしていない」印になる。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];

	/// [EN] Reconstruct world position and normal (same procedure as
	///      ShadowRT.hlsl, mul takes the row vector on the left).
	/// [JP] ワールド座標と法線を復元する(ShadowRT.hlsl と同じ手順、mul は
	///      行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	/// [EN] gbuffer1: rg = oct-encoded normal, b = roughness (same packing
	///      as DeferredLightingPS.hlsl).
	/// [JP] gbuffer1: rg = octエンコード法線、b = ラフネス
	///      (DeferredLightingPS.hlsl と同じ詰め方)。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 gbuffer1 = normal_texture.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(gbuffer1.rg);
	float roughness = gbuffer1.b;

	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);
	float3 mirror_direction = normalize(reflect(view_direction, normal));

	/// [EN] Sample a half vector from the GGX visible-normal distribution
	///      and reflect the view around it (same reflect formula the IBL
	///      convolution uses: L = 2*(V.H)*H - V, V here being the
	///      surface->camera direction). At roughness 0,
	///      SampleGgxVisibleNormal always returns H=normal, so this matches
	///      mirror_direction exactly (no noise).
	/// [JP] GGX 可視法線分布からハーフベクトルをサンプルし、視線をそのまわり
	///      へ反射させる(ImageBasedLighting の畳み込みと同じ式: L =
	///      2*(V.H)*H - V、V はここでは surface→camera 方向)。roughness 0
	///      では SampleGgxVisibleNormal が常に H=normal を返すので
	///      mirror_direction と厳密に一致する(ノイズ無し)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 2654435761u);
	float3 view = -view_direction;

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	float3 half_vector = SampleGgxVisibleNormal(Rand2(rng_state), normal, view, roughness);
	float3 reflect_direction = normalize(2.0 * dot(view, half_vector) * half_vector - view);

	/// [EN] The single-sample estimator's weight for a VNDF sample is just
	///      G2/G1 (no pdf division, no cosine weight needed - both cancel
	///      exactly because the sample was drawn from the visible-normal
	///      distribution itself). A sample that lands below the horizon
	///      makes this 0, so no biased fallback to the mirror direction is
	///      needed - it is simply skipped below. Fresnel is applied on the
	///      composite side as the surface's specular_color.
	/// [JP] VNDF サンプルの単一サンプル推定量の重みは G2/G1 だけ(pdf 除算も
	///      コサイン重みも要らない — 可視法線分布そのものからサンプルして
	///      いるため、両者が厳密に約分される)。地平線下へ落ちたサンプルは
	///      これが0になるので、ミラー方向へのバイアスありフォールバックは
	///      不要 - 下で単にスキップする。Fresnel は合成側で面の
	///      specular_color として掛ける。
	float normal_dot_view = saturate(dot(normal, view));
	float normal_dot_light = dot(normal, reflect_direction);
	float sample_weight = SmithGgxG2OverG1(normal_dot_view, normal_dot_light, roughness);

	ReflectionPayload payload;
	payload.radiance_ = float3(0, 0, 0);
	payload.hit_distance_ = 0.0;

	if (sample_weight > 0.0)
	{
		RayDesc ray_desc;
		ray_desc.Origin = world_position + normal * tuning.normal_bias_;
		ray_desc.Direction = reflect_direction;
		ray_desc.TMin = 0.001;
		ray_desc.TMax = tuning.ray_t_max_;

		/// [EN] The closest hit is needed, so no first-hit-only flag is set.
		/// [JP] 最近接ヒットが要るので first-hit 打ち切りフラグは付けない。
		TraceRay(tlas, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray_desc, payload);

		payload.radiance_ *= sample_weight;
	}
	else
	{
		reflect_direction = mirror_direction;
	}

	/// [EN] Fold radiance to a finite value unconditionally. The output is
	///      RGBA16F, so radiance above 65504 (easy to reach with a bright
	///      specular or emissive reflection) would be stored as +Inf, which
	///      then produces NaN in the reservoir combine's luminance math or a
	///      later denoiser pass. Worse, that would be written back into the
	///      reservoir/history and persist rather than pass through, spreading
	///      through the HDR buffer into bloom/lens-flare.
	/// [JP] 放射輝度を必ず有限値へ畳む。出力先は RGBA16F なので、65504 を
	///      超える放射輝度(明るいスペキュラや発光面の反射なら容易に到達
	///      する)は +Inf として格納され、reservoir 結合の輝度計算や後段の
	///      デノイザで NaN が生まれる。しかもそれは reservoir/履歴として
	///      書き戻されるため通り過ぎず焼き付き、HDRバッファ経由でブルーム
	///      やレンズフレアへ拡散する。
	float3 candidate_radiance = DenoiserSanitizeRadiance(payload.radiance_);
	float candidate_hit_distance = (isnan(payload.hit_distance_) || isinf(payload.hit_distance_)) ? 0.0 : clamp(payload.hit_distance_, 0.0, 65504.0);

	ReflectionReservoir reservoir = ReflectionReservoirFromSample(reflect_direction, candidate_radiance, candidate_hit_distance);

	StructuredBuffer<ReflectionReservoir> reservoir_history = ResourceDescriptorHeap[constant_indices.reflection_.reservoir_history_srv_index_];

	/// [EN] ReSTIR temporal reuse. Velocity-buffer reprojection follows the
	///      same procedure as GI (UV-space displacement is
	///      (velocity.x, -velocity.y)). Reflection's own parallax problem
	///      (what's visible moves with the REFLECTED geometry, not the
	///      surface itself) is handled separately, not here, by
	///      ReflectionDenoiseCS.hlsl's later dual reprojection - the
	///      reservoir's own temporal combine only needs a rough hit from
	///      surface motion; a miss is diluted away by RIS weighting and
	///      spatial reuse anyway.
	/// [JP] ReSTIR 時間的リユース。速度バッファでの再投影は GI と同じ手順
	///      (UV空間の移動量は (velocity.x, -velocity.y))。反射特有の
	///      パララックス問題(映っている内容は面ではなく反射先のジオメトリ
	///      と動く)は、ここではなく後段の ReflectionDenoiseCS.hlsl の
	///      二重リプロジェクションが個別に扱う - reservoir 自体の時間的
	///      結合は面モーションでの粗い当たりで十分、外れても RIS の重み
	///      付けと空間的リユースで薄まる。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0)).rg;
	float2 previous_uv = uv - float2(velocity.x, -velocity.y);

	bool temporal_valid = all(previous_uv >= 0.0) && all(previous_uv <= 1.0);

	if (temporal_valid)
	{
		uint2 previous_pixel = uint2(previous_uv * scene.screen_size_);
		uint previous_index = previous_pixel.y * (uint)scene.screen_size_.x + previous_pixel.x;

		ReflectionReservoir temporal = reservoir_history[previous_index];
		temporal.sample_m_ = min(temporal.sample_m_, REFLECTION_RESERVOIR_M_CAP);

		reservoir = ReflectionReservoirCombine(reservoir, temporal, temporal_valid, rng_state);
	}

	RWStructuredBuffer<ReflectionReservoir> reservoir_write = ResourceDescriptorHeap[constant_indices.reflection_.reservoir_uav_index_];
	reservoir_write[pixel.y * (uint)scene.screen_size_.x + pixel.x] = reservoir;

	output[pixel] = float4(reservoir.sample_radiance_ * reservoir.sample_w_, reservoir.sample_hit_distance_);
}

[shader("miss")]
void ReflectionMiss(inout ReflectionPayload payload)
{
	/// [EN] The ray escaped to the sky. Sample the environment cube if a
	///      skymap is bound, otherwise the procedural sky (if enabled),
	///      otherwise black.
	/// [JP] レイが空へ抜けた。スカイマップがあれば環境キューブを、無ければ
	///      プロシージャル空(有効時)を、それも無ければ黒をサンプルする。
	if (structured_indices.sky_.environment_cube_index_ != 0)
	{
		payload.radiance_ = SampleSkyboxEnvironment(WorldRayDirection()).rgb;
	}
	else
	{
		ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];
		if (cloud_tuning.procedural_sky_enabled_ != 0 && structured_indices.sky_.specular_prefiltered_index_ != 0)
		{
			/// [EN] Procedural-sky mode: sample mip0 of the prefiltered cube
			///      that already has the clouds baked in
			///      (ProceduralSkyToCubeCS bakes then convolves it). That's
			///      roughly 128px, so even a mirror reflection is slightly
			///      soft, but having the clouds show up matters more.
			/// [JP] プロシージャル空モード: 雲まで焼き込み済みの prefilter
			///      キューブ(ProceduralSkyToCubeCS でベイク→畳み込み)の
			///      mip0 をサンプルする。128px 相当なので鏡面でもわずかに
			///      柔らかいが、雲が映る方が重要。
			TextureCube<float4> prefiltered = ResourceDescriptorHeap[structured_indices.sky_.specular_prefiltered_index_];
			payload.radiance_ = prefiltered.SampleLevel(sampler_linear_clamp, WorldRayDirection(), 0).rgb;
		}
		else if (cloud_tuning.procedural_sky_enabled_ != 0)
		{
			/// [EN] The bake has not completed yet in these first few
			///      frames, so fall back to the analytic sky + sun.
			/// [JP] ベイクがまだ済んでいない最初の数フレームは解析的な空+
			///      太陽で代用する。
			ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
			float3 sun_direction = normalize(-light.directional_direction_);
			float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;
			payload.radiance_ = ProceduralSkyColor(WorldRayDirection(), sun_direction, sun_radiance, cloud_tuning);
		}
		else
		{
			payload.radiance_ = float3(0, 0, 0);
		}
	}
	payload.hit_distance_ = 100000.0;
}

[shader("anyhit")]
void ReflectionAnyHit(inout ReflectionPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	if (IsReflectionMaterialPassthrough(structured_indices.raytracing_.instance_data_index_, InstanceID(), PrimitiveIndex(), attributes.barycentrics))
	{
		IgnoreHit();
	}
}

[shader("closesthit")]
void ReflectionClosestHit(inout ReflectionPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	/// [EN] Look up the hit mesh's vertex/index SRVs through the instance
	///      table, and interpolate the normal from barycentrics.
	/// [JP] インスタンステーブルからヒットメッシュの頂点/インデックスSRVを
	///      引き、barycentrics で法線を補間する。
	StructuredBuffer<ReflectionInstanceData> instances = ResourceDescriptorHeap[structured_indices.raytracing_.instance_data_index_];
	ReflectionInstanceData instance = instances[InstanceID()];

	StructuredBuffer<uint> triangle_indices = ResourceDescriptorHeap[instance.index_buffer_index_];
	StructuredBuffer<ReflectionVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];

	uint base_index = PrimitiveIndex() * 3;
	ReflectionVertex vertex0 = vertices[triangle_indices[base_index + 0]];
	ReflectionVertex vertex1 = vertices[triangle_indices[base_index + 1]];
	ReflectionVertex vertex2 = vertices[triangle_indices[base_index + 2]];

	float2 barycentrics = attributes.barycentrics;
	float weight0 = 1.0 - barycentrics.x - barycentrics.y;
	float weight1 = barycentrics.x;
	float weight2 = barycentrics.y;

	float3 object_normal =
		DecodeReflectionVertexNormal(vertex0) * weight0 +
		DecodeReflectionVertexNormal(vertex1) * weight1 +
		DecodeReflectionVertexNormal(vertex2) * weight2;

	float2 texcoord =
		DecodeReflectionVertexTexcoord(vertex0, instance.texcoord_min_, instance.texcoord_extent_) * weight0 +
		DecodeReflectionVertexTexcoord(vertex1, instance.texcoord_min_, instance.texcoord_extent_) * weight1 +
		DecodeReflectionVertexTexcoord(vertex2, instance.texcoord_min_, instance.texcoord_extent_) * weight2;

	/// [EN] Object space -> world space. The exact inverse-transpose for
	///      non-uniform scale is skipped (v1 approximation - correct for
	///      uniform scale/rotation/translation).
	/// [JP] オブジェクト空間→ワールド空間。非一様スケールの厳密な逆転置は
	///      省略(v1近似 - 一様スケール/回転/平行移動なら正しい)。
	float3 world_normal = normalize(mul((float3x3)ObjectToWorld3x4(), object_normal));

	float3 hit_position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	/// [EN] Relight the hit surface (simplified v1): directional light +
	///      Point/Spot/Rect Lambert term + diffuse IBL (0 if no skymap - see
	///      GraphicsEngine's AMBIENT-removal note in DeferredLightingPS.hlsl).
	///      Specular term, punctual-light shadows and textures other than
	///      base color are all skipped in this v1.
	/// [JP] ヒット面の再ライティング(簡易版): ディレクショナルライト+
	///      Point/Spot/Rect のランバート項 + 拡散IBL(スカイマップが無ければ
	///      0)。鏡面項・影(Point/Spot/Rect側)・ベースカラー以外のテクスチャ
	///      は省略のv1。
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];

	float3 lighting = float3(0, 0, 0);

	if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
	{
		/// [EN] Raytrace shaders have no implicit LOD (only SampleLevel is
		///      available), so this reads directly with SampleLevel instead
		///      of going through the helper
		///      (SampleDiffuseIrradianceEnvironmentMap) - equivalent, since
		///      the irradiance cube is only one mip.
		///
		///      sky_intensity_ is deliberately NOT multiplied in here.
		///      DeferredLightingPS.hlsl multiplies the whole lighting term
		///      by it once AFTER lerping traced reflection into ibl_specular
		///      - multiplying it here too would double-apply it (invisible
		///      at the default 1.0, but the moment sky brightness changes,
		///      reflection alone would respond quadratically). The
		///      irradiance cube's convolution already folds in 1/PI, so a
		///      plain multiply here is correct, same as
		///      ImageBasedLightingRadianceLambertian.
		/// [JP] レイトレシェーダでは暗黙LODの Sample が使えない
		///      (SampleLevel のみ)ため、ヘルパー
		///      (SampleDiffuseIrradianceEnvironmentMap)を経由せず直接
		///      SampleLevel で読む。irradiance キューブはミップ1枚なので
		///      等価。
		///
		///      sky_intensity_ はここでは掛けない。DeferredLightingPS.hlsl
		///      がトレース反射を ibl_specular へ lerp した後に lighting
		///      全体へ一度掛けるので、ここで掛けると二重適用になる(既定値
		///      1.0 では見えないが、空の明るさを変えた瞬間に反射だけ二乗で
		///      効く)。irradiance キューブは畳み込み時に 1/PI 込みの規約
		///      なので、ImageBasedLightingRadianceLambertian と同じく素の
		///      乗算で正しい。
		TextureCube<float4> diffuse_irradiance = ResourceDescriptorHeap[structured_indices.sky_.diffuse_irradiance_index_];
		lighting += diffuse_irradiance.SampleLevel(sampler_linear_clamp, world_normal, 0).rgb;
	}

	if (light.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light.directional_direction_);
		float normal_dot_light = saturate(dot(world_normal, light_direction));

		/// [EN] Shadow for the surface visible in the reflection.
		///      closesthit cannot call TraceRay
		///      (ReflectionShader.cpp's maxTraceRecursionDepth_ = 1), but
		///      inline RayQuery is not subject to the recursion-depth limit,
		///      so a shadow can be traced without touching the RTPSO config.
		///      Without this, every surface inside the reflection would
		///      appear fully lit, showing up as "only the reflection's
		///      lighting looks different" compared to the shadowed primary
		///      view.
		/// [JP] 反射に映る面の影。closesthit から TraceRay は撃てない
		///      (ReflectionShader.cpp の maxTraceRecursionDepth_ = 1)が、
		///      インライン RayQuery は再帰深度の制限対象外なので、RTPSO の
		///      設定を変えずに影を撃てる。これが無いと反射の中だけ全ての
		///      面が全照射になり、影のある直接描画との差が「反射だけ
		///      ライティングが違う」として出る。
		float sun_visibility = 1.0;

		if (normal_dot_light > 0.0)
		{
			ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];
			RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

			RayDesc shadow_ray;
			shadow_ray.Origin = hit_position + world_normal * tuning.normal_bias_;
			shadow_ray.Direction = light_direction;
			shadow_ray.TMin = 0.001;
			shadow_ray.TMax = tuning.ray_t_max_;

			if (IsReflectionRayOccluded(tlas, shadow_ray, structured_indices.raytracing_.instance_data_index_))
			{
				sun_visibility = 0.0;
			}
		}

		/// [EN] The 1/PI is required. The direct-light Lambert BRDF, per
		///      BidirectionalReflectanceDistributionFunction.hlsli's
		///      BrdfLambertian, returns albedo/PI by convention - without
		///      dividing by PI here, only the surface visible IN the
		///      reflection would receive PI times (~3.14x) too much direct
		///      light. At sun intensity 1.0 and albedo 1.0, the directly
		///      viewed surface settles at 0.318 while the reflected one
		///      clips to 1.0, blowing out the gradation into a flat,
		///      "lighting isn't working" color.
		/// [JP] 1/PI が必要。直接光側のランバート BRDF は
		///      BidirectionalReflectanceDistributionFunction.hlsli の
		///      BrdfLambertian が albedo/PI を返す規約なので、ここで
		///      割らないと【反射に映る面だけが直接光を PI 倍(約3.14倍)
		///      受ける】。太陽強度 1.0・アルベド 1.0 だと直接見た面は
		///      0.318 で収まるのに反射側は 1.0 に張り付くため、階調が
		///      飛んで「ライティングが効いていない単色」に見える。
		const float lambert_normalization = 1.0 / 3.14159265358979;
		lighting += light.directional_color_.rgb * light.directional_intensity_ * normal_dot_light * sun_visibility * lambert_normalization;
	}

	/// [EN] Point/Spot/Rect lights. Pulled from the same cluster ShadowRT.hlsl
	///      uses (including its N.L gate on the normal), but no shadow ray
	///      is traced for them (see Reflection.hlsli's
	///      ComputeClusteredPunctualLighting - tracing one per light would
	///      multiply the ray count by however many lights are in range).
	/// [JP] Point/Spot/Rect ライト。ShadowRT.hlsl と同じクラスタ(法線での
	///      N・L ゲート込み)から拾うが、影レイは撃たない(Reflection.hlsli
	///      の ComputeClusteredPunctualLighting 参照 - ライト数ぶんレイが
	///      増えるため)。
	lighting += ComputeClusteredPunctualLighting(hit_position, world_normal, scene, light);

	/// [EN] Resolve the material of the submesh the hit triangle belongs to
	///      (not a single color for the whole mesh - fixes a bug where a
	///      mesh with multiple materials, e.g. a Cornell box built as one
	///      Crister with a different color per wall, returned a color
	///      unrelated to the wall actually hit).
	/// [JP] ヒット三角形が属するサブメッシュのマテリアルを解決する(メッシュ
	///      全体で単一色にしない - 複数マテリアルのメッシュ(例: 壁ごとに
	///      色が違う Cornell box を1つの Crister で作った場合)で、実際に
	///      当たった壁と無関係な色が返っていたのを修正)。
	ReflectionMaterial material = ResolveReflectionMaterial(instance, PrimitiveIndex());

	/// [EN] Base color texture. Raytrace shaders have no implicit
	///      derivatives, so SampleLevel is fixed at mip0 (choosing a mip
	///      from the ray's footprint would be the proper approach, but a
	///      glossy reflection's footprint is narrow, so starting at mip0 is
	///      fine).
	/// [JP] ベースカラーテクスチャ。レイトレシェーダには暗黙の微分が
	///      無いので SampleLevel 固定(ミップ選択はレイのフットプリントから
	///      出すのが本式だが、鏡面反射のフットプリントは狭いので mip0 で
	///      始める)。
	float3 albedo = material.base_color_;

	if (material.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> base_color_texture = ResourceDescriptorHeap[material.base_color_texture_index_];
		albedo *= base_color_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	payload.radiance_ = albedo * lighting;
	payload.hit_distance_ = RayTCurrent();
}
