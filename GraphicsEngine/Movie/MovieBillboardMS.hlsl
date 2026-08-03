#include "Movie.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Constants.hlsli"

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void main(in payload MovieASPayload payload, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out vertices MovieMSOutput output[4], out indices uint3 triangles[2])
{
	StructuredBuffer<MovieBillboardInstance> movie_billboard = ResourceDescriptorHeap[structured_indices.movie_.billboard_index_];

	SetMeshOutputCounts(4u, 2u);

	uint instance_id = payload.instance_indices[gid];
	SceneConstantBuffer scene_constant = GetSceneConstantBuffer();
	MovieBillboardInstance instance = movie_billboard[instance_id];

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

	if (gtid < 4u)
	{
		float2 pivot_norm = (instance.size.x > 0.0f) ? (instance.pivot / instance.size) : float2(0.5f, 0.5f);
		float2 local = offsets[gtid] + float2(0.5f - pivot_norm.x, pivot_norm.y - 0.5f);
		float3 scaled = float3(local.x * instance.scale.x, local.y * instance.scale.y, 0.0f);

		float3 world_position;
		if (instance.face_camera != 0)
		{
			float3 camera_right = scene_constant.inverse_view_[0].xyz;
			float3 camera_up = scene_constant.inverse_view_[1].xyz;
			world_position = instance.position + camera_right * scaled.x + camera_up * scaled.y;
		}
		else
		{
			float cx = cos(instance.rotation.x); float sx = sin(instance.rotation.x);
			float cy = cos(instance.rotation.y); float sy = sin(instance.rotation.y);
			float cz = cos(instance.rotation.z); float sz = sin(instance.rotation.z);

			float3 rotated;
			rotated.x = (cy * cz) * scaled.x + (sx * sy * cz - cx * sz) * scaled.y + (cx * sy * cz + sx * sz) * scaled.z;
			rotated.y = (cy * sz) * scaled.x + (sx * sy * sz + cx * cz) * scaled.y + (cx * sy * sz - sx * cz) * scaled.z;
			rotated.z = (-sy)     * scaled.x + (sx * cy)                * scaled.y + (cx * cy)                * scaled.z;

			world_position = instance.position + rotated;
		}

		output[gtid].position = mul(float4(world_position, 1.0f), scene_constant.current_view_projection_);
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
