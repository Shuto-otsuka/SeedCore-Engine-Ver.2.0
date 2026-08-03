#include "Movie.hlsli"
#include "../Shader/Structured.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload MovieASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices MovieFullscreenMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<MovieFullscreenInstance> movie_fullscreen = ResourceDescriptorHeap[structured_indices.movie_.fullscreen_index_];

	SetMeshOutputCounts(4u, 2u);

	uint instance_id = payload.instance_indices[gid];
	MovieFullscreenInstance instance = movie_fullscreen[instance_id];

	float2 corners[4] =
	{
		float2(-1.0f,  1.0f),
		float2( 1.0f,  1.0f),
		float2(-1.0f, -1.0f),
		float2( 1.0f, -1.0f)
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
		output[gtid].position = float4(corners[gtid], 0.0f, 1.0f);
		output[gtid].uv = uvs[gtid];
		output[gtid].color = instance.color;
		output[gtid].texture_index = instance.texture_index;
		output[gtid].texture_aspect = instance.texture_aspect;
	}

	if (gtid == 0)
	{
		triangles[0] = uint3(0u, 1u, 2u);
		triangles[1] = uint3(1u, 3u, 2u);
	}
}
