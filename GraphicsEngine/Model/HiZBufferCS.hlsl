/**
* [EN]
* Hi-Z pyramid build pass. One dispatch per mip: each thread writes one
* destination texel as the MINIMUM of its 2x2 source footprint. With
* reverse-Z the minimum is the FARTHEST surface, which is what a
* conservative occlusion test needs. Odd source dimensions fold the extra
* edge row/column into the reduction so no depth sample is ever dropped.
*
* Pass 0 reads the depth prepass result (Texture2D SRV); later passes read
* the previous Hi-Z mip through a UAV so the whole pyramid can stay in the
* UNORDERED_ACCESS state during the build.
*
* ---------------------------------------------------------------------
*
* [JP]
* Hi-Z ピラミッド構築パス。ミップごとに 1 ディスパッチ: 各スレッドが
* ソース 2x2 範囲の最小値を書き込む。reverse-Z では最小値が最も遠い面で、
* 保守的なオクルージョンテストに必要な値になる。ソースが奇数サイズのときは
* 端の行・列も畳み込み、深度サンプルの取りこぼしをなくす。
*
* パス 0 はデプスプリパス結果（Texture2D SRV）を読み、以降のパスは前段の
* Hi-Z ミップを UAV 経由で読む。これによりピラミッド全体を構築中
* UNORDERED_ACCESS ステートのまま維持できる。
*/
#include "../Shader/Dispatch.hlsli"

struct HiZBuildConstantBuffer
{
	uint source_index_;
	uint destination_index_;
	uint destination_width_;
	uint destination_height_;
	uint source_width_;
	uint source_height_;
	uint source_is_depth_;
	uint hi_z_build_padding_0_;
};

ConstantBuffer<HiZBuildConstantBuffer> GetHiZBuildConstantBuffer()
{
	return ResourceDescriptorHeap[dispatch_buffer_index_];
}

float LoadSourceDepth(uint2 position)
{
	uint2 clamped = min(position, uint2(GetHiZBuildConstantBuffer().source_width_ - 1, GetHiZBuildConstantBuffer().source_height_ - 1));
	if (GetHiZBuildConstantBuffer().source_is_depth_ != 0)
	{
		Texture2D<float> source = ResourceDescriptorHeap[GetHiZBuildConstantBuffer().source_index_];
		return source.Load(int3(clamped, 0));
	}
	RWTexture2D<float> source = ResourceDescriptorHeap[GetHiZBuildConstantBuffer().source_index_];
	return source[clamped];
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	if (dtid.x >= GetHiZBuildConstantBuffer().destination_width_ || dtid.y >= GetHiZBuildConstantBuffer().destination_height_)
	{
		return;
	}

	uint2 base = dtid.xy * 2;
	float depth = LoadSourceDepth(base);
	depth = min(depth, LoadSourceDepth(base + uint2(1, 0)));
	depth = min(depth, LoadSourceDepth(base + uint2(0, 1)));
	depth = min(depth, LoadSourceDepth(base + uint2(1, 1)));

	bool odd_width = (GetHiZBuildConstantBuffer().source_width_ & 1) != 0;
	bool odd_height = (GetHiZBuildConstantBuffer().source_height_ & 1) != 0;
	if (odd_width)
	{
		depth = min(depth, LoadSourceDepth(base + uint2(2, 0)));
		depth = min(depth, LoadSourceDepth(base + uint2(2, 1)));
	}
	if (odd_height)
	{
		depth = min(depth, LoadSourceDepth(base + uint2(0, 2)));
		depth = min(depth, LoadSourceDepth(base + uint2(1, 2)));
	}
	if (odd_width && odd_height)
	{
		depth = min(depth, LoadSourceDepth(base + uint2(2, 2)));
	}

	RWTexture2D<float> destination = ResourceDescriptorHeap[GetHiZBuildConstantBuffer().destination_index_];
	destination[dtid.xy] = depth;
}
