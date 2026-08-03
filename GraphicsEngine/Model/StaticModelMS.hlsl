#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"
#include "../Shader/Culling.hlsli"

/**
* [EN]
* Reference: https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html
*
* Mesh Shader for static (non-skinned) model rendering.
*
* Each thread group processes one meshlet. For each vertex in the meshlet:
*   1. Read the global vertex via the 2-level indirection
*      (primitiveIndices → vertexIndices → global vertex)
*   2. Transform position by world and view-projection matrices
*   3. Compute world-space normal and tangent for G-Buffer
*   4. Compute current and previous clip-space positions for velocity
*
* Max output: 64 vertices, 124 triangles (meshlet constraints).
*
* ---------------------------------------------------------------------
*
* [JP]
* 静的（スキニングなし）モデルレンダリング用の Mesh Shader。
*
* 各スレッドグループが 1 つのメシュレットを処理する。メシュレット内の各頂点に対し:
*   1. 2 段階間接参照でグローバル頂点を読み取る
*      (primitiveIndices → vertexIndices → グローバル頂点)
*   2. ワールド行列とビュー・プロジェクション行列で位置を変換
*   3. G-Buffer 用にワールド空間の法線とタンジェントを計算
*   4. 速度ベクトル用に現在と前フレームのクリップ空間位置を計算
*
* 最大出力: 64 頂点、124 三角形（メシュレット制約）。
*/
/// [EN] Clip-space positions shared with the triangle phase for per-triangle
///      backface culling (fixed-function culling is CullNone).
/// [JP] 三角形単位バックフェイスカリング用に三角形フェーズと共有する
///      クリップ空間位置（固定機能カリングは CullNone）。
groupshared float4 clip_positions[64];

[NumThreads(64, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ModelASPayload as_payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ModelMSOutput verts[64], out indices uint3 tris[124])
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];

	uint meshlet_index = as_payload.meshlet_indices[gid];
	uint instance_index = as_payload.instance_index;
	ModelInstance instance = instances[instance_index];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [EN] Skinned instances are drawn by SkeletalModelMS — emit nothing here.
	///      Folded into the counts because the DXIL validator forbids more than
	///      one SetMeshOutputCounts call site (branch is wave-uniform: instance
	///      comes from the payload).
	/// [JP] スキンインスタンスは SkeletalModelMS が描画するため、ここでは何も
	///      出力しない。DXIL バリデータが SetMeshOutputCounts の複数 call site を
	///      禁止しているため、カウントに畳み込む（instance はペイロード由来なので
	///      分岐は wave-uniform）。
	bool skip = instance.skin_index_ != 0xFFFFFFFF;
	SetMeshOutputCounts(skip ? 0 : meshlet.vertex_count_, skip ? 0 : meshlet.triangle_count_);
	if (skip)
	{
		return;
	}

	/// [EN] Each thread processes one vertex (up to 64 per meshlet).
	/// [JP] 各スレッドが 1 頂点を処理する（メシュレットあたり最大 64）。
	if (gtid < meshlet.vertex_count_)
	{
		uint global_vertex_index = vertex_indices[meshlet.vertex_offset_ + gtid];
		ModelVertex vertex = DecodeModelVertex(vertices[global_vertex_index], instance);

		float4 world_position = mul(float4(vertex.position_, 1.0), instance.world_);
		float4 clip_position = mul(world_position, scene.current_view_projection_);

		/// [JP] 前フレームのクリップ位置はインスタンス自身の前フレームの
		///      ワールド行列で計算する(今フレームの行列を使い回すとカメラの
		///      動きしか速度に反映されず、動く/アニメーションするオブジェクトが
		///      常に速度ゼロ扱いになる)。
		float4 previous_world_position = mul(float4(vertex.position_, 1.0), instance.previous_world_);
		float4 previous_clip_position = mul(previous_world_position, scene.previous_view_projection_);

		float3 world_normal = normalize(mul(float4(vertex.normal_, 0.0), instance.inverse_transpose_world_).xyz);
		float4 world_tangent = float4(normalize(mul(float4(vertex.tangent_.xyz, 0.0), instance.world_).xyz), vertex.tangent_.w);

		ModelMSOutput output;
		output.position = clip_position;
		output.world_position = world_position.xyz;
		output.world_normal = world_normal;
		output.world_tangent = world_tangent;
		output.texcoord = vertex.texcoord_;
		output.current_position = clip_position;
		output.previous_position = previous_clip_position;
		output.instance_index = instance_index;
		output.meshlet_index = meshlet_index;

		verts[gtid] = output;
		clip_positions[gtid] = clip_position;
	}

	GroupMemoryBarrierWithGroupSync();

	/// [EN] Up to 124 triangles per meshlet but only 64 threads — each thread
	///      loops so every declared primitive gets written. Leaving primitives
	///      64..123 unwritten feeds uninitialised indices to the rasterizer,
	///      which produced frame-varying garbage triangles (streak ribbons).
	///      Primitive indices are packed as 3 bytes per triangle in a ByteAddressBuffer.
	/// [JP] メシュレットあたり最大 124 三角形に対しスレッドは 64 本のみ — 全宣言
	///      プリミティブが書かれるよう各スレッドがループする。プリミティブ 64..123 を
	///      未書き込みのままにするとラスタライザに未初期化インデックスが渡り、
	///      フレームごとに変わるゴミ三角形（すじ状リボン）が出ていた。
	///      プリミティブインデックスは ByteAddressBuffer に三角形あたり 3 バイトで格納。
	for (uint triangle_index = gtid; triangle_index < meshlet.triangle_count_; triangle_index += 64)
	{
		uint byte_offset = meshlet.triangle_offset_ + triangle_index * 3;
		uint aligned_offset = byte_offset & ~3;
		uint shift = (byte_offset & 3) * 8;

		uint dword0 = primitive_indices.Load(aligned_offset);
		uint packed = dword0 >> shift;
		if (shift > 8)
		{
			uint dword1 = primitive_indices.Load(aligned_offset + 4);
			packed |= dword1 << (32 - shift);
		}

		uint i0 = packed & 0xFF;
		uint i1 = (packed >> 8) & 0xFF;
		uint i2 = (packed >> 16) & 0xFF;

		/// [EN] Single-sided materials: cull back faces here by emitting a
		///      degenerate triangle. doubleSided materials keep both sides.
		/// [JP] 片面マテリアル: 縮退三角形を出力してここで裏面をカリングする。
		///      doubleSided マテリアルは両面を残す。
		if (instance.double_sided_ == 0 && IsBackFace(clip_positions[i0], clip_positions[i1], clip_positions[i2]))
		{
			tris[triangle_index] = uint3(0, 0, 0);
		}
		else
		{
			tris[triangle_index] = uint3(i0, i1, i2);
		}
	}
}
