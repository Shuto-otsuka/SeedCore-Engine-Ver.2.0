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
struct DepthResizeConstants
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
ConstantBuffer<DepthResizeConstants> depth_resize : register(b0, space1);

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	if (dtid.x >= depth_resize.destination_width_ || dtid.y >= depth_resize.destination_height_)
	{
		return;
	}

	uint2 source_pixel = uint2(
		(dtid.x * depth_resize.source_width_) / depth_resize.destination_width_,
		(dtid.y * depth_resize.source_height_) / depth_resize.destination_height_
	);
	source_pixel = min(source_pixel, uint2(depth_resize.source_width_ - 1, depth_resize.source_height_ - 1));

	Texture2D<float> source = ResourceDescriptorHeap[depth_resize.source_index_];
	float depth = source.Load(int3(source_pixel, 0));

	RWTexture2D<float> destination = ResourceDescriptorHeap[depth_resize.destination_index_];
	destination[dtid.xy] = depth;
}
