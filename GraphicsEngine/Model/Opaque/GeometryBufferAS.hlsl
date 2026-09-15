#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared ModelASPayload payload;

/**
* [EN]
* Amplification Shader for the opaque G-Buffer passes (Static & Skeletal).
*
* A copy of ModelAS.hlsl (keep both in sync!) with one addition: Hi-Z
* occlusion culling. The depth prepass draws with ModelAS and the Hi-Z
* pyramid is built from its result, so only the G-Buffer passes can use it.
* The LOD selection must stay identical to ModelAS so prepass depth and
* G-Buffer geometry agree.
*
* ---------------------------------------------------------------------
*
* [JP]
* 不透明 G-Buffer パス用（Static / Skeletal 共用）の Amplification Shader。
*
* ModelAS.hlsl のコピー（編集時は同期を保つこと！）に Hi-Z オクルージョン
* カリングを 1 点追加したもの。デプスプリパスは ModelAS で描画され、その
* 結果から Hi-Z ピラミッドが構築されるため、Hi-Z を使えるのは G-Buffer
* パスのみ。LOD 選択はプリパス深度と G-Buffer ジオメトリを一致させるため
* ModelAS と完全に同一であること。
*/
[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint instance_id = gid.x;
	ModelStructuredBuffer instance = instances[instance_id];
	StructuredBuffer<ModelMeshletBound> bounds = ResourceDescriptorHeap[instance.geometry_.meshlet_bound_buffer_index_];

	uint meshlet_local = gtid.x;
	bool is_visible = false;

	if (instance.shading_.blend_ == 0 && meshlet_local < instance.geometry_.meshlet_count_)
	{
		/// [EN] Distance-based LOD selection — identical to ModelAS.
		/// [JP] 距離ベース LOD 選択 — ModelAS と同一。
		float world_scale = max(max(length(instance.transform_.world_[0].xyz), length(instance.transform_.world_[1].xyz)), length(instance.transform_.world_[2].xyz));
		if (IsLodSelected(instance.streaming_.lod_error_, instance.streaming_.lod_error_next_, instance.transform_.world_[3].xyz, world_scale, scene.camera_position_.xyz, scene.projection_._m11, scene.screen_size_.y, 1.0))
		{
		/// [EN] Skinned instances skip bound-based culling (bounds are pre-skinning).
		/// [JP] スキンインスタンスはバウンドベースのカリングをスキップ（バウンドはスキン前）。
		if (instance.skining_.skin_index_ != 0xFFFFFFFF)
		{
			is_visible = true;
		}
		else
		{
		uint meshlet_global = instance.geometry_.meshlet_offset_ + meshlet_local;
		ModelMeshletBound bound = bounds[meshlet_global];

		float3 world_center = mul(float4(bound.center_, 1.0), instance.transform_.world_).xyz;
		float world_radius = bound.radius_ * max(max(length(instance.transform_.world_[0].xyz), length(instance.transform_.world_[1].xyz)), length(instance.transform_.world_[2].xyz));

		/// [EN] Frustum culling.
		/// [JP] フラスタムカリング。
		is_visible = IsVisibleInFrustum(world_center, world_radius, scene.current_view_projection_);

		/// [EN] Normal cone backface culling — single-sided materials only.
		/// [JP] 法線コーンバックフェイスカリング — 片面マテリアルのみ。
		if (is_visible && instance.shading_.double_sided_ == 0 && bound.cone_cutoff_ > 0.0)
		{
			float3 world_cone_axis = normalize(mul(float4(bound.cone_axis_, 0.0), instance.transform_.world_).xyz);
			float3 camera_to_center = world_center - scene.camera_position_.xyz;
			float cone_sine = sqrt(1.0 - bound.cone_cutoff_ * bound.cone_cutoff_);
			if (dot(camera_to_center, world_cone_axis) >= cone_sine * length(camera_to_center) + world_radius)
			{
				is_visible = false;
			}
		}

		/// [EN] Hi-Z occlusion culling against the depth-prepass pyramid.
		///      G-Buffer passes only — the prepass generates this data.
		/// [JP] デプスプリパスのピラミッドに対する Hi-Z オクルージョンカリング。
		///      G-Buffer パス専用 — プリパスがこのデータを生成する。
		if (is_visible)
		{
			is_visible = IsVisibleHiZ(world_center, world_radius, scene, shader_resource_indices.model_.hi_z_index_);
		}
		}
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = instance.geometry_.meshlet_offset_ + meshlet_local;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		payload.instance_index = instance_id;
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.meshlet_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
