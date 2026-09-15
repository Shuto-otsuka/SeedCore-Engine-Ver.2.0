#ifndef __COLLIDER_LINE_HLSL__
#define __COLLIDER_LINE_HLSL__

#include "../../Shader/Scene.hlsli"

#define COLLIDER_SHAPE_BOX 0
#define COLLIDER_SHAPE_SPHERE 1
#define COLLIDER_SHAPE_CAPSULE 2
#define COLLIDER_SHAPE_CYLINDER 3
#define COLLIDER_SHAPE_RECT 4
#define COLLIDER_SHAPE_CIRCLE 5
#define COLLIDER_SHAPE_CONE 6

struct ColliderStructuredBuffer
{
	float3 position_;
	uint shape_kind_;
	float4 rotation_;
	float3 dimensions_;
	float collider_structured_buffer_padding_0_;
	float4 color_;
};

StructuredBuffer<ColliderStructuredBuffer> GetColliderStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct ColliderConstantBuffer
{
	uint instance_buffer_index_;
	uint instance_count_;
	uint groups_per_instance_;
	uint sphere_edge_buffer_index_;

	uint sphere_edge_count_;
	uint hemisphere_edge_buffer_index_;
	uint hemisphere_edge_count_;
	uint collider_constant_buffer_padding_0_;
};

ConstantBuffer<ColliderConstantBuffer> GetColliderConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.collider_index_];
}

struct ColliderLineMSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
};

void GetBoxLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	uint2 edges[12] =
	{
		uint2(0, 1), uint2(0, 2), uint2(0, 4), uint2(1, 3),
		uint2(1, 5), uint2(2, 3), uint2(2, 6), uint2(3, 7),
		uint2(4, 5), uint2(4, 6), uint2(5, 7), uint2(6, 7)
	};

	uint2 edge = edges[line_index];
	a = float3((edge.x & 1) ? dimensions.x : -dimensions.x, (edge.x & 2) ? dimensions.y : -dimensions.y, (edge.x & 4) ? dimensions.z : -dimensions.z);
	b = float3((edge.y & 1) ? dimensions.x : -dimensions.x, (edge.y & 2) ? dimensions.y : -dimensions.y, (edge.y & 4) ? dimensions.z : -dimensions.z);
}

/// [EN] The cylindrical body shared by both the standalone Cylinder shape
///      and the Capsule's straight section: two rings (radius dimensions.x)
///      at y = -halfHeight/+halfHeight, plus a handful of vertical side
///      lines. dimensions.y is the half-height.
void GetCylinderLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	float r = dimensions.x;
	float h = dimensions.y;

	if (line_index < 32)
	{
		uint seg = line_index;
		float angle0 = (float(seg) / 32.0) * 6.28318530717959;
		float angle1 = (float(seg + 1) / 32.0) * 6.28318530717959;
		a = float3(r * cos(angle0), -h, r * sin(angle0));
		b = float3(r * cos(angle1), -h, r * sin(angle1));
	}
	else if (line_index < 2 * 32)
	{
		uint seg = line_index - 32;
		float angle0 = (float(seg) / 32.0) * 6.28318530717959;
		float angle1 = (float(seg + 1) / 32.0) * 6.28318530717959;
		a = float3(r * cos(angle0), h, r * sin(angle0));
		b = float3(r * cos(angle1), h, r * sin(angle1));
	}
	else
	{
		uint k = line_index - 2 * 32;
		float angle = (float(k) / 8.0) * 6.28318530717959;
		a = float3(r * cos(angle), -h, r * sin(angle));
		b = float3(r * cos(angle), h, r * sin(angle));
	}
}

/// [EN] Sphere geometry isn't generated procedurally - it's looked up from
///      ColliderRenderer's persistent icosphere edge table (built once on
///      the CPU to match JPH::DebugRenderer's own DrawWireSphere density),
///      scaled by dimensions.x (radius).
void GetSphereLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	StructuredBuffer<float3> edges = ResourceDescriptorHeap[GetColliderConstantBuffer().sphere_edge_buffer_index_];
	a = edges[line_index * 2 + 0] * dimensions.x;
	b = edges[line_index * 2 + 1] * dimensions.x;
}

/// [EN] Cylindrical body (procedural, see GetCylinderLine) plus two
///      hemispherical caps looked up from ColliderRenderer's persistent
///      hemisphere edge table (the y>=0 half of the same icosphere used for
///      Sphere) - the bottom cap reuses the same table mirrored in y.
///      dimensions = (radius, halfHeightOfCylinder, unused).
void GetCapsuleLine(float3 dimensions, uint line_index, out float3 a, out float3 b)
{
	if (line_index < 2 * 32 + 8)
	{
		GetCylinderLine(dimensions, line_index, a, b);
		return;
	}

	float r = dimensions.x;
	float h = dimensions.y;
	uint hemisphere_edge_count = GetColliderConstantBuffer().hemisphere_edge_count_;
	uint cap_line_index = line_index - (2 * 32 + 8);

	StructuredBuffer<float3> edges = ResourceDescriptorHeap[GetColliderConstantBuffer().hemisphere_edge_buffer_index_];

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

	if (line_index < 32)
	{
		uint seg = line_index;
		float angle0 = (float(seg) / 32.0) * 6.28318530717959;
		float angle1 = (float(seg + 1) / 32.0) * 6.28318530717959;
		a = float3(r * cos(angle0), -h, r * sin(angle0));
		b = float3(r * cos(angle1), -h, r * sin(angle1));
	}
	else
	{
		uint k = line_index - 32;
		float angle = (float(k) / 8.0) * 6.28318530717959;
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
	float angle0 = (float(line_index) / 32.0) * 6.28318530717959;
	float angle1 = (float(line_index + 1) / 32.0) * 6.28318530717959;
	a = float3(r * cos(angle0), r * sin(angle0), 0.0);
	b = float3(r * cos(angle1), r * sin(angle1), 0.0);
}

#endif // __COLLIDER_LINE_HLSL__
