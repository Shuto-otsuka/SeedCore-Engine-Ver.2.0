#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"

/**
* [EN]
* Pixel shader for the Timeline panel's 3D model preview. Deliberately
* matches DeferredCompositePS.hlsl's view_mode_==1 ("albedo only" debug
* branch) exactly: base color (x texture), clip-tested, no lighting, no
* normal mapping, no emissive — so the preview shows the same flat color a
* user already knows how to read from that debug view, instead of an
* independently-invented shading result. Output is a single
* R16G16B16A16_FLOAT render target.
*
* ---------------------------------------------------------------------
*
* [JP]
* Timelineパネルの3Dモデルプレビュー用ピクセルシェーダー。
* DeferredCompositePS.hlslのview_mode_==1（「アルベドのみ」デバッグ分岐）と
* あえて完全に一致させる: baseColor（×テクスチャ）をclipテストするだけで、
* ライティングも法線マップもemissiveも無し — 独自に考案したシェーディング
* 結果ではなく、既にそのデバッグ表示で見慣れているのと同じ色をプレビューに
* 出す。出力は単一のR16G16B16A16_FLOATレンダーターゲット。
*/
float4 main(ModelMSOutput input) : SV_Target0
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - instance.alpha_cutoff_);

	return float4(base_color.rgb, 1.0);
}
