#include "Image.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

ImageMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<ImageSpriteStructuredBuffer> image_sprite = GetImageSpriteStructuredBuffer(shader_resource_indices.image_.sprite_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	ImageSpriteStructuredBuffer sprite = image_sprite[instance_id];

	ImageMSOutput output = (ImageMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	bool is_visible = false;
	if (sprite.scale_.x > 0.0 && sprite.scale_.y > 0.0)
	{
		is_visible = IsVisibleInScreen(sprite.position_, sprite.texture_size_ * sprite.scale_, scene_constant.screen_size_);
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

	float2 local = corners[corner_index] - sprite.pivot_;

	float cos_r = cos(sprite.rotation_);
	float sin_r = sin(sprite.rotation_);
	float2 rotated = float2(local.x * cos_r - local.y * sin_r, local.x * sin_r + local.y * cos_r);

	float2 world_pixel = rotated * sprite.scale_ + sprite.position_;

	float2 clip;
	clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
	clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

	output.position = float4(clip, 0.0f, 1.0f);
	output.uv = (sprite.texture_position_ + uvs[corner_index] * sprite.texture_size_) / texture_dimentions + uv_offset;
	output.color = sprite.color_;
	output.texture_index = sprite.texture_index_;

	return output;
}
