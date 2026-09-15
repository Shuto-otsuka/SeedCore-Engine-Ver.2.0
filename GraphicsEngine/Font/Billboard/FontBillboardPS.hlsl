#include "../Font.hlsli"
#include "../../Shader/Sampler.hlsli"

float4 main(FontMSOutput input) : SV_Target
{
	Texture2D<float4> texture_ = ResourceDescriptorHeap[input.texture_index];
	float4 mtsdf = texture_.Sample(sampler_linear_clamp, input.uv);

	float4 color = MtsdfCompose(mtsdf, input);

	clip(color.a - 0.01f);

	return color;
}
