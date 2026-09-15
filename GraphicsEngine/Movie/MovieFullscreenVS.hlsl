#include "Movie.hlsli"
#include "../Shader/ShaderResources.hlsli"

MovieFullscreenMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<MovieFullscreenStructuredBuffer> movie_fullscreen = GetMovieFullscreenStructuredBuffer(shader_resource_indices.movie_.fullscreen_index_);
	MovieFullscreenStructuredBuffer instance = movie_fullscreen[instance_id];

	MovieFullscreenMSOutput output = (MovieFullscreenMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	if (!(instance.texture_aspect_ > 0.0f))
	{
		return output;
	}

	uint quad_indices[6] = { 0u, 1u, 2u, 1u, 3u, 2u };
	uint corner_index = quad_indices[vertex_id];

	float2 corners[4] =
	{
		float2(-1.0f,  1.0f),
		float2( 1.0f,  1.0f),
		float2(-1.0f, -1.0f),
		float2( 1.0f, -1.0f)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	output.position = float4(corners[corner_index], 0.0f, 1.0f);
	output.uv = uvs[corner_index];
	output.color = instance.color_;
	output.texture_index = instance.texture_index_;
	output.texture_aspect = instance.texture_aspect_;

	return output;
}
