#include "Movie.hlsli"
#include "../Shader/Sampler.hlsli"

Texture2D textures[] : register(t0);

float4 main(MovieMSOutput input) : SV_Target
{
	float4 texture_color = textures[input.texture_index].Sample(sampler_linear_clamp, input.uv);
	return float4(texture_color.rgb * input.color.rgb, input.color.a);
}
