#include "Font.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

FontMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<FontBillboardStructuredBuffer> font_billboard = GetFontBillboardStructuredBuffer(shader_resource_indices.font_.billboard_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	FontBillboardStructuredBuffer glyph = font_billboard[instance_id];

	FontMSOutput output = (FontMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	float cx = cos(glyph.rotation_.x); float sx = sin(glyph.rotation_.x);
	float cy = cos(glyph.rotation_.y); float sy = sin(glyph.rotation_.y);
	float cz = cos(glyph.rotation_.z); float sz = sin(glyph.rotation_.z);

	bool is_visible = false;
	if (glyph.selected_ != 0 && glyph.local_size_.x > 0.0 && glyph.local_size_.y > 0.0)
	{
		float2 center_local = glyph.local_position_ + glyph.local_size_ * 0.5f;
		float3 center_scaled = float3(center_local.x, center_local.y, 0.0f);

		float3 center_rotated;
		center_rotated.x = (cy * cz) * center_scaled.x + (sx * sy * cz - cx * sz) * center_scaled.y;
		center_rotated.y = (cy * sz) * center_scaled.x + (sx * sy * sz + cx * cz) * center_scaled.y;
		center_rotated.z = (-sy)     * center_scaled.x + (sx * cy)                * center_scaled.y;

		float3 world_center = glyph.position_ + center_rotated;
		float radius = max(glyph.local_size_.x, glyph.local_size_.y) * 0.5f;
		is_visible = IsVisibleInFrustum(world_center, radius, scene_constant.current_view_projection_);
	}
	if (!is_visible)
	{
		return output;
	}

	uint quad_indices[6] = { 0u, 1u, 2u, 1u, 3u, 2u };
	uint corner_index = quad_indices[vertex_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	float2 local = glyph.local_position_ + corners[corner_index] * glyph.local_size_;
	float3 scaled = float3(local.x, local.y, 0.0f);

	float3 world_position;
	if (glyph.face_camera_ != 0)
	{
		float3 camera_right = scene_constant.inverse_view_[0].xyz;
		float3 camera_up = scene_constant.inverse_view_[1].xyz;
		world_position = glyph.position_ + camera_right * scaled.x + camera_up * scaled.y;
	}
	else
	{
		float3 rotated;
		rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
		rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
		rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

		world_position = glyph.position_ + rotated;
	}

	output.position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
	output.uv = float2(lerp(glyph.uv_min_.x, glyph.uv_max_.x, corners[corner_index].x), lerp(glyph.uv_max_.y, glyph.uv_min_.y, corners[corner_index].y));
	output.color = glyph.color_;
	output.outline_color = glyph.outline_color_;
	output.glow_color = glyph.glow_color_;
	output.texture_index = glyph.texture_index_;
	output.unit_range = glyph.unit_range_;
	output.outline_width = glyph.outline_width_;
	output.glow_power = glyph.glow_power_;

	return output;
}
