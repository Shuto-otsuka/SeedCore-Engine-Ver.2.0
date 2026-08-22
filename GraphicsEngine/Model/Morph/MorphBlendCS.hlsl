/**
* [EN]
* Blends a SubMesh's morph target deltas into its RT proxy position range,
* composing before SkinnedPositionCS's skin matrix pass (which reads
* blended_positions instead of rt_positions when this SubMesh has morphs).
* Dispatched once per SubMesh that has active morph weights this frame; the
* caller pre-fills blended_positions with a full copy of rt_positions so
* SubMeshes/vertices outside this dispatch's range keep their base position.
*
* ---------------------------------------------------------------------
*
* [JP]
* SubMesh のモーフターゲットデルタを、その RT プロキシ位置範囲へブレンド
* する。SkinnedPositionCS のスキン行列パス(この SubMesh がモーフを持つ場合
* rt_positions の代わりに blended_positions を読む)より前に合成する。
* 今フレーム有効なモーフウェイトを持つ SubMesh ごとに1回ディスパッチする —
* 呼び出し側は blended_positions を rt_positions の全体コピーで事前に
* 埋めておくこと。このディスパッチの範囲外の SubMesh/頂点は元の位置の
* ままになる。
*/
struct MorphBlendParams
{
	uint vertex_offset_;
	uint vertex_count_;
	uint target_count_;
	uint pad0_;
};

ConstantBuffer<MorphBlendParams> params : register(b0);
StructuredBuffer<float3> rt_positions : register(t0);
StructuredBuffer<float3> rt_morph_deltas : register(t1);
StructuredBuffer<float> morph_weights : register(t2);
RWStructuredBuffer<float3> blended_positions : register(u0);

[NumThreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint local_index = id.x;
	if (local_index >= params.vertex_count_)
	{
		return;
	}

	uint global_index = params.vertex_offset_ + local_index;
	float3 position = rt_positions[global_index];

	/// [EN] rt_morph_deltas is target-major within this SubMesh's compact
	///      vertex range: target * vertex_count_ + local_index.
	/// [JP] rt_morph_deltas は、この SubMesh のコンパクト頂点範囲内で
	///      ターゲット主順: target * vertex_count_ + local_index。
	for (uint target = 0; target < params.target_count_; ++target)
	{
		position += rt_morph_deltas[target * params.vertex_count_ + local_index] * morph_weights[target];
	}

	/// [EN] Same contract as SkinnedPositionCS: this feeds a BLAS build (either
	///      directly for a morph-only instance, or through SkinnedPositionCS),
	///      so a non-finite vertex here hangs DXR traversal later. Fall back to
	///      the unmorphed base position.
	/// [JP] SkinnedPositionCS と同じ約束: ここの出力は(モーフのみのインスタンス
	///      なら直接、そうでなければ SkinnedPositionCS 経由で)BLAS 構築へ渡る
	///      ため、非有限な頂点は後段の DXR 走査をハングさせる。モーフ適用前の
	///      ベース位置へフォールバックする。
	if (!all(isfinite(position)))
	{
		position = rt_positions[global_index];
	}

	blended_positions[global_index] = position;
}
