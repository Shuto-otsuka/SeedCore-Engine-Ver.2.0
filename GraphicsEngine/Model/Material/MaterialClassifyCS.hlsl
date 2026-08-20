#include "../Model.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Constants.hlsli"

/**
* [EN]
* VisibilityBuffer material sort, pass 1/3: counts how many foreground
* pixels land in each (instance_index % MATERIAL_SORT_BUCKET_COUNT) bucket.
* Model/MaterialPrefixSumCS.hlsl turns these counts into exclusive-scan
* offsets, and Model/MaterialScatterCS.hlsl scatters pixels into their
* bucket's range so Model/MaterialResolveCS.hlsl can walk them
* material-grouped instead of in raw screen order.
*
* ---------------------------------------------------------------------
*
* [JP]
* VisibilityBuffer マテリアルソート、パス1/3: 前景ピクセルが
* (instance_index % MATERIAL_SORT_BUCKET_COUNT) の各バケットへ何個
* 落ちるかを数える。Model/MaterialPrefixSumCS.hlsl がこのカウントを
* 排他的スキャンのオフセットへ変え、Model/MaterialScatterCS.hlsl が
* 各ピクセルをそのバケットの範囲へ書き出すことで、
* Model/MaterialResolveCS.hlsl が生のスクリーン順ではなくマテリアルで
* まとまった順に辿れるようにする。
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

	/// [JP] reverse-Z のクリア値は 0.0 = 背景。ソートに参加させないが、この
	///      パスは全画面を走査する唯一のパス(Scatter/Resolveはソート済み
	///      リスト経由で前景ピクセルしか回らない)なので、RT0-3への明示的な
	///      ゼロ書き込みもここで行う - さもないと Editor/Game で同じ
	///      geometryBuffer_ を使い回した時、前回ディスパッチの内容が背景
	///      ピクセルに残り、前のカメラの絵が二重露光のように透けて見える
	///      (DeferredLightingPS.hlsl は rt1 が全ゼロかどうかで背景判定する)。
	if (depth == 0.0)
	{
		RWTexture2D<float4> background_rt0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_uav_];
		RWTexture2D<float4> background_rt1 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_uav_];
		RWTexture2D<float2> background_rt2 = ResourceDescriptorHeap[structured_indices.gbuffer_.velocity_uav_index_];
		RWTexture2D<float4> background_rt3 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_3_uav_];
		background_rt0[pixel] = float4(0.0, 0.0, 0.0, 0.0);
		background_rt1[pixel] = float4(0.0, 0.0, 0.0, 0.0);
		background_rt2[pixel] = float2(0.0, 0.0);
		background_rt3[pixel] = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	Texture2D<uint4> visibility_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint4 visibility_id = visibility_texture.Load(int3(pixel, 0));

	uint instance_index;
	uint meshlet_index;
	uint triangle_in_meshlet_index;
	UnpackVisibilityID(visibility_id, instance_index, meshlet_index, triangle_in_meshlet_index);

	uint bucket = instance_index % MATERIAL_SORT_BUCKET_COUNT;

	RWByteAddressBuffer bucket_buffer = ResourceDescriptorHeap[structured_indices.material_sort_.bucket_index_];
	uint original_value;
	bucket_buffer.InterlockedAdd(bucket * 4, 1, original_value);
}
