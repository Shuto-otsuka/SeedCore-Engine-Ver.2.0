#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "CloudNoiseBake.hlsli"

/**
* [JP]
* 雲の形状ノイズ(低周波)を Texture3D へ一度だけベイクする。実行時のレイマーチ
* が毎ステップ Fbm/Worley を計算する代わりに、この焼き込み済みテクスチャを
* wrap サンプラーで読むだけで済むようにする(タイル可能なので継ぎ目なし)。
* 中身は Perlin-Worley — 反転 Worley の「もこもこの塊」に Perlin の大きな
* うねりを掛けたもの。反転 Worley 単体だと同じ大きさの粒が一面に並んだ
* 泡状の見た目になり、雲の塊としてのつながりが出ないため。
*
* 128^3 に対して base_cells=4 の 4 オクターブ(4/8/16/32セル)。最上位
* オクターブでも 1 セル 4 ボクセルを確保している — ここを詰めるとベイク時点で
* エイリアスしたハッシュノイズになる。
*/
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture3D<float> shape_output = ResourceDescriptorHeap[structured_indices.cloud_.shape_noise_uav_index_];

	uint width, height, depth;
	shape_output.GetDimensions(width, height, depth);

	if (dtid.x >= width || dtid.y >= height || dtid.z >= depth)
	{
		return;
	}

	float3 uvw = (float3(dtid) + 0.5) / float3(width, height, depth);

	shape_output[dtid] = TileablePerlinWorley(uvw, 4.0, 4);
}
