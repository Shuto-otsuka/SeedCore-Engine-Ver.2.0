#include "Image.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ImageASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ImageMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<ImageSpriteInstance> image_sprite = ResourceDescriptorHeap[structured_indices.sprite_.image_index_];

	SetMeshOutputCounts(4u, 2u);

	uint sprite_id = payload.image_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	ImageSpriteInstance sprite = image_sprite[sprite_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(sprite.texture_size.x, 0.0f),
		float2(0.0f, sprite.texture_size.y),
		float2(sprite.texture_size.x, sprite.texture_size.y)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	Texture2D texture = ResourceDescriptorHeap[sprite.texture_index];
	float texture_width, texture_height;
	texture.GetDimensions(texture_width, texture_height);
	float2 texture_dimentions = float2(texture_width, texture_height);

	float2 uv_offset = float2(0.0f, 0.0f);
	if (sprite.motion_type == 1)
	{
		uv_offset = sprite.scroll_direction * sprite.scroll_speed * scene_constant.total_time_;
	}

	if (gtid < 4u)
	{
		float2 local = corners[gtid] - sprite.pivot;

		float cos_r = cos(sprite.rotation);
		float sin_r = sin(sprite.rotation);
		float2 rotated = float2(local.x * cos_r - local.y * sin_r, local.x * sin_r + local.y * cos_r);

		float2 world_pixel = rotated * sprite.scale + sprite.position;

		float2 clip;
		clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
		clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

		output[gtid].position = float4(clip, 0.0f, 1.0f);
		output[gtid].uv = (sprite.texture_position + uvs[gtid] * sprite.texture_size) / texture_dimentions + uv_offset;
		output[gtid].color = sprite.color;
		output[gtid].texture_index = sprite.texture_index;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
