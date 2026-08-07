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

	/// [EN] Insertion sort by ascending depth. This engine uses REVERSE-Z
	///      (GeometryBuffer clears depth to 0.0 = far plane, so 1.0 = near),
	///      which means ascending depth orders the array FARTHEST-first.
	/// [JP] 深度の昇順で挿入ソートする。このエンジンは reverse-Z
	///      (GeometryBuffer が深度を 0.0 = 遠平面でクリアするので 1.0 が手前)
	///      なので、深度の昇順は【奥から手前】の並びになる。
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

	/// [EN] Composite back-to-front, i.e. walk the array forwards: the sort
	///      above already put the farthest fragment at index 0 under reverse-Z.
	///      The "over" operator below treats `color` as being IN FRONT of the
	///      accumulated `result`, so each newly processed fragment must be
	///      nearer than everything already accumulated - walking backwards
	///      (as this did while it still assumed standard 0=near depth) makes
	///      far fragments paint over near ones and inverts the whole sort.
	/// [JP] 奥から手前へ合成する = 配列を先頭から回す。上のソートが reverse-Z
	///      では最も奥のフラグメントを index 0 に置いているため。下の over
	///      演算子は `color` が累積済みの `result` より【手前】である前提なので、
	///      新たに処理するフラグメントは常に累積済みより手前でなければならない。
	///      逆向きに回すと(標準深度 0=手前 を前提にしていた頃の実装がそうだった)
	///      奥のフラグメントが手前を上塗りし、ソート結果が丸ごと反転する。
	///
	///      出力 rgb は事前乗算済み(premultiplied)。合成先の PSO は
	///      BlendStateType::AlphaPremultiplied (SrcBlend = ONE) であること
	///      (ModelShader.cpp 参照) - 通常の Alpha だとアルファが二重に掛かる。
	float4 result = float4(0.0, 0.0, 0.0, 0.0);
	{
		for (uint index = 0; index < count; index++)
		{
			float4 color = UnpackColorRGBA8(fragments[index].packed_color_);
			result.rgb = color.rgb * color.a + result.rgb * (1.0 - color.a);
			result.a = color.a + result.a * (1.0 - color.a);
		}
	}

	return result;
}
