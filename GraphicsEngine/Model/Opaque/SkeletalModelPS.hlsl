#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Sampler.hlsli"

/**
* [EN]
* Pixel Shader for skeletal model G-Buffer output - VisibilityBuffer id only.
* Identical to StaticModelPS.
*
* ---------------------------------------------------------------------
*
* [JP]
* スケルタルモデル G-Buffer 出力用のピクセルシェーダー - VisibilityBuffer id
* のみ。StaticModelPS と同一。
*/
ModelPSOutput main(ModelMSOutput input, ModelMSPrimitiveOutput primitive)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	ModelStructuredBuffer instance = instances[input.instance_index];

	float4 base_color = instance.texture_.base_color_;
	if (instance.texture_.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.texture_.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - instance.texture_.alpha_cutoff_);

	ModelPSOutput output;
	output.visibility_id = PackVisibilityID(input.instance_index, input.meshlet_index, primitive.triangle_in_meshlet_index, input.texcoord);

	return output;
}
