#include "Movie.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared MovieASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<MovieSpriteStructuredBuffer> movie_sprite = GetMovieSpriteStructuredBuffer(shader_resource_indices.movie_.sprite_index_);
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
		MovieSpriteStructuredBuffer instance = movie_sprite[instance_id];

		if (instance.size_.x > 0.0 && instance.size_.y > 0.0)
		{
			is_visible = IsVisibleInScreen(instance.position_, instance.size_, scene_constant.screen_size_);
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
