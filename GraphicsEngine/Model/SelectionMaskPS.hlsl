#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [JP]
* 選択アウトラインマスク用ピクセルシェーダ。選択中アクターのメッシュを単色
* （マスク値 1.0）で塗る。単チャンネル（R8_UNORM）ターゲットへ 1 RT 出力。
* MS は StaticModelMS / SkeletalModelMS を流用（余分な出力は使わない）。
* G-Buffer パスと同じアルファカットアウトの clip を掛ける。
*/
float main(ModelMSOutput input) : SV_Target0
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	if (instance.alpha_cutoff_ > 0.0)
	{
		float4 base_color = instance.base_color_;
		if (instance.base_color_texture_index_ != 0xFFFFFFFF)
		{
			Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
			base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
		}

		clip(base_color.a - instance.alpha_cutoff_);
	}

	return 1.0;
}
