#include "../Texture.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared TextureASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<TextureSpriteStructuredBuffer> texture_sprite = GetTextureSpriteStructuredBuffer(shader_resource_indices.texture_.sprite_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint sprite_id = dtid.x;

	if (sprite_id < 32768)
	{
		TextureSpriteStructuredBuffer sprite = texture_sprite[sprite_id];

		if (sprite.selected_ != 0 && sprite.scale_.x > 0.0 && sprite.scale_.y > 0.0)
		{
			is_visible = IsVisibleInScreen(sprite.position_, sprite.texture_size_ * sprite.scale_, scene_constant.screen_size_);
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = sprite_id;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.texture_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
