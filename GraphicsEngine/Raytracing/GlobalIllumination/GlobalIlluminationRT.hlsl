#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "GlobalIllumination.hlsli"

/**
* [EN]
* Reference:
* - https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
*   (Ouyang et al., "ReSTIR GI: Path Resampling for Real-Time Path Tracing",
*   HPG 2021 - the ReSTIR reservoir reuse this raygen and the following
*   spatial pass are built on; see GlobalIlluminationReSTIR.hlsli for why the
*   canonical receiver-side target function/Jacobian from that paper are NOT
*   used here.)
* - https://intro-to-restir.cwyman.org/presentations/2023ReSTIR_Course_Notes.pdf
*   (Wyman et al., ReSTIR course notes - background on RIS/target-function
*   weighting shared by every ReSTIR variant in this engine.)
*
* Ray-traced diffuse global illumination, one bounce (RTPSO / DispatchRays:
* raygen + miss + closesthit). Fires ONE cosine-weighted hemisphere ray per
* pixel per frame from the G-Buffer surface: miss returns sky radiance,
* closesthit relights the hit surface (sun + shadow ray + base color texture +
* diffuse IBL) so the bounce carries colour bleeding rather than just sky
* visibility. raygen writes the incoming radiance; the ALBEDO of the receiving
* surface is applied in DeferredLightingPS.hlsl.
*
* ---------------------------------------------------------------------
*
* [JP]
* レイトレ拡散グローバルイルミネーション、1バウンス(RTPSO / DispatchRays:
* raygen + miss + closesthit)。G-Buffer の面から、1ピクセル1フレームにつき
* コサイン重み付き半球レイを1本撃つ。miss は空の放射輝度を返し、closesthit は
* ヒット面を再ライティング(太陽+影レイ+ベースカラーテクスチャ+拡散IBL)する
* ので、単なる空可視性ではなくカラーブリーディングになる。raygen は入射
* 放射輝度を書き、受け側の面のアルベドは DeferredLightingPS.hlsl で掛ける。
*/

[shader("raygeneration")]
void GlobalIlluminationRayGeneration()
{
	uint2 pixel = DispatchRaysIndex().xy;
	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.global_illumination_.output_uav_index_];

	/// [EN] Background (reverse-Z far plane = 0) has no indirect light. a=0
	///      becomes the "invalid" marker downstream.
	/// [JP] 背景(reverse-Z 遠平面=0)は間接光なし。a=0 で「無効」を示す。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<GlobalIlluminationRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.global_illumination_.ray_constant_index_];

	/// [EN] Reconstruct world position and normal (same procedure as
	///      ShadowRT.hlsl, mul takes the row vector on the left).
	/// [JP] ワールド座標と法線を復元する(ShadowRT.hlsl と同じ手順、mul は
	///      行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	/// [EN] RNG seed that changes every frame. A constant offset is mixed
	///      into frame_index_ so this pass's random sequence does not line
	///      up with ShadowRT/AO's in the same frame (lining up would
	///      correlate the two passes' noise into visible banding).
	/// [JP] 毎フレーム変わる種。同フレームの ShadowRT / AO と乱数列が
	///      揃わないよう定数オフセットを混ぜる(揃うとノイズが相関して
	///      縞に見える)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 15486071u);

	float3 ray_direction = CosineSampleHemisphere(rng_state, normal);

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	RayDesc ray_desc;
	ray_desc.Origin = world_position + normal * tuning.normal_bias_;
	ray_desc.Direction = ray_direction;
	ray_desc.TMin = 0.001;
	ray_desc.TMax = tuning.ray_t_max_;

	GlobalIlluminationPayload payload;
	payload.radiance_ = float3(0, 0, 0);
	payload.hit_distance_ = 0.0;

	/// [EN] Relighting the closest-hit surface needs it, so no
	///      first-hit-only flag is set.
	/// [JP] 最近接ヒットの面を再ライティングするので first-hit 打ち切りは
	///      付けない。
	TraceRay(tlas, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray_desc, payload);

	/// [EN] Monte Carlo estimator normalization.
	///      The diffuse rendering equation is Lo = integral (albedo/PI) * Li
	///      * cos dw. The pdf of cosine-weighted hemisphere sampling is
	///      cos/PI, so the single-sample estimate is
	///      (albedo/PI * Li * cos) / (cos/PI) = albedo * Li - the cos and
	///      1/PI cancel exactly against the pdf, so NO explicit weighting is
	///      needed here at all. The albedo belongs to the RECEIVING surface,
	///      so it is applied on the composite side instead - which is why
	///      raygen can write the incoming radiance Li as-is.
	/// [JP] モンテカルロ推定量の正規化について。
	///      拡散の反射方程式は Lo = ∫ (albedo/PI) * Li * cos dω。
	///      コサイン重み付き半球サンプリングの pdf は cos/PI なので、
	///      1サンプル推定量は (albedo/PI * Li * cos) / (cos/PI) =
	///      albedo * Li。つまり cos と 1/PI が pdf と完全に約分され、
	///      【重み付けは何も要らない】。アルベドは受け側の面のものなので
	///      コンポジット側で掛ける。ここが入射放射輝度そのものを書いてよい
	///      理由。
	float3 candidate_radiance = payload.radiance_;
	float3 candidate_position = world_position + ray_direction * payload.hit_distance_;

	GlobalIlluminationReservoir reservoir = GlobalIlluminationReservoirFromSample(candidate_position, normal, candidate_radiance);

	StructuredBuffer<GlobalIlluminationReservoir> reservoir_history = ResourceDescriptorHeap[constant_indices.global_illumination_.reservoir_history_srv_index_];

	/// [EN] ReSTIR temporal reuse. Velocity-buffer reprojection follows the
	///      same procedure as GlobalIlluminationDenoiseCS.hlsl's main()
	///      (UV-space displacement is (velocity.x, -velocity.y)).
	/// [JP] ReSTIR 時間的リユース。速度バッファでの再投影は
	///      GlobalIlluminationDenoiseCS.hlsl の main() と同じ手順(UV空間の
	///      移動量は (velocity.x, -velocity.y))。
	Texture2D<float2> velocity_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
	float2 velocity = velocity_texture.Load(int3(pixel, 0)).rg;
	float2 previous_uv = uv - float2(velocity.x, -velocity.y);

	bool temporal_valid = all(previous_uv >= 0.0) && all(previous_uv <= 1.0);

	if (temporal_valid)
	{
		uint2 previous_pixel = uint2(previous_uv * scene.screen_size_);
		uint previous_index = previous_pixel.y * (uint)scene.screen_size_.x + previous_pixel.x;

		GlobalIlluminationReservoir temporal = reservoir_history[previous_index];
		temporal.sample_m_ = min(temporal.sample_m_, GI_RESERVOIR_M_CAP);

		reservoir = GlobalIlluminationReservoirCombine(reservoir, temporal, temporal_valid, rng_state);
	}

	RWStructuredBuffer<GlobalIlluminationReservoir> reservoir_write = ResourceDescriptorHeap[constant_indices.global_illumination_.reservoir_uav_index_];
	reservoir_write[pixel.y * (uint)scene.screen_size_.x + pixel.x] = reservoir;

	output[pixel] = float4(reservoir.sample_radiance_ * reservoir.sample_w_ * tuning.intensity_, 1.0);
}

[shader("miss")]
void GlobalIlluminationMiss(inout GlobalIlluminationPayload payload)
{
	/// [EN] The ray escaped to the sky. Sample the environment cube if a
	///      skymap is bound, otherwise the procedural sky (if enabled),
	///      otherwise black. GI is low-frequency, so unlike a mirror
	///      reflection, the already-blurred convolved cube is good enough.
	/// [JP] レイが空へ抜けた。スカイマップがあれば環境キューブを、無ければ
	///      プロシージャル空(有効時)を、それも無ければ黒をサンプルする。
	///      GI は低周波なので、鏡面反射と違ってボケた畳み込み済みキューブで
	///      十分。
	if (structured_indices.sky_.environment_cube_index_ != 0)
	{
		payload.radiance_ = SampleSkyboxEnvironment(WorldRayDirection()).rgb;
	}
	else
	{
		ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];

		if (cloud_tuning.procedural_sky_enabled_ != 0 && structured_indices.sky_.specular_prefiltered_index_ != 0)
		{
			TextureCube<float4> prefiltered = ResourceDescriptorHeap[structured_indices.sky_.specular_prefiltered_index_];
			payload.radiance_ = prefiltered.SampleLevel(sampler_linear_clamp, WorldRayDirection(), 0).rgb;
		}
		else if (cloud_tuning.procedural_sky_enabled_ != 0)
		{
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
void GlobalIlluminationAnyHit(inout GlobalIlluminationPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	if (IsReflectionMaterialPassthrough(structured_indices.raytracing_.instance_data_index_, InstanceID(), PrimitiveIndex(), attributes.barycentrics))
	{
		IgnoreHit();
	}
}

[shader("closesthit")]
void GlobalIlluminationClosestHit(inout GlobalIlluminationPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	/// [EN] Look up the hit mesh's vertex/index SRVs through the instance
	///      table (shared with the reflection pass), and interpolate the
	///      normal and UV from barycentrics.
	/// [JP] インスタンステーブル(反射パスと共有)からヒットメッシュの
	///      頂点/インデックス SRV を引き、barycentrics で法線と UV を
	///      補間する。
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
	///      non-uniform scale is skipped.
	/// [JP] オブジェクト空間→ワールド空間。非一様スケールの厳密な逆転置は
	///      省略。
	float3 world_normal = normalize(mul((float3x3)ObjectToWorld3x4(), object_normal));

	/// [EN] Flip the normal to face the incoming ray. GI rays fly toward any
	///      direction in the hemisphere, so they can hit a thin panel or a
	///      back face. Using the raw vertex normal as-is would flip N.L's
	///      sign and create a surface that is "lit brightly from behind",
	///      showing up as an unnatural bright smear in otherwise dark areas.
	/// [JP] 面の向きを視線側に合わせる。GI レイは半球の任意方向へ飛ぶので、
	///      薄い板や裏面に当たることがある。そのまま頂点法線を使うと N・L
	///      が反転して「裏から照らされているのに明るい」面ができ、暗部に
	///      不自然な明るい染みとして出る。
	if (dot(world_normal, WorldRayDirection()) > 0.0)
	{
		world_normal = -world_normal;
	}

	float3 hit_position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];

	float3 lighting = float3(0, 0, 0);

	/// [EN] Diffuse IBL (0 if no skymap). The irradiance cube's convolution
	///      already folds in 1/PI by convention, so a plain multiply is
	///      correct here (same as ImageBasedLightingRadianceLambertian).
	///      sky_intensity_ is NOT applied to GI on the composite side, so
	///      unlike the reflection pass, it is applied here instead.
	/// [JP] 拡散IBL(スカイマップが無ければ0)。irradiance キューブは
	///      畳み込み時に 1/PI 込みの規約なので素の乗算でよい
	///      (ImageBasedLightingRadianceLambertian と同じ)。sky_intensity_
	///      はコンポジット側では GI に掛からないので、反射パスと違いここで
	///      掛ける。
	if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
	{
		TextureCube<float4> diffuse_irradiance = ResourceDescriptorHeap[structured_indices.sky_.diffuse_irradiance_index_];
		lighting += diffuse_irradiance.SampleLevel(sampler_linear_clamp, world_normal, 0).rgb * structured_indices.sky_.intensity_;
	}

	if (light.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light.directional_direction_);
		float normal_dot_light = saturate(dot(world_normal, light_direction));

		/// [EN] Shadow for the bounce surface. Without this, indirect light
		///      would leak in from shadowed walls too, making interiors
		///      look uniformly lifted. closesthit cannot call TraceRay
		///      (maxTraceRecursionDepth_ = 1), but inline RayQuery is not
		///      subject to the recursion-depth limit, so a shadow can be
		///      traced without touching the RTPSO.
		/// [JP] バウンス面の影。これが無いと日陰の壁からも間接光が飛んで
		///      きて、室内が一様に持ち上がって見える。closesthit から
		///      TraceRay は撃てない(maxTraceRecursionDepth_ = 1)が、
		///      インライン RayQuery は再帰深度の制限対象外なので RTPSO を
		///      触らずに影を撃てる。
		float sun_visibility = 1.0;

		if (normal_dot_light > 0.0)
		{
			ConstantBuffer<GlobalIlluminationRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.global_illumination_.ray_constant_index_];
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

		/// [EN] The 1/PI of the Lambert BRDF. The engine's direct-light
		///      convention (BrdfLambertian) returns albedo/PI, so without
		///      dividing by PI here, only the bounce surface would be PI
		///      times too bright (the same trap encountered in the
		///      reflection pass).
		/// [JP] ランバート BRDF の 1/PI。エンジンの直接光は BrdfLambertian
		///      が albedo/PI を返す規約なので、ここで割らないとバウンス面
		///      だけ PI 倍明るくなる(反射パスで踏んだのと同じ罠)。
		const float lambert_normalization = 1.0 / 3.14159265358979;
		lighting += light.directional_color_.rgb * light.directional_intensity_ * normal_dot_light * sun_visibility * lambert_normalization;
	}

	/// [EN] Point/Spot/Rect lights, sharing the same
	///      ComputeClusteredPunctualLighting the reflection pass uses (no
	///      shadow ray per light).
	/// [JP] Point/Spot/Rect ライト。反射パスと同じ
	///      ComputeClusteredPunctualLighting を共有する(影レイは撃たない)。
	lighting += ComputeClusteredPunctualLighting(hit_position, world_normal, scene, light);

	/// [EN] Resolve the material of the submesh the hit triangle belongs to
	///      (not a single color for the whole mesh - this was the direct
	///      cause of a bug: a mesh with multiple materials, e.g. a Cornell
	///      box built with per-wall colors as one Crister, always returned
	///      the FIRST material's color regardless of which wall - red or
	///      green - was actually hit, so color bleeding was structurally
	///      impossible to see).
	/// [JP] ヒット三角形が属するサブメッシュのマテリアルを解決する(メッシュ
	///      全体で単一色にしない — これが直接の原因だった: 複数マテリアルの
	///      メッシュ(壁ごとに色が違う Cornell box を1つの Crister で作った
	///      場合など)では常に先頭マテリアルの色しか返らず、赤壁/緑壁の
	///      どちらに当たっても同じ色になっていたため、色滲みが原理的に
	///      一切出ていなかった)。
	ReflectionMaterial material = ResolveReflectionMaterial(instance, PrimitiveIndex());

	/// [EN] Base color. Multiplying the albedo in here is the whole
	///      mechanism of "color bleeding" - this one line is why light
	///      bounced off a red wall turns red.
	/// [JP] ベースカラー。ここでアルベドを掛けることが「色が飛ぶ」= カラー
	///      ブリーディングの本体。赤い壁で跳ねた光が赤くなるのはこの一行。
	float3 albedo = material.base_color_;

	if (material.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> base_color_texture = ResourceDescriptorHeap[material.base_color_texture_index_];
		albedo *= base_color_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	payload.radiance_ = albedo * lighting;
	payload.hit_distance_ = RayTCurrent();
}
