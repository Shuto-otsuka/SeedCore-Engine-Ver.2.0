#include "Font.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload FontASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices FontMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<FontSpriteInstance> font_sprite = ResourceDescriptorHeap[structured_indices.sprite_.font_index_];

	SetMeshOutputCounts(4u, 2u);

	uint glyph_id = payload.glyph_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	FontSpriteInstance glyph = font_sprite[glyph_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	if (gtid < 4u)
	{
		float2 world_pixel = glyph.position + corners[gtid] * glyph.size;

		float2 clip;
		clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
		clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

		output[gtid].position = float4(clip, 0.0f, 1.0f);
		output[gtid].uv = lerp(glyph.uv_min, glyph.uv_max, corners[gtid]);
		output[gtid].color = glyph.color;
		output[gtid].outline_color = glyph.outline_color;
		output[gtid].glow_color = glyph.glow_color;
		output[gtid].texture_index = glyph.texture_index;
		output[gtid].unit_range = glyph.unit_range;
		output[gtid].outline_width = glyph.outline_width;
		output[gtid].glow_power = glyph.glow_power;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
