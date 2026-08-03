#include "Movie.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Constants.hlsli"

Texture2D textures[] : register(t0);

float4 main(MovieFullscreenMSOutput input) : SV_Target
{
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();

	float screen_aspect = scene_constant.screen_size_.x / scene_constant.screen_size_.y;

	float2 half_size;
	if (input.texture_aspect > screen_aspect)
	{
		half_size.x = 0.5f;
		half_size.y = half_size.x * screen_aspect / input.texture_aspect;
	}
	else
	{
		half_size.y = 0.5f;
		half_size.x = half_size.y * input.texture_aspect / screen_aspect;
	}

	float2 center = float2(0.5f, 0.5f);
	float2 min_uv = center - half_size;
	float2 max_uv = center + half_size;

	if (input.uv.x < min_uv.x || input.uv.x > max_uv.x || input.uv.y < min_uv.y || input.uv.y > max_uv.y)
	{
		return float4(0.0f, 0.0f, 0.0f, input.color.a);
	}

	float2 texture_coord = (input.uv - min_uv) / (max_uv - min_uv);
	float4 texture_color = textures[input.texture_index].Sample(sampler_linear_clamp, texture_coord);

	return float4(texture_color.rgb * input.color.rgb, input.color.a);
}
