#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "CloudNoiseBake.hlsli"

/**
* [EN]
* Bakes the cloud shape noise (low frequency) into a Texture3D once. Instead
* of the runtime raymarch computing Fbm/Worley every step, it just samples
* this pre-baked texture through the wrap sampler (tileable, so no seams).
* The content is Perlin-Worley - inverted Worley's "puffy blobs" modulated by
* Perlin's large-scale billowing. Inverted Worley alone reads as a foam of
* equal-sized bubbles with no sense of connected cloud masses.
*
* 128^3 at base_cells=4 with 4 octaves (4/8/16/32 cells). Even the top octave
* keeps 4 voxels per cell - packing it tighter would bake to aliased hash
* noise instead of usable detail.
*
* ---------------------------------------------------------------------
*
* [JP]
* 雲の形状ノイズ(低周波)を Texture3D へ一度だけベイクする。実行時のレイマーチ
* が毎ステップ Fbm/Worley を計算する代わりに、この焼き込み済みテクスチャを
* wrap サンプラーで読むだけで済むようにする(タイル可能なので継ぎ目なし)。
* 中身は Perlin-Worley - 反転 Worley の「もこもこの塊」に Perlin の大きな
* うねりを掛けたもの。反転 Worley 単体だと同じ大きさの粒が一面に並んだ
* 泡状の見た目になり、雲の塊としてのつながりが出ないため。
*
* 128^3 に対して base_cells=4 の 4 オクターブ(4/8/16/32セル)。最上位
* オクターブでも 1 セル 4 ボクセルを確保している - ここを詰めるとベイク時点で
* エイリアスしたハッシュノイズになる。
*/
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture3D<float> shape_output = ResourceDescriptorHeap[structured_indices.cloud_.shape_noise_uav_index_];

	uint width, height, depth;
	shape_output.GetDimensions(width, height, depth);

	/// [EN] Bounds guard: the dispatch is rounded up to a multiple of the
	///      4x4x4 thread group size, so threads past the actual volume's
	///      edge on any axis must bail out before touching any resource.
	/// [JP] 範囲外ガード: ディスパッチは 4x4x4 スレッドグループサイズの
	///      倍数に切り上げられているので、どの軸でも実際のボリューム端を
	///      超えたスレッドはリソースに触れる前に抜ける必要がある。
	if (dtid.x >= width || dtid.y >= height || dtid.z >= depth)
	{
		return;
	}

	float3 uvw = (float3(dtid) + 0.5) / float3(width, height, depth);

	shape_output[dtid] = TileablePerlinWorley(uvw, 4.0, 4);
}
