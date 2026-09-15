#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

// Amplification Shader for the shell-fur forward pass.
//
// Dispatched as fur_instance_count_ * FUR_SHELL_MAX groups. Group gid.x maps to
// fur instance (gid.x / FUR_SHELL_MAX) and shell layer (gid.x % FUR_SHELL_MAX).
// Shell 0 is the base surface (already drawn opaque), so only shells 1..
// fur_shell_count_-1 emit geometry here. The fur instances live in the shared
// instance buffer at GetFurConstantBuffer().fur_instance_offset_.
//
// Per-meshlet LOD + frustum culling mirrors ModelAS.hlsl (occlusion culling is
// skipped, same as the transparent AS).

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared FurShellPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint fur_local = gid.x / FUR_SHELL_MAX;
	uint shell_index = gid.x % FUR_SHELL_MAX;
	uint instance_id = GetFurConstantBuffer().fur_instance_offset_ + fur_local;
	ModelStructuredBuffer instance = instances[instance_id];

	bool shell_active = shell_index >= 1 && shell_index < instance.shading_.fur_shell_count_ && instance.skining_.skin_index_ == 0xFFFFFFFF;

	uint meshlet_local = gtid.x;
	bool is_visible = false;

	if (shell_active && meshlet_local < instance.geometry_.meshlet_count_)
	{
		float world_scale = max(max(length(instance.transform_.world_[0].xyz), length(instance.transform_.world_[1].xyz)), length(instance.transform_.world_[2].xyz));
		if (IsLodSelected(instance.streaming_.lod_error_, instance.streaming_.lod_error_next_, instance.transform_.world_[3].xyz, world_scale, scene.camera_position_.xyz, scene.projection_._m11, scene.screen_size_.y, 1.0))
		{
			StructuredBuffer<ModelMeshletBound> bounds = ResourceDescriptorHeap[instance.geometry_.meshlet_bound_buffer_index_];
			uint meshlet_global = instance.geometry_.meshlet_offset_ + meshlet_local;
			ModelMeshletBound bound = bounds[meshlet_global];

			float3 world_center = mul(float4(bound.center_, 1.0), instance.transform_.world_).xyz;
			// widen the bound by the fur length so shells near the silhouette are not culled
			float world_radius = (bound.radius_ + instance.shading_.fur_length_) * world_scale;

			is_visible = IsVisibleInFrustum(world_center, world_radius, scene.current_view_projection_);
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
		payload.shell_index = shell_index;
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.meshlet_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
