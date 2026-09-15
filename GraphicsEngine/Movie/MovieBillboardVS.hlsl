#include "Movie.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

MovieMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<MovieBillboardStructuredBuffer> movie_billboard = GetMovieBillboardStructuredBuffer(shader_resource_indices.movie_.billboard_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	MovieBillboardStructuredBuffer instance = movie_billboard[instance_id];

	MovieMSOutput output = (MovieMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	bool is_visible = false;
	if (instance.scale_.x > 0.0 && instance.scale_.y > 0.0)
	{
		float radius = max(instance.scale_.x, instance.scale_.y) * 0.5f;
		is_visible = IsVisibleInFrustum(instance.position_, radius, scene_constant.current_view_projection_);
	}
	if (!is_visible)
	{
		return output;
	}

	uint quad_indices[6] = { 0u, 1u, 2u, 1u, 3u, 2u };
	uint corner_index = quad_indices[vertex_id];

	float2 offsets[4] =
	{
		float2(-0.5f,  0.5f),
		float2( 0.5f,  0.5f),
		float2(-0.5f, -0.5f),
		float2( 0.5f, -0.5f)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	float2 pivot_norm = (instance.size_.x > 0.0f) ? (instance.pivot_ / instance.size_) : float2(0.5f, 0.5f);
	float2 local = offsets[corner_index] + float2(0.5f - pivot_norm.x, pivot_norm.y - 0.5f);
	float3 scaled = float3(local.x * instance.scale_.x, local.y * instance.scale_.y, 0.0f);

	float3 world_position;
	if (instance.face_camera_ != 0)
	{
		float3 camera_right = scene_constant.inverse_view_[0].xyz;
		float3 camera_up = scene_constant.inverse_view_[1].xyz;
		world_position = instance.position_ + camera_right * scaled.x + camera_up * scaled.y;
	}
	else
	{
		float cx = cos(instance.rotation_.x); float sx = sin(instance.rotation_.x);
		float cy = cos(instance.rotation_.y); float sy = sin(instance.rotation_.y);
		float cz = cos(instance.rotation_.z); float sz = sin(instance.rotation_.z);

		float3 rotated;
		rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
		rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
		rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

		world_position = instance.position_ + rotated;
	}

	output.position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
	output.uv = uvs[corner_index];
	output.color = instance.color_;
	output.texture_index = instance.texture_index_;

	return output;
}
