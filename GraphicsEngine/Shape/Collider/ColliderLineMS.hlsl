#include "ColliderLine.hlsli"

static const float two_pi = 6.28318530717959f;

static const uint ring_seg = 32;
static const uint vertical_count = 8;

static const uint box_line_count = 12;
static const uint cylinder_body_line_count = 2 * ring_seg + vertical_count;
static const uint rect_line_count = 4;
static const uint circle_line_count = ring_seg;
static const uint cone_line_count = ring_seg + vertical_count;

static const uint threads_per_group = 128;

uint GetLineCount(uint shape_kind)
{
	if (shape_kind == collider_shape_box)
	{
		return box_line_count;
	}
	if (shape_kind == collider_shape_sphere)
	{
		return collider_instance_constants.sphere_edge_count_;
	}
	if (shape_kind == collider_shape_capsule)
	{
		return cylinder_body_line_count + 2 * collider_instance_constants.hemisphere_edge_count_;
	}
	if (shape_kind == collider_shape_cylinder)
	{
		return cylinder_body_line_count;
	}
	if (shape_kind == collider_shape_rect)
	{
		return rect_line_count;
	}
	if (shape_kind == collider_shape_circle)
	{
		return circle_line_count;
	}
	if (shape_kind == collider_shape_cone)
	{
		return cone_line_count;
	}
	return 0;
}

float3 BoxCorner(float3 half_extent, uint index)
{
	return float3(
		(index & 1) ? half_extent.x : -half_extent.x,
		(index & 2) ? half_extent.y : -half_extent.y,
		(index & 4) ? half_extent.z : -half_extent.z);
}

void GetBoxLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	uint2 edges[12] =
	{
		uint2(0, 1), uint2(0, 2), uint2(0, 4), uint2(1, 3),
		uint2(1, 5), uint2(2, 3), uint2(2, 6), uint2(3, 7),
		uint2(4, 5), uint2(4, 6), uint2(5, 7), uint2(6, 7)
	};

	uint2 edge = edges[line_index];
	a = BoxCorner(dimensions, edge.x);
	b = BoxCorner(dimensions, edge.y);
}

/// [EN] The cylindrical body shared by both the standalone Cylinder shape
///      and the Capsule's straight section: two rings (radius dimensions.x)
///      at y = -halfHeight/+halfHeight, plus a handful of vertical side
///      lines. dimensions.y is the half-height.
/// [JP] 単体の Cylinder 形状と Capsule の胴体部分が共有する円柱本体:
///      y = -halfHeight/+halfHeight の2つのリング(半径 dimensions.x)と、
///      縦の側面ラインいくつか。dimensions.y はハーフハイト。
void GetCylinderBodyLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	float r = dimensions.x;
	float h = dimensions.y;

	if (line_index < ring_seg)
	{
		uint seg = line_index;
		float angle0 = (float(seg) / float(ring_seg)) * two_pi;
		float angle1 = (float(seg + 1) / float(ring_seg)) * two_pi;
		a = float3(r * cos(angle0), -h, r * sin(angle0));
		b = float3(r * cos(angle1), -h, r * sin(angle1));
	}
	else if (line_index < 2 * ring_seg)
	{
		uint seg = line_index - ring_seg;
		float angle0 = (float(seg) / float(ring_seg)) * two_pi;
		float angle1 = (float(seg + 1) / float(ring_seg)) * two_pi;
		a = float3(r * cos(angle0), h, r * sin(angle0));
		b = float3(r * cos(angle1), h, r * sin(angle1));
	}
	else
	{
		uint k = line_index - 2 * ring_seg;
		float angle = (float(k) / float(vertical_count)) * two_pi;
		a = float3(r * cos(angle), -h, r * sin(angle));
		b = float3(r * cos(angle), h, r * sin(angle));
	}
}

/// [EN] Sphere geometry isn't generated procedurally — it's looked up from
///      ColliderRenderer's persistent icosphere edge table (built once on
///      the CPU to match JPH::DebugRenderer's own DrawWireSphere density),
///      scaled by dimensions.x (radius).
/// [JP] 球のジオメトリは手続き生成せず、ColliderRenderer が持つ持続的な
///      アイコスフィアのエッジテーブル(JPH::DebugRenderer自身の
///      DrawWireSphereと同じ密度になるようCPU側で一度だけ構築)から引く。
///      dimensions.x(半径)でスケールする。
void GetSphereLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	StructuredBuffer<float3> edges = ResourceDescriptorHeap[collider_instance_constants.sphere_edge_buffer_index_];
	a = edges[line_index * 2 + 0] * dimensions.x;
	b = edges[line_index * 2 + 1] * dimensions.x;
}

/// [EN] Cylindrical body (procedural, see GetCylinderBodyLine) plus two
///      hemispherical caps looked up from ColliderRenderer's persistent
///      hemisphere edge table (the y>=0 half of the same icosphere used for
///      Sphere) — the bottom cap reuses the same table mirrored in y.
///      dimensions = (radius, halfHeightOfCylinder, unused).
/// [JP] 円柱本体(手続き生成、GetCylinderBodyLine参照)＋ ColliderRenderer が
///      持つ持続的な半球エッジテーブル(Sphereと同じアイコスフィアの
///      y>=0側半分)から引く2つの半球キャップ — 下側キャップは同じ
///      テーブルをY反転して使い回す。dimensions = (半径, 円柱部分の
///      ハーフハイト, 未使用)。
void GetCapsuleLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	if (line_index < cylinder_body_line_count)
	{
		GetCylinderBodyLine(dimensions, line_index, a, b);
		return;
	}

	float r = dimensions.x;
	float h = dimensions.y;
	uint hemisphere_edge_count = collider_instance_constants.hemisphere_edge_count_;
	uint cap_line_index = line_index - cylinder_body_line_count;

	StructuredBuffer<float3> edges = ResourceDescriptorHeap[collider_instance_constants.hemisphere_edge_buffer_index_];

	if (cap_line_index < hemisphere_edge_count)
	{
		float3 dir_a = edges[cap_line_index * 2 + 0];
		float3 dir_b = edges[cap_line_index * 2 + 1];
		a = float3(dir_a.x * r, dir_a.y * r + h, dir_a.z * r);
		b = float3(dir_b.x * r, dir_b.y * r + h, dir_b.z * r);
	}
	else
	{
		uint bottom_line_index = cap_line_index - hemisphere_edge_count;
		float3 dir_a = edges[bottom_line_index * 2 + 0];
		float3 dir_b = edges[bottom_line_index * 2 + 1];
		a = float3(dir_a.x * r, -dir_a.y * r - h, dir_a.z * r);
		b = float3(dir_b.x * r, -dir_b.y * r - h, dir_b.z * r);
	}
}

void GetConeLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	float r = dimensions.x;
	float h = dimensions.y;

	if (line_index < ring_seg)
	{
		uint seg = line_index;
		float angle0 = (float(seg) / float(ring_seg)) * two_pi;
		float angle1 = (float(seg + 1) / float(ring_seg)) * two_pi;
		a = float3(r * cos(angle0), -h, r * sin(angle0));
		b = float3(r * cos(angle1), -h, r * sin(angle1));
	}
	else
	{
		uint k = line_index - ring_seg;
		float angle = (float(k) / float(vertical_count)) * two_pi;
		a = float3(r * cos(angle), -h, r * sin(angle));
		b = float3(0.0, h, 0.0);
	}
}

void GetRectLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	float hx = dimensions.x;
	float hy = dimensions.y;

	float3 corners[4] =
	{
		float3(-hx, -hy, 0.0), float3(hx, -hy, 0.0),
		float3(hx, hy, 0.0), float3(-hx, hy, 0.0)
	};

	a = corners[line_index];
	b = corners[(line_index + 1) % 4];
}

void GetCircleLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	float r = dimensions.x;
	float angle0 = (float(line_index) / float(ring_seg)) * two_pi;
	float angle1 = (float(line_index + 1) / float(ring_seg)) * two_pi;
	a = float3(r * cos(angle0), r * sin(angle0), 0.0);
	b = float3(r * cos(angle1), r * sin(angle1), 0.0);
}

void GetLocalLine(uint shape_kind, float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	a = float3(0.0, 0.0, 0.0);
	b = float3(0.0, 0.0, 0.0);

	if (shape_kind == collider_shape_box)
	{
		GetBoxLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_sphere)
	{
		GetSphereLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_capsule)
	{
		GetCapsuleLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_cylinder)
	{
		GetCylinderBodyLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_rect)
	{
		GetRectLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_circle)
	{
		GetCircleLine(dimensions, line_index, a, b);
	}
	else if (shape_kind == collider_shape_cone)
	{
		GetConeLine(dimensions, line_index, a, b);
	}
}

/// [EN] One collider instance now spans collider_instance_constants.
///      groups_per_instance_ groups (a Jolt-density sphere/capsule can need
///      well over a thousand lines — far more than a single group's
///      threads_per_group lines) — gid decomposes into which instance and
///      which threads_per_group-sized slice of that instance's line list
///      this group covers.
/// [JP] 1つのコライダーインスタンスは、いまや
///      collider_instance_constants.groups_per_instance_ 個のグループに
///      またがる(Jolt本家相当密度の球/カプセルは1グループの
///      threads_per_group本を大きく超えうるため) — gid を
///      「どのインスタンスか」と「そのインスタンスの線リストのうち
///      threads_per_group本単位のどのスライスか」に分解する。
[NumThreads(128, 1, 1)]
[OutputTopology("line")]
void main(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ColliderLineMSOutput verts[256], out indices uint2 lines[128])
{
	uint groups_per_instance = collider_instance_constants.groups_per_instance_;
	uint instance_index = gid / groups_per_instance;
	uint sub_group_index = gid % groups_per_instance;
	uint line_index = sub_group_index * threads_per_group + gtid;

	uint instance_count = collider_instance_constants.instance_count_;

	uint group_line_count = 0;
	uint shape_kind = 0;
	float3 position = float3(0.0, 0.0, 0.0);
	float4 rotation = float4(0.0, 0.0, 0.0, 1.0);
	float3 dimensions = float3(0.0, 0.0, 0.0);
	float4 color = float4(0.0, 0.0, 0.0, 0.0);

	if (instance_index < instance_count)
	{
		StructuredBuffer<ColliderInstance> instances = ResourceDescriptorHeap[collider_instance_constants.instance_buffer_index_];
		ColliderInstance instance = instances[instance_index];
		shape_kind = instance.shape_kind_;
		position = instance.position_;
		rotation = instance.rotation_;
		dimensions = instance.dimensions_;
		color = instance.color_;

		uint total_lines = GetLineCount(shape_kind);
		uint sub_group_base = sub_group_index * threads_per_group;
		if (sub_group_base < total_lines)
		{
			group_line_count = min(threads_per_group, total_lines - sub_group_base);
		}
	}

	SetMeshOutputCounts(group_line_count * 2, group_line_count);

	if (gtid < group_line_count)
	{
		SceneConstantBuffer scene = GetSceneConstantBuffer();

		float3 local_a, local_b;
		GetLocalLine(shape_kind, dimensions, line_index, local_a, local_b);

		float3 world_a = RotateVectorByQuaternion(rotation, local_a) + position;
		float3 world_b = RotateVectorByQuaternion(rotation, local_b) + position;

		verts[gtid * 2 + 0].position = mul(float4(world_a, 1.0), scene.current_view_projection_);
		verts[gtid * 2 + 0].color = color;

		verts[gtid * 2 + 1].position = mul(float4(world_b, 1.0), scene.current_view_projection_);
		verts[gtid * 2 + 1].color = color;

		lines[gtid] = uint2(gtid * 2 + 0, gtid * 2 + 1);
	}
}
