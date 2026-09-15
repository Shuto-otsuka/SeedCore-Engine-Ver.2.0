#include "../Movie.hlsli"
#include "../../Shader/Sampler.hlsli"

float4 main(MovieMSOutput input) : SV_Target
{
	Texture2D<float4> texture_ = ResourceDescriptorHeap[input.texture_index];
	float4 texture_color = texture_.Sample(sampler_linear_clamp, input.uv);
	return float4(texture_color.rgb * input.color.rgb, input.color.a);
}
