#include "Movie.hlsli"
#include "../Shader/Structured.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared MovieASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<MovieFullscreenInstance> movie_fullscreen = ResourceDescriptorHeap[structured_indices.movie_.fullscreen_index_];

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint instance_id = dtid.x;

	if (instance_id < 32)
	{
		MovieFullscreenInstance instance = movie_fullscreen[instance_id];
		is_visible = instance.texture_aspect > 0.0f;
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
