#ifndef __CLUSTER_HLSL__
#define __CLUSTER_HLSL__

#define CLUSTER_TILE_SIZE 64
#define CLUSTER_DEPTH_SLICES 16
#define CLUSTER_MAX_POINT_LIGHTS 64
#define CLUSTER_MAX_SPOT_LIGHTS 64
#define CLUSTER_MAX_RECT_LIGHTS 64
#define CLUSTER_STRIDE (CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + CLUSTER_MAX_RECT_LIGHTS)

struct ClusterData
{
	uint point_count_;
	uint spot_count_;
	uint rect_count_;
	float cluster_data_padding_;
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
