#include "../Movie.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared MovieASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<MovieBillboardStructuredBuffer> movie_billboard = GetMovieBillboardStructuredBuffer(shader_resource_indices.movie_.billboard_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint instance_id = dtid.x;

	if (instance_id < 1024)
	{
		MovieBillboardStructuredBuffer instance = movie_billboard[instance_id];

		if (instance.scale_.x > 0.0 && instance.scale_.y > 0.0)
		{
			float radius = max(instance.scale_.x, instance.scale_.y) * 0.5f;
			is_visible = IsVisibleInFrustum(instance.position_, radius, scene_constant.current_view_projection_);
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = instance_id;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.instance_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
