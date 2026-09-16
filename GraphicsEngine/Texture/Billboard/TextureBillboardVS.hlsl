#include "../Texture.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

TextureMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<TextureBillboardStructuredBuffer> texture_billboard = GetTextureBillboardStructuredBuffer(shader_resource_indices.texture_.billboard_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	TextureBillboardStructuredBuffer billboard = texture_billboard[instance_id];

	TextureMSOutput output = (TextureMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	bool is_visible = false;
	if (billboard.scale_.x > 0.0 && billboard.scale_.y > 0.0)
	{
		float radius = max(billboard.scale_.x, billboard.scale_.y) * 0.5f;
		is_visible = IsVisibleInFrustum(billboard.position_, radius, scene_constant.current_view_projection_);
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

	Texture2D texture = ResourceDescriptorHeap[billboard.texture_index_];
	float texture_width, texture_height;
	texture.GetDimensions(texture_width, texture_height);
	float2 texture_dimentions = float2(texture_width, texture_height);

	float2 uv_offset = float2(0.0f, 0.0f);
	if (billboard.motion_type_ == 1)
	{
		uv_offset = billboard.scroll_direction_ * billboard.scroll_speed_ * scene_constant.total_time_;
	}

	float2 pivot_norm = (billboard.texture_size_.x > 0.0f) ? (billboard.pivot_ / billboard.texture_size_) : float2(0.5f, 0.5f);
	float2 local = offsets[corner_index] + float2(0.5f - pivot_norm.x, pivot_norm.y - 0.5f);
	float3 scaled = float3(local.x * billboard.scale_.x, local.y * billboard.scale_.y, 0.0f);

	float3 world_position;
	if (billboard.face_camera_ != 0)
	{
		float3 camera_right = scene_constant.inverse_view_[0].xyz;
		float3 camera_up = scene_constant.inverse_view_[1].xyz;

		float cos_r = cos(billboard.rotation_.z);
		float sin_r = sin(billboard.rotation_.z);
		float roll_x = scaled.x * cos_r + scaled.y * sin_r;
		float roll_y = -scaled.x * sin_r + scaled.y * cos_r;

		world_position = billboard.position_ + camera_right * roll_x + camera_up * roll_y;
	}
	else
	{
		float cx = cos(billboard.rotation_.x); float sx = sin(billboard.rotation_.x);
		float cy = cos(billboard.rotation_.y); float sy = sin(billboard.rotation_.y);
		float cz = cos(billboard.rotation_.z); float sz = sin(billboard.rotation_.z);

		float3 rotated;
		rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
		rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
		rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

		world_position = billboard.position_ + rotated;
	}

	output.position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
	output.uv = (billboard.texture_position_ + uvs[corner_index] * billboard.texture_size_) / texture_dimentions + uv_offset;
	output.color = billboard.color_;
	output.texture_index = billboard.texture_index_;

	return output;
}
