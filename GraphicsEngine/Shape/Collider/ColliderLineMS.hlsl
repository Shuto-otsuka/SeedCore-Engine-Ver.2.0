#include "ColliderLine.hlsli"

/// [EN] One collider instance spans ColliderConstantBuffer::
///      groups_per_instance_ groups (a Jolt-density sphere/capsule can need
///      well over a thousand lines — far more than a single group's 128
///      lines) — gid decomposes into which instance and which 128-line
///      slice of that instance's line list this group covers.
/// [JP] 1つのコライダーインスタンスは ColliderConstantBuffer::
///      groups_per_instance_ 個のグループにまたがる(Jolt本家相当密度の
///      球/カプセルは1グループの128本を大きく超えうるため) — gid を
///      「どのインスタンスか」と「そのインスタンスの線リストのうち
///      128本単位のどのスライスか」に分解する。
[NumThreads(128, 1, 1)]
[OutputTopology("line")]
void main(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ColliderLineMSOutput verts[256], out indices uint2 lines[128])
{
	ColliderConstantBuffer collider = GetColliderConstantBuffer();

	uint groups_per_instance = collider.groups_per_instance_;
	uint instance_index = gid / groups_per_instance;
	uint sub_group_index = gid % groups_per_instance;
	uint line_index = sub_group_index * 128 + gtid;

	uint group_line_count = 0;
	uint shape_kind = 0;
	float3 position = float3(0.0, 0.0, 0.0);
	float4 rotation = float4(0.0, 0.0, 0.0, 1.0);
	float3 dimensions = float3(0.0, 0.0, 0.0);
	float4 color = float4(0.0, 0.0, 0.0, 0.0);

	if (instance_index < collider.instance_count_)
	{
		StructuredBuffer<ColliderStructuredBuffer> instances = GetColliderStructuredBuffer(collider.instance_buffer_index_);
		ColliderStructuredBuffer instance = instances[instance_index];
		shape_kind = instance.shape_kind_;
		position = instance.position_;
		rotation = instance.rotation_;
		dimensions = instance.dimensions_;
		color = instance.color_;

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

		uint sub_group_base = sub_group_index * 128;
		if (sub_group_base < total_lines)
		{
			group_line_count = min(128, total_lines - sub_group_base);
		}
	}

	SetMeshOutputCounts(group_line_count * 2, group_line_count);

	if (gtid < group_line_count)
	{
		SceneConstantBuffer scene = GetSceneConstantBuffer();

		float3 local_a = float3(0.0, 0.0, 0.0);
		float3 local_b = float3(0.0, 0.0, 0.0);
		if (shape_kind == COLLIDER_SHAPE_BOX)
		{
			GetBoxLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_SPHERE)
		{
			GetSphereLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_CAPSULE)
		{
			GetCapsuleLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_CYLINDER)
		{
			GetCylinderLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_RECT)
		{
			GetRectLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_CIRCLE)
		{
			GetCircleLine(dimensions, line_index, local_a, local_b);
		}
		else if (shape_kind == COLLIDER_SHAPE_CONE)
		{
			GetConeLine(dimensions, line_index, local_a, local_b);
		}

		float3 rotation_a = 2.0 * cross(rotation.xyz, local_a);
		float3 world_a = local_a + rotation.w * rotation_a + cross(rotation.xyz, rotation_a) + position;

		float3 rotation_b = 2.0 * cross(rotation.xyz, local_b);
		float3 world_b = local_b + rotation.w * rotation_b + cross(rotation.xyz, rotation_b) + position;

		verts[gtid * 2 + 0].position_ = mul(float4(world_a, 1.0), scene.current_view_projection_);
		verts[gtid * 2 + 0].color_ = color;

		verts[gtid * 2 + 1].position_ = mul(float4(world_b, 1.0), scene.current_view_projection_);
		verts[gtid * 2 + 1].color_ = color;

		lines[gtid] = uint2(gtid * 2 + 0, gtid * 2 + 1);
	}
}
