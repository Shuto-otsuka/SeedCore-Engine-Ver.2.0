#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Sampler.hlsli"

/**
* [EN]
* Pixel Shader for static model G-Buffer output - VisibilityBuffer id only.
* Still samples the base color texture to apply the alpha-cutout clip (masked
* materials must not get a visibility id where they're cut out), but no
* longer computes/writes normal, roughness, metallic, emissive, or velocity -
* Model/MaterialResolveCS.hlsl rewrites those from this id + depth afterward.
*
* ---------------------------------------------------------------------
*
* [JP]
* 静的モデル G-Buffer 出力用のピクセルシェーダー - VisibilityBuffer id のみ。
* アルファカットアウトの clip 判定のためベースカラーテクスチャは引き続き
* サンプルする(マスクマテリアルはカットアウト部分に visibility id を
* 持ってはいけない)が、法線・ラフネス・メタリック・エミッシブ・速度は
* もう計算/書き込みしない - この後 Model/MaterialResolveCS.hlsl がこの id と
* depth から書き直す。
*/
ModelPSOutput main(ModelMSOutput input, ModelMSPrimitiveOutput primitive)
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	ModelStructuredBuffer instance = instances[input.instance_index];

	/// [EN] Sample base color texture if available, multiply by material color.
	/// [JP] ベースカラーテクスチャがあればサンプリングし、マテリアルカラーと乗算。
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
