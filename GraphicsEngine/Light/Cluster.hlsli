#ifndef __CLUSTER_HLSL__
#define __CLUSTER_HLSL__

#include "../Shader/Constants.hlsli"

#define CLUSTER_TILE_SIZE 64
#define CLUSTER_DEPTH_SLICES 16
#define CLUSTER_MAX_POINT_LIGHTS 64
#define CLUSTER_MAX_SPOT_LIGHTS 64
#define CLUSTER_MAX_RECT_LIGHTS 64
#define CLUSTER_STRIDE (CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + CLUSTER_MAX_RECT_LIGHTS)

/**
* [EN]
* Per-view cluster-light-assignment tuning (2 rows / 32 bytes). Values only
* - the UAV/SRV indices for the cluster buffers live in
* ClusterAssignUnorderedAccessIndices/ClusterAssignShaderResourceIndices
* below, same split as ConstantIndices/ShaderResourceIndices/
* UnorderedAccessIndices elsewhere.
*/
struct ClusterAssignConstantBuffer
{
	uint point_light_count_;
	uint spot_light_count_;
	uint rect_light_count_;
	uint total_clusters_;

	uint cluster_count_x_;
	uint cluster_count_y_;
	float near_plane_;
	float far_plane_;
};

ConstantBuffer<ClusterAssignConstantBuffer> GetClusterConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.cluster_assign_index_];
}

/**
* [EN]
* Read views of the point/spot/rect light arrays ClusterAssignCS.hlsl bins
* into the cluster grid.
*/
struct ClusterAssignShaderResourceIndices
{
	uint point_light_index_;
	uint spot_light_index_;
	uint rect_light_index_;
	uint cluster_assign_shader_resource_padding_0_;
};

/**
* [EN]
* Write targets of ClusterAssignCS.hlsl: the per-cluster light-count/offset
* header (ClusterInstance) and the flat light-index list it points into.
*/
struct ClusterAssignUnorderedAccessIndices
{
	uint cluster_data_index_;
	uint cluster_light_list_index_;
	float2 cluster_assign_unordered_access_padding_0_;
};

/**
* [EN]
* Read views of the point/spot/rect light arrays and the cluster-assignment
* output (ClusterAssignUnorderedAccessIndices' write side) that
* DeferredLightingPS.hlsl/ShadowRT.hlsl bin per-pixel lighting against.
*/
struct LightShaderResourceIndices
{
	uint point_light_index_;
	uint spot_light_index_;
	uint rect_light_index_;
	uint cluster_data_index_;

	uint cluster_light_list_index_;
	float3 light_shader_resource_padding_0_;
};

struct ClusterInstance
{
	uint point_count_;
	uint spot_count_;
	uint rect_count_;
	float cluster_instance_padding_0_;
};

uint3 ComputeClusterCount(float2 screen_size)
{
	return uint3(
		(uint(screen_size.x) + CLUSTER_TILE_SIZE - 1) / CLUSTER_TILE_SIZE,
		(uint(screen_size.y) + CLUSTER_TILE_SIZE - 1) / CLUSTER_TILE_SIZE,
		CLUSTER_DEPTH_SLICES
	);
}

uint ComputeDepthSlice(float linear_depth, float near_plane, float far_plane)
{
	if (linear_depth <= near_plane)
	{
		return 0;
	}
	if (linear_depth >= far_plane)
	{
		return CLUSTER_DEPTH_SLICES - 1;
	}

	float log_near = log(near_plane);
	float log_range = log(far_plane) - log_near;
	uint slice = uint((log(linear_depth) - log_near) / log_range * float(CLUSTER_DEPTH_SLICES));
	return min(slice, CLUSTER_DEPTH_SLICES - 1);
}

float SliceToDepth(uint slice, float near_plane, float far_plane)
{
	float log_near = log(near_plane);
	float log_range = log(far_plane) - log_near;
	return exp(log_near + log_range * (float(slice) / float(CLUSTER_DEPTH_SLICES)));
}

uint ClusterIndex(uint3 cluster_id, uint3 cluster_count)
{
	return (cluster_id.z * cluster_count.y + cluster_id.y) * cluster_count.x + cluster_id.x;
}

#endif // __CLUSTER_HLSL__
