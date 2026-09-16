#include "../Texture.hlsli"
#include "../../Shader/Sampler.hlsli"

float4 main(TextureMSOutput input) : SV_Target
{
	Texture2D<float4> texture_ = ResourceDescriptorHeap[input.texture_index];
	float4 color = texture_.Sample(sampler_linear_wrap, input.uv);
	color *= input.color;

	clip(color.a - 0.01);

	return color;
}
