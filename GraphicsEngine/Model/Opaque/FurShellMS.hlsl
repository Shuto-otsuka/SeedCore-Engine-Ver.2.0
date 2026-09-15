#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Culling.hlsli"

// Mesh Shader for the shell-fur forward pass. One thread group per surviving
// meshlet of one shell layer (the shell index rides in the payload). Each
// vertex is pushed out along its normal by pow(shell_ratio, k) * fur_length_;
// the PS clips per-strand and shades with the Kajiya-Kay fur model.
//
// Static (non-skinned) instances only for now - the FurShellAS already rejects
// skinned ones. Max output: 64 vertices, 124 triangles.

groupshared float4 clip_positions[64];

[NumThreads(64, 1, 1)]
[OutputTopology("triangle")]
void main(in payload FurShellPayload as_payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices FurShellMSOutput verts[64], out indices uint3 tris[124])
{
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);

	uint meshlet_index = as_payload.meshlet_indices[gid];
	uint instance_index = as_payload.instance_index;
	uint shell_index = as_payload.shell_index;
	ModelStructuredBuffer instance = instances[instance_index];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	SetMeshOutputCounts(meshlet.vertex_count_, meshlet.triangle_count_);

	float shell_ratio = instance.shading_.fur_shell_count_ > 1 ? (float)shell_index / (float)instance.shading_.fur_shell_count_ : 0.0;
	// shells bunch towards the base (more coverage near the skin), and droop a
	// little with the world-down axis so the coat is not a rigid extrusion.
	float shell_height = pow(shell_ratio, 1.35) * instance.shading_.fur_length_;
	float3 droop = float3(0.0, -1.0, 0.0) * shell_ratio * shell_ratio * instance.shading_.fur_length_ * 0.35;

	if (gtid < meshlet.vertex_count_)
	{
		uint global_vertex_index = vertex_indices[meshlet.vertex_offset_ + gtid];
		ExpandedModelVertex vertex = DecodeModelVertex(vertices[global_vertex_index], instance);
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

		float3 offset_local = normalize(vertex.normal_) * shell_height;
		float4 world_position = mul(float4(vertex.position_ + offset_local, 1.0), instance.transform_.world_);
		world_position.xyz += droop;
		float4 clip_position = mul(world_position, scene.current_view_projection_);

		FurShellMSOutput output;
		output.position = clip_position;
		output.world_position = world_position.xyz;
		output.world_normal = normalize(mul(float4(vertex.normal_, 0.0), instance.transform_.inverse_transpose_world_).xyz);
		output.world_tangent = normalize(mul(float4(vertex.tangent_.xyz, 0.0), instance.transform_.world_).xyz);
		output.texcoord = vertex.texcoord_;
		output.instance_index = instance_index;
		output.shell_ratio = shell_ratio;

		verts[gtid] = output;
		clip_positions[gtid] = clip_position;
	}

	GroupMemoryBarrierWithGroupSync();

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
