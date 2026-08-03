#include "Image.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ImageASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ImageMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<ImageBillboardInstance> image_billboard = ResourceDescriptorHeap[structured_indices.sprite_.image_billboard_index_];

	SetMeshOutputCounts(4u, 2u);

	uint billboard_id = payload.image_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	ImageBillboardInstance billboard = image_billboard[billboard_id];

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

	Texture2D texture = ResourceDescriptorHeap[billboard.texture_index];
	float texture_width, texture_height;
	texture.GetDimensions(texture_width, texture_height);
	float2 texture_dimentions = float2(texture_width, texture_height);

	float2 uv_offset = float2(0.0f, 0.0f);
	if (billboard.motion_type == 1)
	{
		uv_offset = billboard.scroll_direction * billboard.scroll_speed * scene_constant.total_time_;
	}

	if (gtid < 4u)
	{
		float2 pivot_norm = (billboard.texture_size.x > 0.0f) ? (billboard.pivot / billboard.texture_size) : float2(0.5f, 0.5f);
		float2 local = offsets[gtid] + float2(0.5f - pivot_norm.x, pivot_norm.y - 0.5f);
		float3 scaled = float3(local.x * billboard.scale.x, local.y * billboard.scale.y, 0.0f);

		float3 world_position;
		if (billboard.face_camera != 0)
		{
			float3 camera_right = scene_constant.inverse_view_[0].xyz;
			float3 camera_up = scene_constant.inverse_view_[1].xyz;
			world_position = billboard.position + camera_right * scaled.x + camera_up * scaled.y;
		}
		else
		{
			float cx = cos(billboard.rotation.x); float sx = sin(billboard.rotation.x);
			float cy = cos(billboard.rotation.y); float sy = sin(billboard.rotation.y);
			float cz = cos(billboard.rotation.z); float sz = sin(billboard.rotation.z);

			float3 rotated;
			rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
			rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
			rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

			world_position = billboard.position + rotated;
		}

		output[gtid].position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
		output[gtid].uv = (billboard.texture_position + uvs[gtid] * billboard.texture_size) / texture_dimentions + uv_offset;
		output[gtid].color = billboard.color;
		output[gtid].texture_index = billboard.texture_index;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
