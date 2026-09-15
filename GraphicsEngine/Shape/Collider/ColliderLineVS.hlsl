#include "ColliderLine.hlsli"

ColliderLineMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	ColliderConstantBuffer collider = GetColliderConstantBuffer();

	ColliderLineMSOutput output = (ColliderLineMSOutput)0;
	output.position_ = float4(2.0, 2.0, 2.0, 1.0);

	if (instance_id >= collider.instance_count_)
	{
		return output;
	}

	StructuredBuffer<ColliderStructuredBuffer> instances = GetColliderStructuredBuffer(collider.instance_buffer_index_);
	ColliderStructuredBuffer instance = instances[instance_id];

	uint shape_kind = instance.shape_kind_;
	uint line_index = vertex_id / 2;

	uint total_lines = 0;
	if (shape_kind == COLLIDER_SHAPE_BOX)
	{
		total_lines = 12;
	}
	else if (shape_kind == COLLIDER_SHAPE_SPHERE)
	{
		total_lines = collider.sphere_edge_count_;
	}
	else if (shape_kind == COLLIDER_SHAPE_CAPSULE)
	{
		total_lines = 2 * 32 + 8 + 2 * collider.hemisphere_edge_count_;
	}
	else if (shape_kind == COLLIDER_SHAPE_CYLINDER)
	{
		total_lines = 2 * 32 + 8;
	}
	else if (shape_kind == COLLIDER_SHAPE_RECT)
	{
		total_lines = 4;
	}
	else if (shape_kind == COLLIDER_SHAPE_CIRCLE)
	{
		total_lines = 32;
	}
	else if (shape_kind == COLLIDER_SHAPE_CONE)
	{
		total_lines = 32 + 8;
	}

	if (line_index >= total_lines)
	{
		return output;
	}

	float3 local_a = float3(0.0, 0.0, 0.0);
	float3 local_b = float3(0.0, 0.0, 0.0);
	if (shape_kind == COLLIDER_SHAPE_BOX)
	{
		GetBoxLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_SPHERE)
	{
		GetSphereLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_CAPSULE)
	{
		GetCapsuleLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_CYLINDER)
	{
		GetCylinderLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_RECT)
	{
		GetRectLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_CIRCLE)
	{
		GetCircleLine(instance.dimensions_, line_index, local_a, local_b);
	}
	else if (shape_kind == COLLIDER_SHAPE_CONE)
	{
		GetConeLine(instance.dimensions_, line_index, local_a, local_b);
	}

	float3 local_position = (vertex_id & 1) ? local_b : local_a;
	float3 rotation = 2.0 * cross(instance.rotation_.xyz, local_position);
	float3 world_position = local_position + instance.rotation_.w * rotation + cross(instance.rotation_.xyz, rotation) + instance.position_;

	output.position_ = mul(float4(world_position, 1.0), GetSceneConstantBuffer().current_view_projection_);
	output.color_ = instance.color_;

	return output;
}
