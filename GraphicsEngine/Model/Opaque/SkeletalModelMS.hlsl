#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Culling.hlsli"

/**
* [EN]
* Reference: https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html
*
* Mesh Shader for skeletal (skinned) model rendering.
*
* Same structure as StaticModelMS, but applies linear blend skinning
* before the world transform. Each vertex blends up to 4 bone matrices
* using joints/weights stored in the vertex data.
*
* Bone matrices are read from a StructuredBuffer indexed by
* (instance.bone_offset + joint_index).
*
* ---------------------------------------------------------------------
*
* [JP]
* スケルタル（スキニング付き）モデルレンダリング用の Mesh Shader。
*
* StaticModelMS と同じ構造だが、ワールド変換の前にリニアブレンド
* スキニングを適用する。各頂点は頂点データに格納されたジョイント/ウェイトを
* 使い、最大 4 つのボーン行列をブレンドする。
*
* ボーン行列は (instance.bone_offset + joint_index) でインデックスされる
* StructuredBuffer から読み取る。
*/
groupshared float4 clip_positions[64];

[NumThreads(64, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ModelASPayload as_payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ModelMSOutput verts[64], out indices uint3 tris[124], out primitives ModelMSPrimitiveOutput prims[124])
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.bone_matrix_index_];

	uint meshlet_index = as_payload.meshlet_indices[gid];
	uint instance_index = as_payload.instance_index;
	ModelStructuredBuffer instance = instances[instance_index];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [EN] The AS dispatches every instance to both the static and skeletal
	///      pipelines; non-skinned instances are drawn by StaticModelMS, so emit
	///      nothing here. Folded into the counts because the DXIL validator
	///      forbids more than one SetMeshOutputCounts call site (branch is
	///      wave-uniform: instance comes from the payload).
	/// [JP] AS は全インスタンスを static / skeletal 両パイプラインにディスパッチする。
	///      非スキンインスタンスは StaticModelMS が描画するため、ここでは何も
	///      出力しない。DXIL バリデータが SetMeshOutputCounts の複数 call site を
	///      禁止しているため、カウントに畳み込む（instance はペイロード由来なので
	///      分岐は wave-uniform）。
	bool skip = instance.skining_.skin_index_ == 0xFFFFFFFF;
	SetMeshOutputCounts(skip ? 0 : meshlet.vertex_count_, skip ? 0 : meshlet.triangle_count_);
	if (skip)
	{
		return;
	}

	if (gtid < meshlet.vertex_count_)
	{
		uint global_vertex_index = vertex_indices[meshlet.vertex_offset_ + gtid];
		ExpandedModelVertex vertex = DecodeModelVertex(vertices[global_vertex_index], instance);

		/// [EN] Morph composes before skin (matches the RT compute path -
		///      see MorphBlendCS.hlsl).
		/// [JP] モーフはスキンより前に合成する(RT のコンピュートパスと
		///      同じ順序 - MorphBlendCS.hlsl 参照)。
		if (instance.morph_.morph_target_count_ != 0)
		{
			StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.morph_.vertex_morph_source_buffer_index_];
			StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_.morph_delta_buffer_index_];
			StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.morph_weight_index_];

			uint local_vertex_index = vertex_morph_source[global_vertex_index] - instance.morph_.morph_vertex_offset_;
			for (uint target = 0; target < instance.morph_.morph_target_count_; ++target)
			{
				vertex.position_ += morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index] * morph_weights[instance.morph_.morph_weight_offset_ + target];
			}
		}

		StructuredBuffer<CompressedModelSkin> skin_vertices = ResourceDescriptorHeap[instance.skining_.skin_vertex_buffer_index_];
		ExpandedModelSkin skin = DecodeSkinVertex(skin_vertices[global_vertex_index]);

		/// [EN] Linear blend skinning: blend up to 4 bone matrices weighted by vertex weights.
		/// [JP] リニアブレンドスキニング: 頂点ウェイトで最大 4 つのボーン行列をブレンドする。
		float4x4 skin_matrix =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.x]) * skin.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.y]) * skin.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.z]) * skin.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.w]) * skin.weights_.w;

		/// [JP] PS へ渡すのは position + texcoord + id 系のみでよいので、法線/
		///      タンジェントのスキニングはしない(Model/MaterialResolveCS.hlsl が
		///      その id + depth から計算し直す)。position のスキニングは
		///      描画される輪郭/深度そのものに関わるため引き続き必須。
		float3 skinned_position = mul(float4(vertex.position_, 1.0), skin_matrix).xyz;

		float4 world_position = mul(float4(skinned_position, 1.0), instance.transform_.world_);
		float4 clip_position = mul(world_position, scene.current_view_projection_);

		ModelMSOutput output;
		output.position = clip_position;
		output.texcoord = vertex.texcoord_;
		output.instance_index = instance_index;
		output.meshlet_index = meshlet_index;

		verts[gtid] = output;
		clip_positions[gtid] = clip_position;
	}

	GroupMemoryBarrierWithGroupSync();

	/// [EN] Up to 124 triangles per meshlet but only 64 threads — each thread
	///      loops so every declared primitive gets written (unwritten primitives
	///      are uninitialised and rasterize as frame-varying garbage).
	/// [JP] メシュレットあたり最大 124 三角形に対しスレッドは 64 本のみ — 全宣言
	///      プリミティブが書かれるよう各スレッドがループする（未書き込みの
	///      プリミティブは未初期化で、フレームごとに変わるゴミとして描画される）。
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

		if (instance.shading_.double_sided_ == 0 && IsBackFace(clip_positions[i0], clip_positions[i1], clip_positions[i2]))
		{
			tris[triangle_index] = uint3(0, 0, 0);
		}
		else
		{
			tris[triangle_index] = uint3(i0, i1, i2);
		}

		prims[triangle_index].triangle_in_meshlet_index = triangle_index;
	}
}
