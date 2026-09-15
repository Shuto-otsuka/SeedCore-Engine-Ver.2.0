#include "../Font.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Scene.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload FontASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices FontMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<FontBillboardStructuredBuffer> font_billboard = GetFontBillboardStructuredBuffer(shader_resource_indices.font_.billboard_index_);

	SetMeshOutputCounts(4u, 2u);

	uint glyph_id = payload.glyph_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	FontBillboardStructuredBuffer glyph = font_billboard[glyph_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	if (gtid < 4u)
	{
		// ローカル平面(x:右, y:上)にグリフ矩形を配置
		float2 local = glyph.local_position_ + corners[gtid] * glyph.local_size_;
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
			float cx = cos(glyph.rotation_.x); float sx = sin(glyph.rotation_.x);
			float cy = cos(glyph.rotation_.y); float sy = sin(glyph.rotation_.y);
			float cz = cos(glyph.rotation_.z); float sz = sin(glyph.rotation_.z);

			float3 rotated;
			rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
			rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
			rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

			world_position = glyph.position_ + rotated;
		}

		output[gtid].position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
		// ローカルはy-up、テクスチャはy-downなのでvを反転して対応させる
		output[gtid].uv = float2(lerp(glyph.uv_min_.x, glyph.uv_max_.x, corners[gtid].x), lerp(glyph.uv_max_.y, glyph.uv_min_.y, corners[gtid].y));
		output[gtid].color = glyph.color_;
		output[gtid].outline_color = glyph.outline_color_;
		output[gtid].glow_color = glyph.glow_color_;
		output[gtid].texture_index = glyph.texture_index_;
		output[gtid].unit_range = glyph.unit_range_;
		output[gtid].outline_width = glyph.outline_width_;
		output[gtid].glow_power = glyph.glow_power_;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
