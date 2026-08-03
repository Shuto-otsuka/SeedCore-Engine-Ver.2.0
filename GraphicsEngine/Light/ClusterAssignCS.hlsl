#include "../Shader/Constants.hlsli"
#include "Cluster.hlsli"
#include "../Shader/Light.hlsli"

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	ConstantBuffer<SceneConstantBuffer> scene = GetSceneConstantBuffer();
	ConstantBuffer<ClusterAssignConstantBuffer> cluster_constant = GetClusterConstantBuffer();

	uint total_lights = cluster_constant.point_light_count_ + cluster_constant.spot_light_count_ + cluster_constant.rect_light_count_;
	if (dtid.x >= total_lights)
	{
		return;
	}

	RWStructuredBuffer<ClusterData> cluster_data = ResourceDescriptorHeap[cluster_constant.cluster_data_unordered_access_view_index_];
	RWByteAddressBuffer light_index_list = ResourceDescriptorHeap[cluster_constant.cluster_light_list_unordered_access_view_index_];

	uint3 cluster_count = uint3(cluster_constant.cluster_count_x_, cluster_constant.cluster_count_y_, CLUSTER_DEPTH_SLICES);
	float near_plane = scene.near_plane_;
	float far_plane = scene.far_plane_;

	float3 light_position;
	float light_range;
	uint light_type;    // 0 = point, 1 = spot, 2 = rect
	uint local_index;   // index within the light's own array

	if (dtid.x < cluster_constant.point_light_count_)
	{
		local_index = dtid.x;
		StructuredBuffer<PointLightData> point_lights = ResourceDescriptorHeap[cluster_constant.point_light_shader_resource_view_index_];
		PointLightData point_light = point_lights[local_index];
		light_position = point_light.position;
		light_range = point_light.range;
		light_type = 0;
	}
	else if (dtid.x < cluster_constant.point_light_count_ + cluster_constant.spot_light_count_)
	{
		local_index = dtid.x - cluster_constant.point_light_count_;
		StructuredBuffer<SpotLightData> spot_lights = ResourceDescriptorHeap[cluster_constant.spot_light_shader_resource_view_index_];
		SpotLightData spot_light = spot_lights[local_index];
		light_position = spot_light.position;
		light_range = spot_light.range;
		light_type = 1;
	}
	else
	{
		local_index = dtid.x - cluster_constant.point_light_count_ - cluster_constant.spot_light_count_;
		StructuredBuffer<RectLightData> rect_lights = ResourceDescriptorHeap[cluster_constant.rect_light_shader_resource_view_index_];
		RectLightData rect_light = rect_lights[local_index];
		light_position = rect_light.position;
		light_range = rect_light.range;
		light_type = 2;
	}

	float4 view_position = mul(float4(light_position, 1.0), scene.view_);
	float view_z = view_position.z;

	float min_z = max(view_z - light_range, near_plane);
	float max_z = min(view_z + light_range, far_plane);
	if (min_z > far_plane || max_z < near_plane)
	{
		return;
	}

	uint min_slice = ComputeDepthSlice(min_z, near_plane, far_plane);
	uint max_slice = ComputeDepthSlice(max_z, near_plane, far_plane);

	float4 clip_position = mul(float4(view_position.xyz, 1.0), scene.projection_);
	float2 ndc = clip_position.xy / max(abs(clip_position.w), 0.0001);
	float2 screen_center = (ndc * float2(0.5, -0.5) + 0.5) * scene.screen_size_;

	float proj_scale = scene.projection_[0][0] * scene.screen_size_.x * 0.5;
	float screen_radius = light_range * proj_scale / max(abs(view_z), 0.001);

	int2 min_tile = int2(max(screen_center - screen_radius, float2(0, 0)) / float(CLUSTER_TILE_SIZE));
	int2 max_tile = int2(min(screen_center + screen_radius, scene.screen_size_ - 1.0) / float(CLUSTER_TILE_SIZE));

	min_tile = clamp(min_tile, int2(0, 0), int2(cluster_count.xy) - 1);
	max_tile = clamp(max_tile, int2(0, 0), int2(cluster_count.xy) - 1);

	for (uint z = min_slice; z <= max_slice; z++)
	{
		for (int y = min_tile.y; y <= max_tile.y; y++)
		{
			for (int x = min_tile.x; x <= max_tile.x; x++)
			{
				uint cluster_index = ClusterIndex(uint3(x, y, z), cluster_count);
				uint base = cluster_index * CLUSTER_STRIDE;

				if (light_type == 0)
				{
					uint slot;
					InterlockedAdd(cluster_data[cluster_index].point_count_, 1, slot);
					if (slot < CLUSTER_MAX_POINT_LIGHTS)
					{
						light_index_list.Store((base + slot) * 4, local_index);
					}
				}
				else if (light_type == 1)
				{
					uint slot;
					InterlockedAdd(cluster_data[cluster_index].spot_count_, 1, slot);
					if (slot < CLUSTER_MAX_SPOT_LIGHTS)
					{
						light_index_list.Store((base + CLUSTER_MAX_POINT_LIGHTS + slot) * 4, local_index);
					}
				}
				else
				{
					uint slot;
					InterlockedAdd(cluster_data[cluster_index].rect_count_, 1, slot);
					if (slot < CLUSTER_MAX_RECT_LIGHTS)
					{
						light_index_list.Store((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + slot) * 4, local_index);
					}
				}
			}
		}
	}
}
