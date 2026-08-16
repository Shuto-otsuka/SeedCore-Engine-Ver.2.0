#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Sky/SkyMath.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../../Model/Model.hlsli"
#include "../Reflection/Reflection.hlsli"
#include "Refraction.hlsli"

/**
* [EN]
* Ray-traced refraction (RTPSO / DispatchRays, raygeneration only - no miss /
* closesthit exports). Only pixels whose KHR_materials_transmission factor is
* > 0 (read via VisID -> ModelInstance, same as Model/Opaque/DeferredLightingPS.hlsl)
* trace at all; everything else writes a=0 (invalid) so DeferredLightingPS.hlsl
* skips them. Bends the view ray through the surface with Snell's law (HLSL's
* refract(), eta = 1/ior entering the medium), then walks the bounce chain
* with an inline RayQuery loop inside raygeneration - NOT a recursive TraceRay
* from a closesthit shader. Every other RTPSO in this engine
* (Reflection/GlobalIllumination) deliberately keeps maxTraceRecursionDepth_
* at 1 and reaches for inline RayQuery whenever a hit shader needs another
* ray (see GlobalIlluminationShader.h's header comment); recursive TraceRay
* from closesthit has no precedent here and reliably failed RTPSO creation on
* this hardware/driver, so the bounce loop is inlined into raygeneration
* instead to match the engine's only proven pattern. Each interface applies
* Beer-Lambert absorption over the distance just traveled (KHR_materials_volume)
* and either continues refracting or - on total internal reflection, where
* refract() degenerates to the zero vector - reflects instead, until the ray
* misses (sky) or the bounce budget runs out.
*
* Reuses Reflection.hlsli's per-triangle instance/material tables wholesale
* (ReflectionInstanceData / ResolveReflectionMaterial) rather than duplicating
* them - same TLAS, same instance order, and ReflectionMaterialData already
* carries ior_/transmission_factor_/volume_attenuation_*_ alongside the
* base_color_ Reflection itself uses.
*
* Deliberately does NOT also trace a reflected ray at each interface (that
* would double the ray count per bounce) - the mirror-like reflection off the
* refractive surface's own front face is already provided independently by
* Raytracing/Reflection/ReflectionRT.hlsl (dielectric F0 from the same ior),
* and DeferredLightingPS.hlsl composites both by Fresnel weight.
*
* ---------------------------------------------------------------------
*
* [JP]
* レイトレ屈折(RTPSO / DispatchRays、raygenerationのみ - miss/closesthit
* エクスポート無し)。KHR_materials_transmission の factor が 0 より大きい
* ピクセルだけ(VisID→ModelInstance経由で判定、Model/Opaque/DeferredLightingPS.hlsl
* と同じ)レイを撃つ。それ以外は a=0(無効)を書き、DeferredLightingPS.hlsl
* 側でスキップされる。視線を Snell の法則(HLSL の refract()、媒質へ入る
* eta = 1/ior)で屈折させ、そこからバウンス連鎖を raygeneration 内の
* インライン RayQuery ループで辿る - closesthit からの再帰 TraceRay では
* ない。このエンジンの他の RTPSO(Reflection/GlobalIllumination)はどちらも
* maxTraceRecursionDepth_ を 1 に固定し、ヒットシェーダがもう1本レイを
* 要るときは常にインライン RayQuery に頼っている
* (GlobalIlluminationShader.h 冒頭コメント参照)。closesthit からの再帰
* TraceRay はこのエンジンに前例が無く、実機でRTPSO作成が確実に失敗した
* ため、エンジンで実績のある唯一のパターンに合わせてバウンスループを
* raygeneration 内へインライン化した。各界面で直前に進んだ距離ぶんの
* Beer-Lambert吸収(KHR_materials_volume)を適用し、屈折を続けるか、
* 全反射(refract() がゼロベクトルに縮退する)なら反射に切り替えながら、
* レイが外れる(空)かバウンス予算が尽きるまで辿る。
*
* Reflection.hlsli の三角形単位インスタンス/マテリアルテーブル
* (ReflectionInstanceData / ResolveReflectionMaterial)をそのまま再利用する
* (複製しない) - 同じTLAS、同じインスタンス順序であり、
* ReflectionMaterialData には Reflection 自身が使う base_color_ と並んで
* ior_/transmission_factor_/volume_attenuation_*_ も既に乗っている。
*
* 各界面で反射レイもあえて撃たない(バウンスごとにレイ数が倍増するため) -
* 屈折面自体の鏡面反射は Raytracing/Reflection/ReflectionRT.hlsl(同じiorから
* 求めた誘電体F0)が独立して提供しており、Model/Opaque/DeferredLightingPS.hlsl が
* Fresnel重みで両者を合成する。
*/

// Must match RefractionShader::maxBounces_ (Raytracing/Refraction/RefractionShader.h).
// Purely a loop bound now (no longer tied to maxTraceRecursionDepth_, which
// stays 1 - see the header comment above).
#define REFRACTION_MAX_BOUNCES 4

float3 SampleRefractionSky(float3 direction)
{
	if (structured_indices.sky_.environment_cube_index_ != 0)
	{
		return SampleSkyboxEnvironment(direction).rgb;
	}

	ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];
	if (cloud_tuning.procedural_sky_enabled_ != 0 && structured_indices.sky_.specular_prefiltered_index_ != 0)
	{
		TextureCube<float4> prefiltered = ResourceDescriptorHeap[structured_indices.sky_.specular_prefiltered_index_];
		return prefiltered.SampleLevel(sampler_linear_clamp, direction, 0).rgb;
	}
	else if (cloud_tuning.procedural_sky_enabled_ != 0)
	{
		ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
		float3 sun_direction = normalize(-light.directional_direction_);
		float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;
		return ProceduralSkyColor(direction, sun_direction, sun_radiance, cloud_tuning);
	}

	return float3(0, 0, 0);
}

[shader("raygeneration")]
void RefractionRayGeneration()
{
	uint2 pixel = DispatchRaysIndex().xy;
	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.refraction_.output_uav_index_];

	// 背景(reverse-Z 遠平面=0)は屈折なし。a=0 で「無効」を示す。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));
	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	// VisID から instance_index を引いて ModelInstance を直接読み、
	// KHR_materials_transmission が無いピクセルはここで抜ける
	// (Model/Opaque/DeferredLightingPS.hlsl と同じ配線 - Model/Material/MaterialResolveCS.hlsl
	// 参照)。
	Texture2D<uint2> visibility_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint2 visibility_id = visibility_texture.Load(int3(pixel, 0));
	uint material_instance_index, material_meshlet_index, material_triangle_index;
	UnpackVisibilityID(visibility_id, material_instance_index, material_meshlet_index, material_triangle_index);
	StructuredBuffer<ModelInstance> material_instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance material_instance = material_instances[material_instance_index];

	if (material_instance.transmission_factor_ <= 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<RefractionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.refraction_.ray_constant_index_];

	// ワールド座標と法線を復元(ReflectionRT.hlsl と同じ手順)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float3 normal = OctNormalDecode(normal_texture.Load(int3(pixel, 0)).rg);

	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);

	// Snell の法則で空気(ior=1)から媒質(instance.ior_)へ屈折させる。
	// HLSL の refract(i, n, eta) は全反射時にゼロベクトルを返す - 入射角が
	// 臨界角を超えるほぼ真横からの視線でしか普通は起きないが、保険として
	// ミラー反射へフォールバックする。
	float eta = 1.0 / max(material_instance.ior_, 1.0001);
	float3 ray_direction = refract(view_direction, normal, eta);
	if (dot(ray_direction, ray_direction) < 0.0001)
	{
		ray_direction = reflect(view_direction, normal);
	}
	ray_direction = normalize(ray_direction);

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	float3 ray_origin = world_position + ray_direction * tuning.normal_bias_;
	float3 throughput = float3(1, 1, 1);
	float3 radiance = float3(0, 0, 0);

	// バウンス連鎖はここでインライン RayQuery ループとして辿る
	// (closesthit からの再帰 TraceRay は使わない - ファイル冒頭コメント参照)。
	for (uint bounce = 0; bounce < REFRACTION_MAX_BOUNCES; bounce++)
	{
		RayDesc ray_desc;
		ray_desc.Origin = ray_origin;
		ray_desc.Direction = ray_direction;
		ray_desc.TMin = 0.001;
		ray_desc.TMax = tuning.ray_t_max_;

		RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
		query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray_desc);
		query.Proceed();

		if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
		{
			// レイが媒質を抜けて空へ出た。ここまでの throughput_(Beer-Lambert
			// 吸収と界面ロスの累積)を空のサンプルへ掛けて最終値にする。
			radiance = throughput * SampleRefractionSky(ray_direction);
			break;
		}

		StructuredBuffer<ReflectionInstanceData> instances = ResourceDescriptorHeap[structured_indices.raytracing_.instance_data_index_];
		ReflectionInstanceData hit_instance = instances[query.CommittedInstanceID()];

		StructuredBuffer<uint> triangle_indices = ResourceDescriptorHeap[hit_instance.index_buffer_index_];
		StructuredBuffer<ReflectionVertex> vertices = ResourceDescriptorHeap[hit_instance.vertex_buffer_index_];

		uint primitive_index = query.CommittedPrimitiveIndex();
		uint base_index = primitive_index * 3;
		ReflectionVertex vertex0 = vertices[triangle_indices[base_index + 0]];
		ReflectionVertex vertex1 = vertices[triangle_indices[base_index + 1]];
		ReflectionVertex vertex2 = vertices[triangle_indices[base_index + 2]];

		float2 barycentrics = query.CommittedTriangleBarycentrics();
		float weight0 = 1.0 - barycentrics.x - barycentrics.y;
		float weight1 = barycentrics.x;
		float weight2 = barycentrics.y;

		float3 object_normal =
			DecodeReflectionVertexNormal(vertex0) * weight0 +
			DecodeReflectionVertexNormal(vertex1) * weight1 +
			DecodeReflectionVertexNormal(vertex2) * weight2;

		// v1近似: 非一様スケールの厳密な逆転置は省略(ReflectionRT.hlsl と同じ)。
		float3 hit_world_normal = normalize(mul((float3x3)query.CommittedObjectToWorld3x4(), object_normal));

		float hit_t = query.CommittedRayT();
		float3 hit_position = ray_origin + ray_direction * hit_t;

		ReflectionMaterialData material = ResolveReflectionMaterial(hit_instance, primitive_index);

		// Beer-Lambert吸収: 直前の区間(hit_t)をこの三角形の媒質の
		// attenuation_color_/attenuation_distance_ で減衰させる。
		// attenuation_distance_ が既定の FLT_MAX(=吸収なし)ならほぼ 1 のまま。
		float3 optical_density = -log(max(material.volume_attenuation_color_, 0.0001)) / max(material.volume_attenuation_distance_, 0.0001);
		float3 absorption = exp(-optical_density * hit_t);
		throughput *= absorption;

		if (bounce + 1 >= REFRACTION_MAX_BOUNCES)
		{
			// バウンス予算切れ - 打ち切り(吸収済みとして黒扱い)。
			radiance = float3(0, 0, 0);
			break;
		}

		// 入射方向はこの三角形にちょうど飛び込んできたレイの方向。ここが
		// 媒質からの「出口」だと仮定し(単純な凸形状のガラス1枚を想定するv1近似)、
		// 空気(ior=1)へ戻る屈折を試みる。全反射なら媒質内で反射を続ける。
		float3 incoming_direction = ray_direction;
		float3 next_direction = refract(incoming_direction, hit_world_normal, material.ior_);
		if (dot(next_direction, next_direction) < 0.0001)
		{
			next_direction = reflect(incoming_direction, hit_world_normal);
		}
		next_direction = normalize(next_direction);

		ray_origin = hit_position + next_direction * tuning.normal_bias_;
		ray_direction = next_direction;
	}

	output[pixel] = float4(radiance * tuning.strength_, 1.0);
}
