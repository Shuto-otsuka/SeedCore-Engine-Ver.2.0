#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

/**
* [EN]
* VisibilityBuffer material sort, pass 3/3: re-walks every foreground pixel
* (same test as Model/MaterialClassifyCS.hlsl) and atomically claims the next
* free slot in its bucket's range (Model/MaterialPrefixSumCS.hlsl turned the
* bucket buffer from counts into offsets; this pass's InterlockedAdd turns
* those offsets into running write cursors), writing the pixel's linear
* screen coordinate there. Model/MaterialResolveCS.hlsl is dispatched 1D over
* the resulting list.
*
* ---------------------------------------------------------------------
*
* [JP]
* VisibilityBuffer マテリアルソート、パス3/3: 全前景ピクセルを再度辿り
* (Model/MaterialClassifyCS.hlsl と同じ判定)、そのバケット範囲内の次の
* 空きスロットを atomic に確保して(Model/MaterialPrefixSumCS.hlsl が
* bucket バッファをカウントからオフセットへ変えており、このパスの
* InterlockedAdd がそのオフセットを進行中の書き込みカーソルへ変える)、
* ピクセルの線形スクリーン座標をそこへ書く。Model/MaterialResolveCS.hlsl は
* 結果のリストを1Dディスパッチで辿る。
*/
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	if (dtid.x >= (uint)scene.screen_size_.x || dtid.y >= (uint)scene.screen_size_.y)
	{
		return;
	}

	uint2 pixel = dtid.xy;

	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		return;
	}

	Texture2D<uint2> visibility_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint2 visibility_id = visibility_texture.Load(int3(pixel, 0));

	uint instance_index;
	uint meshlet_index;
	uint triangle_in_meshlet_index;
	UnpackVisibilityID(visibility_id, instance_index, meshlet_index, triangle_in_meshlet_index);

	uint bucket = instance_index % MATERIAL_SORT_BUCKET_COUNT;

	RWByteAddressBuffer bucket_buffer = ResourceDescriptorHeap[structured_indices.material_sort_.bucket_index_];
	uint write_slot;
	bucket_buffer.InterlockedAdd(bucket * 4, 1, write_slot);

	RWByteAddressBuffer sorted_pixel_list = ResourceDescriptorHeap[structured_indices.material_sort_.sorted_pixel_list_index_];
	uint screen_width = (uint)scene.screen_size_.x;
	uint linear_pixel = pixel.y * screen_width + pixel.x;
	sorted_pixel_list.Store(write_slot * 4, linear_pixel);
}
