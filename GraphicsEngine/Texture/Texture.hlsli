#ifndef __TEXTURE_HLSL__
#define __TEXTURE_HLSL__

struct TextureASPayload
{
	uint texture_indices[32];
};

struct TextureMSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 color    : COLOR0;
	nointerpolation uint texture_index : BLENDINDICES;
};

struct TextureSpriteStructuredBuffer
{
	float2 position_;
	float rotation_;
	float2 scale_;
	float texture_sprite_structured_buffer_padding_0_;
	float2 texture_size_;
	float2 texture_position_;
	float2 pivot_;
	float2 texture_sprite_structured_buffer_padding_1_;
	float4 color_;
	uint texture_index_;
	float scroll_speed_;
	float2 scroll_direction_;
	uint motion_type_;
	uint selected_;
	float3 texture_sprite_structured_buffer_padding_2_;
};

StructuredBuffer<TextureSpriteStructuredBuffer> GetTextureSpriteStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct TextureBillboardStructuredBuffer
{
	float3 position_;
	float3 rotation_;
	float2 scale_;
	float2 texture_size_;
	float2 texture_position_;
	float2 pivot_;
	float2 texture_billboard_structured_buffer_padding_0_;
	float4 color_;
	uint texture_index_;
	float scroll_speed_;
	float2 scroll_direction_;
	uint motion_type_;
	uint face_camera_;
	uint selected_;
	float2 texture_billboard_structured_buffer_padding_1_;
};

StructuredBuffer<TextureBillboardStructuredBuffer> GetTextureBillboardStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct TextureShaderResourceIndices
{
	uint sprite_index_;
	uint billboard_index_;
	uint2 texture_shader_resource_padding_0_;
};

#endif // __TEXTURE_HLSL__
