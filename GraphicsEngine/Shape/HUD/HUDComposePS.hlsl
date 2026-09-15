#include "../../Shader/ShaderResources.hlsli"

struct HUDComposeOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

float4 main(HUDComposeOutput input) : SV_Target0
{
	Texture2D<float4> ui_color_alpha = ResourceDescriptorHeap[shader_resource_indices.hud_.ui_color_alpha_index_];
	return ui_color_alpha.Load(int3(input.position.xy, 0));
}
