#ifndef __FONT_HLSL__
#define __FONT_HLSL__

struct FontASPayload
{
	uint glyph_indices[32];
};

struct FontMSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 color    : COLOR0;
	nointerpolation float4 outline_color : COLOR1;
	nointerpolation float4 glow_color    : COLOR2;
	nointerpolation uint texture_index   : BLENDINDICES;
	nointerpolation float2 unit_range    : TEXCOORD1;
	nointerpolation float outline_width  : TEXCOORD2;
	nointerpolation float glow_power     : TEXCOORD3;
};

struct FontSpriteStructuredBuffer
{
	float2 position_;
	float2 size_;
	float2 uv_min_;
	float2 uv_max_;
	float4 color_;
	float4 outline_color_;
	float4 glow_color_;
	uint texture_index_;
	float outline_width_;
	float glow_power_;
	float font_sprite_structured_buffer_padding_1_;
	float2 unit_range_;
	uint selected_;
	float font_sprite_structured_buffer_padding_2_;
};

StructuredBuffer<FontSpriteStructuredBuffer> GetFontSpriteStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct FontBillboardStructuredBuffer
{
	float3 position_;
	float font_billboard_structured_buffer_padding_0_;
	float3 rotation_;
	float font_billboard_structured_buffer_padding_1_;
	float2 local_position_;
	float2 local_size_;
	float2 uv_min_;
	float2 uv_max_;
	float4 color_;
	float4 outline_color_;
	float4 glow_color_;
	uint texture_index_;
	float outline_width_;
	float glow_power_;
	float font_billboard_structured_buffer_padding_2_;
	float2 unit_range_;
	uint face_camera_;
	uint selected_;
};

StructuredBuffer<FontBillboardStructuredBuffer> GetFontBillboardStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct FontShaderResourceIndices
{
	uint sprite_index_;
	uint billboard_index_;
	uint2 font_shader_resource_padding_0_;
};

float MtsdfMedian(float3 value)
{
	return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float MtsdfScreenPxRange(float2 unit_range, float2 uv)
{
	float2 screen_texel_size = 1.0f / max(fwidth(uv), 0.00001f);
	return max(0.5f * dot(unit_range, screen_texel_size), 1.0f);
}

float4 MtsdfCompose(float4 mtsdf, FontMSOutput input)
{
	float screen_px_range = MtsdfScreenPxRange(input.unit_range, input.uv);

	float signed_distance = (MtsdfMedian(mtsdf.rgb) - 0.5f) * screen_px_range;
	float soft_distance = (mtsdf.a - 0.5f) * screen_px_range;

	float fill_alpha = saturate(signed_distance + 0.5f);

	float outline_alpha = saturate(signed_distance + input.outline_width + 0.5f);

	float3 rgb = lerp(input.outline_color.rgb, input.color.rgb, fill_alpha);
	float alpha = lerp(outline_alpha * input.outline_color.a, fill_alpha * input.color.a, fill_alpha);

	float glow_falloff = saturate(soft_distance / max(screen_px_range, 1.0f) + 0.5f);
	float glow_alpha = pow(glow_falloff, 2.0f) * input.glow_power * input.glow_color.a;

	float final_alpha = alpha + glow_alpha * (1.0f - alpha);
	float3 final_rgb = (rgb * alpha + input.glow_color.rgb * glow_alpha * (1.0f - alpha)) / max(final_alpha, 0.0001f);

	return float4(final_rgb, final_alpha);
}

#endif // __FONT_HLSL__
