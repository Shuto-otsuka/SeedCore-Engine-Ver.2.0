#include "Font.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [JP]
* 選択アウトラインマスク用ピクセルシェーダ。FontSpritePS/FontBillboardPS と
* 同じ MTSDF 合成でアルファを求めてクリップ判定し、マスク値 1.0 を書く。
* Sprite/Billboard 共通（FontMSOutput が同じ）。
*/
float main(FontMSOutput input) : SV_Target0
{
	Texture2D<float4> texture_ = ResourceDescriptorHeap[input.texture_index];
	float4 mtsdf = texture_.Sample(sampler_linear_clamp, input.uv);

	float4 color = MtsdfCompose(mtsdf, input);

	clip(color.a - 0.01f);

	return 1.0;
}
