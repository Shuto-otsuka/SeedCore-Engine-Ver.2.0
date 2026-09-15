/**
* [EN]
* Point-resamples a native-resolution depth texture into a
* different-resolution destination (typically the DLSS Ray
* Reconstruction-upscaled output resolution), so a fixed-function
* depth-tested pass can bind a depth buffer matching whatever resolution
* it is actually rendering at. Nearest-neighbor rather than bilinear -
* blending two unrelated depth values across a silhouette edge would
* produce a nonsensical intermediate depth, worse than picking one side.
*
* ---------------------------------------------------------------------
*
* [JP]
* ネイティブ解像度の深度テクスチャを、別解像度の出力先(通常はDLSS Ray
* Reconstructionでアップスケールされた出力解像度)へポイントリサンプルする。
* これにより、固定機能の深度テストを使うパスが、実際に描画している
* 解像度に一致する深度バッファをバインドできるようになる。バイリニアでは
* なくニアレストネイバー - シルエットの境界をまたいで無関係な2つの深度値を
* ブレンドすると、どちらか一方を選ぶより悪い、意味のない中間深度になる。
*/
#include "../Shader/Dispatch.hlsli"

struct DepthResizeConstantBuffer
{
	uint source_index_;
	uint destination_index_;
	uint destination_width_;
	uint destination_height_;
	uint source_width_;
	uint source_height_;
	uint depth_resize_padding_0_;
	uint depth_resize_padding_1_;
};

ConstantBuffer<DepthResizeConstantBuffer> GetDepthResizeConstantBuffer()
{
	return ResourceDescriptorHeap[dispatch_buffer_index_];
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	if (dtid.x >= GetDepthResizeConstantBuffer().destination_width_ || dtid.y >= GetDepthResizeConstantBuffer().destination_height_)
	{
		return;
	}

	uint2 source_pixel = uint2(
		(dtid.x * GetDepthResizeConstantBuffer().source_width_) / GetDepthResizeConstantBuffer().destination_width_,
		(dtid.y * GetDepthResizeConstantBuffer().source_height_) / GetDepthResizeConstantBuffer().destination_height_
	);
	source_pixel = min(source_pixel, uint2(GetDepthResizeConstantBuffer().source_width_ - 1, GetDepthResizeConstantBuffer().source_height_ - 1));

	Texture2D<float> source = ResourceDescriptorHeap[GetDepthResizeConstantBuffer().source_index_];
	float depth = source.Load(int3(source_pixel, 0));

	RWTexture2D<float> destination = ResourceDescriptorHeap[GetDepthResizeConstantBuffer().destination_index_];
	destination[dtid.xy] = depth;
}
