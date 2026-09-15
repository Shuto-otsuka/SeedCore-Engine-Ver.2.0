#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared float4 clip_positions[64];

[NumThreads(64, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ModelASPayload as_payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices DepthPrepassOutput verts[64], out indices uint3 tris[124])
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);

	uint meshlet_index = as_payload.meshlet_indices[gid];
	uint instance_index = as_payload.instance_index;
	ModelStructuredBuffer instance = instances[instance_index];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	SetMeshOutputCounts(meshlet.vertex_count_, meshlet.triangle_count_);

	if (gtid < meshlet.vertex_count_)
	{
		uint global_vertex_index = vertex_indices[meshlet.vertex_offset_ + gtid];
		ExpandedModelVertex vertex = DecodeModelVertex(vertices[global_vertex_index], instance);

		/// [EN] The prepass draws static and skinned instances in one dispatch,
		///      so skinning is applied here to keep prepass depth identical to
		///      the G-Buffer pass (branch is wave-uniform).
		/// [JP] プリパスは static / skinned を 1 ディスパッチで描くため、
		///      G-Buffer パスと深度を一致させるようここでスキニングを適用する
		///      （分岐は wave-uniform）。
		/// [EN] Morph composes before skin (matches StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl/MaterialResolveCS.hlsl/ModelTransparentPS.hlsl) - this prepass's
		///      whole purpose is to match the G-Buffer pass's depth exactly
		///      (comment below), so it must apply the identical morph +
		///      skin composition.
		/// [JP] モーフはスキンより前に合成する(StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl/MaterialResolveCS.hlsl/ModelTransparentPS.hlsl と同じ) —
		///      このプリパスの存在意義は G-Buffer パスと深度を完全一致
		///      させること(下のコメント参照)なので、同一のモーフ+スキン
		///      合成を適用する必要がある。
		float3 local_position = vertex.position_;
		if (instance.morph_.morph_target_count_ != 0)
		{
			StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.morph_.vertex_morph_source_buffer_index_];
			StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_.morph_delta_buffer_index_];
			StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.morph_weight_index_];

			uint local_vertex_index = vertex_morph_source[global_vertex_index] - instance.morph_.morph_vertex_offset_;
			for (uint target = 0; target < instance.morph_.morph_target_count_; ++target)
			{
				local_position += morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index] * morph_weights[instance.morph_.morph_weight_offset_ + target];
			}
		}
		if (instance.skining_.skin_index_ != 0xFFFFFFFF)
		{
			StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.bone_matrix_index_];
			StructuredBuffer<CompressedModelSkin> skin_vertices = ResourceDescriptorHeap[instance.skining_.skin_vertex_buffer_index_];
			ExpandedModelSkin skin = DecodeSkinVertex(skin_vertices[global_vertex_index]);
			float4x4 skin_matrix =
				LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.x]) * skin.weights_.x +
				LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.y]) * skin.weights_.y +
				LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.z]) * skin.weights_.z +
				LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin.joints_.w]) * skin.weights_.w;
			local_position = mul(float4(local_position, 1.0), skin_matrix).xyz;
		}

		float4 world_position = mul(float4(local_position, 1.0), instance.transform_.world_);
		float4 clip_position = mul(world_position, scene.current_view_projection_);

		verts[gtid].position = clip_position;
		verts[gtid].texcoord = vertex.texcoord_;
		verts[gtid].instance_index = instance_index;
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
	}
}
