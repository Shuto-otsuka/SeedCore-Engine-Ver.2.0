#ifndef __COLLIDER_LINE_HLSL__
#define __COLLIDER_LINE_HLSL__

#include "../../Shader/Constants.hlsli"

static const uint collider_shape_box = 0;
static const uint collider_shape_sphere = 1;
static const uint collider_shape_capsule = 2;
static const uint collider_shape_cylinder = 3;
static const uint collider_shape_rect = 4;
static const uint collider_shape_circle = 5;
static const uint collider_shape_cone = 6;

struct ColliderInstance
{
	float3 position_;
	uint shape_kind_;
	float4 rotation_;
	float3 dimensions_;
	float collider_instance_padding_0_;
	float4 color_;
};

struct ColliderInstanceConstants
{
	uint instance_buffer_index_;
	uint instance_count_;
	uint groups_per_instance_;
	uint sphere_edge_buffer_index_;
	uint sphere_edge_count_;
	uint hemisphere_edge_buffer_index_;
	uint hemisphere_edge_count_;
	uint collider_instance_constants_padding_0_;
};
ConstantBuffer<ColliderInstanceConstants> collider_instance_constants : register(b0, space0);

struct ColliderLineMSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
};

float3 RotateVectorByQuaternion(float4 q, float3 v)
{
	float3 t = 2.0 * cross(q.xyz, v);
	return v + q.w * t + cross(q.xyz, t);
}

#endif // __COLLIDER_LINE_HLSL__
