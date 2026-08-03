#include "Model.hlsli"

/**
* [JP]
* 選択アウトラインマスク用ピクセルシェーダ。選択中アクターのメッシュを単色
* （マスク値 1.0）で塗る。単チャンネル（R8_UNORM）ターゲットへ 1 RT 出力。
* MS は StaticModelMS / SkeletalModelMS を流用（余分な出力は使わない）。
*/
float main(ModelMSOutput input) : SV_Target0
{
	return 1.0;
}
