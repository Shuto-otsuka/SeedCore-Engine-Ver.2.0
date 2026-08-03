#include "Movie.hlsli"
#include "../Shader/Sampler.hlsli"

Texture2D textures[] : register(t0);

float main(MovieMSOutput input) : SV_Target0
{
	float alpha = textures[input.texture_index].Sample(sampler_linear_clamp, input.uv).a * input.color.a;
	clip(alpha - 0.01);

	return 1.0;
}
