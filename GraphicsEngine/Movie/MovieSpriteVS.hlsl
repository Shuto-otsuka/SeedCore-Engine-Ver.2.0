#include "Movie.hlsli"
#include "../Shader/ShaderResources.hlsli"
#include "../Shader/Culling.hlsli"

MovieMSOutput main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	StructuredBuffer<MovieSpriteStructuredBuffer> movie_sprite = GetMovieSpriteStructuredBuffer(shader_resource_indices.movie_.sprite_index_);
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	MovieSpriteStructuredBuffer instance = movie_sprite[instance_id];

	MovieMSOutput output = (MovieMSOutput)0;
	output.position = float4(2.0f, 2.0f, 2.0f, 1.0f);

	bool is_visible = false;
	if (instance.size_.x > 0.0 && instance.size_.y > 0.0)
	{
		is_visible = IsVisibleInScreen(instance.position_, instance.size_, scene_constant.screen_size_);
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
		float2(instance.size_.x, 0.0f),
		float2(0.0f, instance.size_.y),
		float2(instance.size_.x, instance.size_.y)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	float2 local = corners[corner_index] - instance.pivot_;

	float cos_r = cos(instance.rotation_);
	float sin_r = sin(instance.rotation_);
	float2 rotated = float2(local.x * cos_r - local.y * sin_r, local.x * sin_r + local.y * cos_r);

	float2 world_pixel = rotated * instance.scale_ + instance.position_;

	float2 clip;
	clip.x = world_pixel.x / scene_constant.display_size_.x * 2.0f - 1.0f;
	clip.y = 1.0f - world_pixel.y / scene_constant.display_size_.y * 2.0f;

	output.position = float4(clip, 0.0f, 1.0f);
	output.uv = uvs[corner_index];
	output.color = instance.color_;
	output.texture_index = instance.texture_index_;

	return output;
}
