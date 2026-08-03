#include "Model.hlsli"
#include "../Shader/Structured.hlsli"

/**
* [EN]
* OIT Resolve Pixel Shader.
* Walks the Per-Pixel Linked List for the current pixel, collects
* up to OIT_MAX_LAYERS (4) fragments, sorts them front-to-back by
* depth, then blends back-to-front to produce the final transparent
* color with alpha. The output is composited over the opaque scene
* via alpha blending in the PSO.
*
* ---------------------------------------------------------------------
*
* [JP]
* OIT リゾルブ ピクセルシェーダー。
* 現在のピクセルの Per-Pixel Linked List を走査し、最大 OIT_MAX_LAYERS (4)
* フラグメントを収集、深度でフロントからバックにソートした後、
* バックからフロントにブレンドして最終的な透明色 + アルファを出力する。
* 出力は PSO のアルファブレンドで不透明シーン上に合成される。
*/
float4 main(OITResolveOutput input) : SV_Target0
{
	RWTexture2D<uint> head_pointer = ResourceDescriptorHeap[structured_indices.oit_.head_pointer_index_];
	RWStructuredBuffer<OITFragment> fragment_buffer = ResourceDescriptorHeap[structured_indices.oit_.fragment_buffer_index_];

	uint2 pixel = uint2(input.position.xy);
	uint index = head_pointer[pixel];

	if (index == OIT_INVALID_INDEX)
	{
		discard;
	}

	OITFragment fragments[OIT_MAX_LAYERS];
	uint count = 0;

	[unroll(OIT_MAX_LAYERS)]
	while (index != OIT_INVALID_INDEX && count < OIT_MAX_LAYERS)
	{
		fragments[count] = fragment_buffer[index];
		index = fragments[count].next_;
		count++;
	}

	/// [EN] Insertion sort by depth (front-to-back: smallest depth first).
	/// [JP] 深度でソート（フロントからバック: 深度が小さい順）。
	{
		for (uint index = 1; index < count; index++)
		{
			OITFragment key = fragments[index];
			int j_index = int(index) - 1;
			while (j_index >= 0 && fragments[j_index].depth_ > key.depth_)
			{
				fragments[j_index + 1] = fragments[j_index];
				j_index--;
			}
			fragments[j_index + 1] = key;
		}
	}

	/// [EN] Blend back-to-front (start from the furthest fragment).
	/// [JP] バックからフロントにブレンド（最も遠いフラグメントから開始）。
	float4 result = float4(0.0, 0.0, 0.0, 0.0);
	{
		for (int index = int(count) - 1; index >= 0; index--)
		{
			float4 color = UnpackColorRGBA8(fragments[index].packed_color_);
			result.rgb = color.rgb * color.a + result.rgb * (1.0 - color.a);
			result.a = color.a + result.a * (1.0 - color.a);
		}
	}

	return result;
}
