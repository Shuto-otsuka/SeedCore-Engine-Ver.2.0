#include "../Font.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

FontMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<FontSpriteStructuredBuffer> font_sprite = GetFontSpriteStructuredBuffer(shader_resource_indices.font_.sprite_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	FontSpriteStructuredBuffer glyph = font_sprite[instance_id];

	FontMSOutput output = (FontMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	bool is_visible = false;
	if (glyph.selected_ != 0 && glyph.size_.x > 0.0 && glyph.size_.y > 0.0)
	{
		is_visible = IsVisibleInScreen(glyph.position_, glyph.size_, scene_constant.screen_size_);
	}
	if (!is_visible)
	{
		return output;
	}

	uint quad_indices[6] = { 0u, 1u, 2u, 1u, 3u, 2u };
	uint corner_index = quad_indices[vertex_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	float2 world_pixel = glyph.position_ + corners[corner_index] * glyph.size_;

	float2 clip;
	clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
	clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

	output.position = float4(clip, 0.0f, 1.0f);
	output.uv = lerp(glyph.uv_min_, glyph.uv_max_, corners[corner_index]);
	output.color = glyph.color_;
	output.outline_color = glyph.outline_color_;
	output.glow_color = glyph.glow_color_;
	output.texture_index = glyph.texture_index_;
	output.unit_range = glyph.unit_range_;
	output.outline_width = glyph.outline_width_;
	output.glow_power = glyph.glow_power_;

	return output;
}
