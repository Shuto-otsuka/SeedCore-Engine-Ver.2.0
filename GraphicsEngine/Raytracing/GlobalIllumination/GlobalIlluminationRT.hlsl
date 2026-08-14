#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "GlobalIllumination.hlsli"

/**
* [EN]
* Ray-traced diffuse global illumination, one bounce (RTPSO / DispatchRays:
* raygen + miss + closesthit). Fires ONE cosine-weighted hemisphere ray per
* pixel per frame from the G-Buffer surface: miss returns sky radiance,
* closesthit relights the hit surface (sun + shadow ray + base color texture +
* diffuse IBL) so the bounce carries colour bleeding rather than just sky
* visibility. raygen writes the incoming radiance; the ALBEDO of the receiving
* surface is applied in DeferredLightingPS.hlsl.
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

	// 背景(reverse-Z 遠平面=0)は間接光なし。a=0 で「無効」を示す。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<GlobalIlluminationRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.global_illumination_.ray_constant_index_];

	// ワールド座標と法線を復元(ShadowRT.hlsl と同じ手順、mul は行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	/// [JP] 毎フレーム変わる種。同フレームの ShadowRT / AO と乱数列が揃わない
	///      よう定数オフセットを混ぜる(揃うとノイズが相関して縞に見える)。
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

	// 最近接ヒットの面を再ライティングするので first-hit 打ち切りは付けない。
	TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray_desc, payload);

	/// [JP] モンテカルロ推定量の正規化について。
	///      拡散の反射方程式は Lo = ∫ (albedo/PI) * Li * cos dω。
	///      コサイン重み付き半球サンプリングの pdf は cos/PI なので、
	///      1サンプル推定量は (albedo/PI * Li * cos) / (cos/PI) = albedo * Li。
	///      つまり cos と 1/PI が pdf と完全に約分され、【重み付けは何も要らない】。
	///      アルベドは受け側の面のものなのでコンポジット側で掛ける。ここが
	///      入射放射輝度そのものを書いてよい理由。
	output[pixel] = float4(payload.radiance_ * tuning.intensity_, 1.0);
}

[shader("miss")]
void GlobalIlluminationMiss(inout GlobalIlluminationPayload payload)
{
	// レイが空へ抜けた。スカイマップがあれば環境キューブを、無ければ
	// プロシージャル空(有効時)を、それも無ければ黒をサンプルする。
	// GI は低周波なので、鏡面反射と違ってボケた畳み込み済みキューブで十分。
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

[shader("closesthit")]
void GlobalIlluminationClosestHit(inout GlobalIlluminationPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	// インスタンステーブル(反射パスと共有)からヒットメッシュの頂点/インデックス
	// SRV を引き、barycentrics で法線と UV を補間する。
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

	// オブジェクト空間→ワールド空間。非一様スケールの厳密な逆転置は省略。
	float3 world_normal = normalize(mul((float3x3)ObjectToWorld3x4(), object_normal));

	/// [JP] 面の向きを視線側に合わせる。GI レイは半球の任意方向へ飛ぶので、
	///      薄い板や裏面に当たることがある。そのまま頂点法線を使うと N・L が
	///      反転して「裏から照らされているのに明るい」面ができ、暗部に
	///      不自然な明るい染みとして出る。
	if (dot(world_normal, WorldRayDirection()) > 0.0)
	{
		world_normal = -world_normal;
	}

	float3 hit_position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];

	float3 lighting = float3(0, 0, 0);

	// 拡散IBL(無ければフラットな環境光)。irradiance キューブは畳み込み時に
	// 1/PI 込みの規約なので素の乗算でよい(ImageBasedLightingRadianceLambertian
	// と同じ)。sky_intensity_ はコンポジット側では GI に掛からないので、
	// 反射パスと違いここで掛ける。
	if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
	{
		TextureCube<float4> diffuse_irradiance = ResourceDescriptorHeap[structured_indices.sky_.diffuse_irradiance_index_];
		lighting += diffuse_irradiance.SampleLevel(sampler_linear_clamp, world_normal, 0).rgb * structured_indices.sky_.intensity_;
	}
	else
	{
		lighting += 0.03;
	}

	if (light.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light.directional_direction_);
		float normal_dot_light = saturate(dot(world_normal, light_direction));

		// バウンス面の影。これが無いと日陰の壁からも間接光が飛んできて、
		// 室内が一様に持ち上がって見える。closesthit から TraceRay は撃てない
		// (maxTraceRecursionDepth_ = 1)が、インライン RayQuery は再帰深度の
		// 制限対象外なので RTPSO を触らずに影を撃てる。
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

			RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> shadow_query;
			shadow_query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, shadow_ray);
			shadow_query.Proceed();

			if (shadow_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
			{
				sun_visibility = 0.0;
			}
		}

		// ランバート BRDF の 1/PI。エンジンの直接光は BrdfLambertian が
		// albedo/PI を返す規約なので、ここで割らないとバウンス面だけ PI 倍
		// 明るくなる(反射パスで踏んだのと同じ罠)。
		const float lambert_normalization = 1.0 / 3.14159265358979;
		lighting += light.directional_color_.rgb * light.directional_intensity_ * normal_dot_light * sun_visibility * lambert_normalization;
	}

	// Point/Spot/Rect ライト。反射パスと同じ ComputeClusteredPunctualLighting
	// を共有する(影レイは撃たない)。
	lighting += ComputeClusteredPunctualLighting(hit_position, world_normal, scene, light);

	// ヒット三角形が属するサブメッシュのマテリアルを解決する(メッシュ全体で
	// 単一色にしない — これが直接の原因だった: 複数マテリアルのメッシュ
	// (壁ごとに色が違う Cornell box を1つの Crister で作った場合など)では
	// 常に先頭マテリアルの色しか返らず、赤壁/緑壁のどちらに当たっても同じ色
	// になっていたため、色滲みが原理的に一切出ていなかった)。
	ReflectionMaterialData material = ResolveReflectionMaterial(instance, PrimitiveIndex());

	// ベースカラー。ここでアルベドを掛けることが「色が飛ぶ」= カラー
	// ブリーディングの本体。赤い壁で跳ねた光が赤くなるのはこの一行。
	float3 albedo = material.base_color_;

	if (material.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> base_color_texture = ResourceDescriptorHeap[material.base_color_texture_index_];
		albedo *= base_color_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	payload.radiance_ = albedo * lighting;
	payload.hit_distance_ = RayTCurrent();
}
