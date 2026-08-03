#include "Font.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload FontASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices FontMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<FontBillboardInstance> font_billboard = ResourceDescriptorHeap[structured_indices.sprite_.font_billboard_index_];

	SetMeshOutputCounts(4u, 2u);

	uint glyph_id = payload.glyph_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	FontBillboardInstance glyph = font_billboard[glyph_id];

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
		float2 local = glyph.local_position + corners[gtid] * glyph.local_size;
		float3 scaled = float3(local.x, local.y, 0.0f);

		float3 world_position;
		if (glyph.face_camera != 0)
		{
			float3 camera_right = scene_constant.inverse_view_[0].xyz;
			float3 camera_up = scene_constant.inverse_view_[1].xyz;
			world_position = glyph.position + camera_right * scaled.x + camera_up * scaled.y;
		}
		else
		{
			float cx = cos(glyph.rotation.x); float sx = sin(glyph.rotation.x);
			float cy = cos(glyph.rotation.y); float sy = sin(glyph.rotation.y);
			float cz = cos(glyph.rotation.z); float sz = sin(glyph.rotation.z);

			float3 rotated;
			rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
			rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
			rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

			world_position = glyph.position + rotated;
		}

		output[gtid].position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
		// ローカルはy-up、テクスチャはy-downなのでvを反転して対応させる
		output[gtid].uv = float2(lerp(glyph.uv_min.x, glyph.uv_max.x, corners[gtid].x), lerp(glyph.uv_max.y, glyph.uv_min.y, corners[gtid].y));
		output[gtid].color = glyph.color;
		output[gtid].outline_color = glyph.outline_color;
		output[gtid].glow_color = glyph.glow_color;
		output[gtid].texture_index = glyph.texture_index;
		output[gtid].unit_range = glyph.unit_range;
		output[gtid].outline_width = glyph.outline_width;
		output[gtid].glow_power = glyph.glow_power;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
