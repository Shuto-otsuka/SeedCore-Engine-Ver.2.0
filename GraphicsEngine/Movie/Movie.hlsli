#ifndef __MOVIE_HLSL__
#define __MOVIE_HLSL__

struct MovieASPayload
{
	uint instance_indices[32];
};

struct MovieMSOutput
{
	float4 position : SV_Position;
	float2 uv        : TEXCOORD0;
	float4 color     : COLOR0;
	nointerpolation uint texture_index : BLENDINDICES;
};

struct MovieFullscreenMSOutput
{
	float4 position : SV_Position;
	float2 uv        : TEXCOORD0;
	float4 color     : COLOR0;
	nointerpolation uint texture_index    : BLENDINDICES;
	nointerpolation float texture_aspect  : TEXCOORD1;
};

struct MovieSpriteStructuredBuffer
{
	float2 position_;
	float rotation_;
	float2 scale_;
	float movie_sprite_structured_buffer_padding_0_;
	float2 size_;
	float2 movie_sprite_structured_buffer_padding_1_;
	float2 pivot_;
	float2 movie_sprite_structured_buffer_padding_2_;
	float4 color_;
	uint texture_index_;
	uint selected_;
	uint2 movie_sprite_structured_buffer_padding_3_;
};

StructuredBuffer<MovieSpriteStructuredBuffer> GetMovieSpriteStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct MovieBillboardStructuredBuffer
{
	float3 position_;
	float3 rotation_;
	float2 scale_;
	float2 size_;
	float2 movie_billboard_structured_buffer_padding_0_;
	float2 pivot_;
	float2 movie_billboard_structured_buffer_padding_1_;
	float4 color_;
	uint texture_index_;
	uint face_camera_;
	uint selected_;
	float movie_billboard_structured_buffer_padding_2_;
};

StructuredBuffer<MovieBillboardStructuredBuffer> GetMovieBillboardStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct MovieFullscreenStructuredBuffer
{
	float4 color_;
	uint texture_index_;
	float texture_aspect_;
	uint2 movie_fullscreen_structured_buffer_padding_0_;
};

StructuredBuffer<MovieFullscreenStructuredBuffer> GetMovieFullscreenStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct MovieShaderResourceIndices
{
	uint sprite_index_;
	uint billboard_index_;
	uint fullscreen_index_;
	uint movie_shader_resource_padding_0_;
};

#endif // __MOVIE_HLSL__
