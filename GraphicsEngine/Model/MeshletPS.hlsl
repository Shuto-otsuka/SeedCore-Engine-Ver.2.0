#include "Model.hlsli"

/**
* [JP]
* メッシュレット可視化用ピクセルシェーダ。メッシュレットごとに異なる色を塗り、
* Mesh Shader のメッシュレット分割を目で確認できるようにする。色は meshlet_index
* のハッシュから作る（隣接メッシュレットで色が変わる）。ソリッド塗り・単一 RT
* （エディタフレームバッファ）へ出力。エディタの表示モード専用。
*/
float3 MeshletColor(uint index)
{
	uint hash = index * 2654435761u;   // Knuth の乗算ハッシュ
	return float3(
		((hash >> 0)  & 0xFF) / 255.0,
		((hash >> 8)  & 0xFF) / 255.0,
		((hash >> 16) & 0xFF) / 255.0);
}

float4 main(ModelMSOutput input) : SV_Target0
{
	return float4(MeshletColor(input.meshlet_index), 1.0);
}
