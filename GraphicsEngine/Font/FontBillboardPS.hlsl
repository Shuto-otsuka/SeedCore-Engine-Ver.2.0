#include "Font.hlsli"
#include "../Shader/Sampler.hlsli"

Texture2D textures[] : register(t0);

float4 main(FontMSOutput input) : SV_Target
{
	float4 mtsdf = textures[input.texture_index].Sample(sampler_linear_clamp, input.uv);

	float4 color = MtsdfCompose(mtsdf, input);

	clip(color.a - 0.01f);

	return color;
}
