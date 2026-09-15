#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Scene.hlsli"

FurShellMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<ModelCullingStructuredBuffer> culling = GetModelCullingStructuredBuffer(dispatch_buffer_index_);
	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);

	ModelCullingStructuredBuffer visible = culling[instance_id];
	ModelStructuredBuffer instance = instances[visible.instance_index_];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[visible.meshlet_index_];
	uint triangle_index = vertex_id / 3;

	FurShellMSOutput output = (FurShellMSOutput)0;
	output.position = float4(2.0, 2.0, 2.0, 1.0);

	if (triangle_index >= meshlet.triangle_count_)
	{
		return output;
	}

	uint byte_offset = meshlet.triangle_offset_ + vertex_id;
	uint local_vertex_index = (primitive_indices.Load(byte_offset & ~3) >> ((byte_offset & 3) * 8)) & 0xFF;
	uint global_vertex_index = vertex_indices[meshlet.vertex_offset_ + local_vertex_index];

	ExpandedModelVertex vertex = DecodeModelVertex(vertices[global_vertex_index], instance);
	if (instance.morph_.morph_target_count_ != 0)
	{
		StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.morph_.vertex_morph_source_buffer_index_];
		StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_.morph_delta_buffer_index_];
		StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.morph_weight_index_];

		uint morph_vertex_index = vertex_morph_source[global_vertex_index] - instance.morph_.morph_vertex_offset_;
		for (uint target = 0; target < instance.morph_.morph_target_count_; ++target)
		{
			vertex.position_ += morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + morph_vertex_index] * morph_weights[instance.morph_.morph_weight_offset_ + target];
		}
	}

	float shell_ratio = instance.shading_.fur_shell_count_ > 1 ? (float)visible.shell_index_ / (float)instance.shading_.fur_shell_count_ : 0.0;
	float shell_height = pow(shell_ratio, 1.35) * instance.shading_.fur_length_;
	float3 droop = float3(0.0, -1.0, 0.0) * shell_ratio * shell_ratio * instance.shading_.fur_length_ * 0.35;

	float3 offset_local = normalize(vertex.normal_) * shell_height;
	float4 world_position = mul(float4(vertex.position_ + offset_local, 1.0), instance.transform_.world_);
	world_position.xyz += droop;

	output.position = mul(world_position, GetSceneConstantBuffer().current_view_projection_);
	output.world_position = world_position.xyz;
	output.world_normal = normalize(mul(float4(vertex.normal_, 0.0), instance.transform_.inverse_transpose_world_).xyz);
	output.world_tangent = normalize(mul(float4(vertex.tangent_.xyz, 0.0), instance.transform_.world_).xyz);
	output.texcoord = vertex.texcoord_;
	output.instance_index = visible.instance_index_;
	output.shell_ratio = shell_ratio;

	return output;
}
