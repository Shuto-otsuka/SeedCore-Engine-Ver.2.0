#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [JP]
* デプスプリパス用ピクセルシェーダ。レンダーターゲットは無く、深度だけを書く。
* 役割はただ一つ、カットアウト（alphaMode=MASK）の穴を clip して深度を書かせない
* こと。これが無いと穴も深度が埋まり、後ろのモデルが深度テストで消える。
* G-Buffer パスの clip と完全に一致させ、プリパス深度とズレないようにする。
* 不透明マテリアル（alpha_cutoff_ == 0）はサンプル不要なので即 return する。
*/
void main(DepthPrepassOutput input)
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	/// [JP] 不透明はカットアウトしないのでテクスチャサンプルを省く。
	if (instance.alpha_cutoff_ <= 0.0)
	{
		return;
	}

	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - instance.alpha_cutoff_);
}
