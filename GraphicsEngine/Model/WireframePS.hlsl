#include "Model.hlsli"

/**
* [JP]
* ワイヤーフレーム表示用ピクセルシェーダ。ラスタライザを WireNoneLHS（FillMode=
* Wireframe / Cull=None）にした専用 PSO で、モデルの辺だけを単色で描く。
* G-Buffer は書かず、エディタフレームバッファ（R16G16B16A16_FLOAT）へ 1 RT 出力。
* MS は StaticModelMS / SkeletalModelMS を流用（余分な出力は使わない）。
*/
float4 main(ModelMSOutput input) : SV_Target0
{
	return float4(0.55, 0.8, 1.0, 1.0);
}
