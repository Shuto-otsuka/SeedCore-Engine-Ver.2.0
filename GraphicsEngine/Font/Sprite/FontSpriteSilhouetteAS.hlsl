#include "../Font.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared FontASPayload payload;

/**
* [JP]
* シルエット用 Amplification Shader。FontSpriteAS.hlsl のコピー
* （編集時は同期を保つこと）に selected != 0 のインスタンスだけを通す
* フィルタを追加したもの。
*/
[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<FontSpriteStructuredBuffer> font_sprite = GetFontSpriteStructuredBuffer(shader_resource_indices.font_.sprite_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint glyph_id = dtid.x;

	if (glyph_id < 65536)
	{
		FontSpriteStructuredBuffer glyph = font_sprite[glyph_id];

		if (glyph.selected_ != 0 && glyph.size_.x > 0.0 && glyph.size_.y > 0.0)
		{
			is_visible = IsVisibleInScreen(glyph.position_, glyph.size_, scene_constant.display_size_);
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = glyph_id;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.glyph_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
