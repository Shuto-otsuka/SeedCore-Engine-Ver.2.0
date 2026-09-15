#include "Image.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Scene.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload ImageASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices ImageMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<ImageSpriteStructuredBuffer> image_sprite = GetImageSpriteStructuredBuffer(shader_resource_indices.image_.sprite_index_);

	SetMeshOutputCounts(4u, 2u);

	uint sprite_id = payload.image_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	ImageSpriteStructuredBuffer sprite = image_sprite[sprite_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(sprite.texture_size_.x, 0.0f),
		float2(0.0f, sprite.texture_size_.y),
		float2(sprite.texture_size_.x, sprite.texture_size_.y)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	Texture2D texture = ResourceDescriptorHeap[sprite.texture_index_];
	float texture_width, texture_height;
	texture.GetDimensions(texture_width, texture_height);
	float2 texture_dimentions = float2(texture_width, texture_height);

	float2 uv_offset = float2(0.0f, 0.0f);
	if (sprite.motion_type_ == 1)
	{
		uv_offset = sprite.scroll_direction_ * sprite.scroll_speed_ * scene_constant.total_time_;
	}

	if (gtid < 4u)
	{
		float2 local = corners[gtid] - sprite.pivot_;

		float cos_r = cos(sprite.rotation_);
		float sin_r = sin(sprite.rotation_);
		float2 rotated = float2(local.x * cos_r - local.y * sin_r, local.x * sin_r + local.y * cos_r);

		float2 world_pixel = rotated * sprite.scale_ + sprite.position_;

		float2 clip;
		clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
		clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

		output[gtid].position = float4(clip, 0.0f, 1.0f);
		output[gtid].uv = (sprite.texture_position_ + uvs[gtid] * sprite.texture_size_) / texture_dimentions + uv_offset;
		output[gtid].color = sprite.color_;
		output[gtid].texture_index = sprite.texture_index_;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
