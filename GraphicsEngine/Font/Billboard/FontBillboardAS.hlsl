#include "../Font.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/Culling.hlsli"

groupshared uint survived_count;
groupshared uint local_indices[32];
groupshared FontASPayload payload;

[numthreads(32, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<FontBillboardStructuredBuffer> font_billboard = GetFontBillboardStructuredBuffer(shader_resource_indices.font_.billboard_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	if (gtid.x == 0)
	{
		survived_count = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	bool is_visible = false;
	uint glyph_id = dtid.x;

	if (glyph_id < 65536)
	{
		FontBillboardStructuredBuffer glyph = font_billboard[glyph_id];

		if (glyph.local_size_.x > 0.0 && glyph.local_size_.y > 0.0)
		{
			float2 center_local = glyph.local_position_ + glyph.local_size_ * 0.5f;
			float3 scaled = float3(center_local.x, center_local.y, 0.0f);

			float cx = cos(glyph.rotation_.x); float sx = sin(glyph.rotation_.x);
			float cy = cos(glyph.rotation_.y); float sy = sin(glyph.rotation_.y);
			float cz = cos(glyph.rotation_.z); float sz = sin(glyph.rotation_.z);

			float3 rotated;
			rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y;
			rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y;
			rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y;

			float3 world_center = glyph.position_ + rotated;
			float radius = max(glyph.local_size_.x, glyph.local_size_.y) * 0.5f;
			is_visible = IsVisibleInFrustum(world_center, radius, scene_constant.current_view_projection_);
		}
	}

	if (is_visible)
	{
		uint slot;
		InterlockedAdd(survived_count, 1, slot);
		local_indices[slot] = glyph_id;
	}

	GroupMemoryBarrierWithGroupSync();

	if (gtid.x == 0)
	{
		for (uint index = 0; index < survived_count; ++index)
		{
			payload.glyph_indices[index] = local_indices[index];
		}
	}

	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(survived_count, 1, 1, payload);
}
