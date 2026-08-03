#include "Movie.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload MovieASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices MovieMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<MovieSpriteInstance> movie_sprite = ResourceDescriptorHeap[structured_indices.movie_.sprite_index_];

	SetMeshOutputCounts(4u, 2u);

	uint instance_id = payload.instance_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	MovieSpriteInstance instance = movie_sprite[instance_id];

	float2 corners[4] =
	{
		float2(0.0f, 0.0f),
		float2(instance.size.x, 0.0f),
		float2(0.0f, instance.size.y),
		float2(instance.size.x, instance.size.y)
	};

	float2 uvs[4] =
	{
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f)
	};

	if (gtid < 4u)
	{
		float2 local = corners[gtid] - instance.pivot;

		float cos_r = cos(instance.rotation);
		float sin_r = sin(instance.rotation);
		float2 rotated = float2(local.x * cos_r - local.y * sin_r, local.x * sin_r + local.y * cos_r);

		float2 world_pixel = rotated * instance.scale + instance.position;

		float2 clip;
		clip.x = world_pixel.x * scene_constant.inverse_screen_size_.x * 2.0f - 1.0f;
		clip.y = 1.0f - world_pixel.y * scene_constant.inverse_screen_size_.y * 2.0f;

		output[gtid].position = float4(clip, 0.0f, 1.0f);
		output[gtid].uv = uvs[gtid];
		output[gtid].color = instance.color;
		output[gtid].texture_index = instance.texture_index;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
