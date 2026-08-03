#include "Image.hlsli"
#include "../Shader/Sampler.hlsli"

Texture2D textures[] : register(t0);

float4 main(ImageMSOutput input) : SV_Target
{
	float4 color = textures[input.texture_index].Sample(sampler_linear_wrap, input.uv);
	color *= input.color;

	clip(color.a - 0.01);

	return color;
}
