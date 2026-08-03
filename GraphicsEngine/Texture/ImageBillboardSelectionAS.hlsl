#include "Image.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared ImageASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<ImageBillboardInstance> image_billboard = ResourceDescriptorHeap[structured_indices.sprite_.image_billboard_index_];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint billboard_id = dtid.x;

	if (billboard_id < 32768)
	{
		ImageBillboardInstance billboard = image_billboard[billboard_id];

		if (billboard.selected != 0 && billboard.scale.x > 0.0 && billboard.scale.y > 0.0)
		{
			float radius = max(billboard.scale.x, billboard.scale.y) * 0.5f;
			is_visible = IsVisibleInFrustum(billboard.position, radius, scene_constant.current_view_projection_);
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = billboard_id;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.image_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
