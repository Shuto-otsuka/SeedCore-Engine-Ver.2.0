#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "CloudNoiseBake.hlsli"

/**
* [JP]
* 雲のディテールノイズ(高周波)を Texture3D へ一度だけベイクする。形状ノイズ
* より細かい反転 Worley FBM で、雲の輪郭を侵食してもこもこの縁を作るのに使う。
* 縁の侵食は Worley 系の担当なので、こちらは Perlin を混ぜず反転 Worley のまま。
*
* 64^3 に対して base_cells=4 の 3 オクターブ(4/8/16セル)。最上位オクターブで
* 1 セル 4 ボクセル — エイリアスを避けるため最上位でもセルあたり複数ボクセルを確保する。
*/
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	RWTexture3D<float> detail_output = ResourceDescriptorHeap[structured_indices.cloud_.detail_noise_uav_index_];

	uint width, height, depth;
	detail_output.GetDimensions(width, height, depth);

	if (dtid.x >= width || dtid.y >= height || dtid.z >= depth)
	{
		return;
	}

	float3 uvw = (float3(dtid) + 0.5) / float3(width, height, depth);

	detail_output[dtid] = TileableWorleyFbm(uvw, 4.0, 3);
}
