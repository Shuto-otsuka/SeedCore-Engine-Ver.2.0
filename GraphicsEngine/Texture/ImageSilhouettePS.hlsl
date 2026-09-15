#include "Image.hlsli"
#include "../Shader/Sampler.hlsli"

float main(ImageMSOutput input) : SV_Target0
{
	Texture2D<float4> texture_ = ResourceDescriptorHeap[input.texture_index];
	float alpha = texture_.Sample(sampler_linear_clamp, input.uv).a * input.color.a;
	clip(alpha - 0.01);

	return 1.0;
}
