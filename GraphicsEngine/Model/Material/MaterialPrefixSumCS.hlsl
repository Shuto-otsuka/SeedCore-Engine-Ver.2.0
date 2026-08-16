#include "../Model.hlsli"
#include "../../Shader/Structured.hlsli"

/**
* [EN]
* VisibilityBuffer material sort, pass 2/3: single-dispatch Hillis-Steele
* exclusive prefix sum over the MATERIAL_SORT_BUCKET_COUNT bucket counts
* Model/MaterialClassifyCS.hlsl wrote, converting count[bucket] into
* offset[bucket] (the first slot in the sorted pixel list this bucket owns)
* in place. MATERIAL_SORT_BUCKET_COUNT is exactly the max threadgroup size
* (1024), so this whole scan fits in one threadgroup/one dispatch - no
* multi-pass reduction needed.
*
* ---------------------------------------------------------------------
*
* [JP]
* VisibilityBuffer マテリアルソート、パス2/3: Model/MaterialClassifyCS.hlsl が
* 書いた MATERIAL_SORT_BUCKET_COUNT 個のバケットカウントに対する、単一
* ディスパッチの Hillis-Steele 排他的接頭和。count[bucket] をその場で
* offset[bucket](このバケットが持つソート済みピクセルリストの先頭スロット)へ
* 変換する。MATERIAL_SORT_BUCKET_COUNT はちょうどスレッドグループの最大
* サイズ(1024)なので、このスキャン全体が1スレッドグループ/1ディスパッチに
* 収まる - 複数パスのリダクションは不要。
*/
groupshared uint scan_data[MATERIAL_SORT_BUCKET_COUNT];

[numthreads(MATERIAL_SORT_BUCKET_COUNT, 1, 1)]
void main(uint thread_id : SV_GroupThreadID)
{
	RWByteAddressBuffer bucket_buffer = ResourceDescriptorHeap[structured_indices.material_sort_.bucket_index_];

	uint original_count = bucket_buffer.Load(thread_id * 4);
	scan_data[thread_id] = original_count;
	GroupMemoryBarrierWithGroupSync();

	/// [EN] Inclusive scan: after iteration k, scan_data[i] holds the sum of
	///      the 2^(k+1) counts ending at i.
	/// [JP] 包含的スキャン: k回目の反復後、scan_data[i] はiで終わる2^(k+1)個の
	///      カウントの合計を持つ。
	[unroll]
	for (uint offset = 1; offset < MATERIAL_SORT_BUCKET_COUNT; offset *= 2)
	{
		uint added = 0;
		if (thread_id >= offset)
		{
			added = scan_data[thread_id - offset];
		}
		GroupMemoryBarrierWithGroupSync();
		scan_data[thread_id] += added;
		GroupMemoryBarrierWithGroupSync();
	}

	/// [JP] 包含的スキャン結果から自分自身のカウントを引けば排他的スキャン
	///      (=このバケットより前にある全バケットの合計 = 開始オフセット)。
	uint exclusive_offset = scan_data[thread_id] - original_count;
	bucket_buffer.Store(thread_id * 4, exclusive_offset);
}
