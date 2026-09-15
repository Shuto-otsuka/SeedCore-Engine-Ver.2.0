#ifndef __IMAGE_HLSL__
#define __IMAGE_HLSL__

struct ImageASPayload
{
	uint image_indices[32];
};

struct ImageMSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float4 color    : COLOR0;
	nointerpolation uint texture_index : BLENDINDICES;
};

struct ImageSpriteStructuredBuffer
{
	float2 position_;
	float rotation_;
	float2 scale_;
	float image_sprite_structured_buffer_padding_0_;
	float2 texture_size_;
	float2 texture_position_;
	float2 pivot_;
	float2 image_sprite_structured_buffer_padding_1_;
	float4 color_;
	uint texture_index_;
	float scroll_speed_;
	float2 scroll_direction_;
	uint motion_type_;
	uint selected_;
	float3 image_sprite_structured_buffer_padding_2_;
};

StructuredBuffer<ImageSpriteStructuredBuffer> GetImageSpriteStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct ImageBillboardStructuredBuffer
{
	float3 position_;
	float3 rotation_;
	float2 scale_;
	float2 texture_size_;
	float2 texture_position_;
	float2 pivot_;
	float2 image_billboard_structured_buffer_padding_0_;
	float4 color_;
	uint texture_index_;
	float scroll_speed_;
	float2 scroll_direction_;
	uint motion_type_;
	uint face_camera_;
	uint selected_;
	float2 image_billboard_structured_buffer_padding_1_;
};

StructuredBuffer<ImageBillboardStructuredBuffer> GetImageBillboardStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct ImageShaderResourceIndices
{
	uint sprite_index_;
	uint billboard_index_;
	uint2 image_shader_resource_padding_0_;
};

#endif // __IMAGE_HLSL__
