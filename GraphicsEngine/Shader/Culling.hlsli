#ifndef __CULLING_HLSL__
#define __CULLING_HLSL__

#include "Constants.hlsli"

bool IsVisibleInScreen(float2 position, float2 size, float2 screen_size)
{
	return position.x + size.x > 0.0f
		&& position.x < screen_size.x
		&& position.y + size.y > 0.0f
		&& position.y < screen_size.y;
}

bool IsVisibleInFrustum(float3 pos, float radius, row_major float4x4 vp)
{
	float4 planes[6];
	// Left
	planes[0] = float4(vp[0][0]+vp[0][3], vp[1][0]+vp[1][3], vp[2][0]+vp[2][3], vp[3][0]+vp[3][3]);
	// Right
	planes[1] = float4(vp[0][3]-vp[0][0], vp[1][3]-vp[1][0], vp[2][3]-vp[2][0], vp[3][3]-vp[3][0]);
	// Bottom
	planes[2] = float4(vp[0][1]+vp[0][3], vp[1][1]+vp[1][3], vp[2][1]+vp[2][3], vp[3][1]+vp[3][3]);
	// Top
	planes[3] = float4(vp[0][3]-vp[0][1], vp[1][3]-vp[1][1], vp[2][3]-vp[2][1], vp[3][3]-vp[3][1]);
	// Depth (z_ndc >= 0). With a reversed-Z projection this is the far plane; with a standard projection this is the near plane.
	planes[4] = float4(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
	// Depth (z_ndc <= 1). With a reversed-Z projection this is the near plane; with a standard projection this is the far plane.
	planes[5] = float4(vp[0][3]-vp[0][2], vp[1][3]-vp[1][2], vp[2][3]-vp[2][2], vp[3][3]-vp[3][2]);

	[unroll]
	for (int i = 0; i < 6; ++i)
	{
		float d = dot(planes[i].xyz, pos) + planes[i].w;
		if (d < -radius * length(planes[i].xyz))
		{
			return false;
		}
	}
	return true;
}

// Screen-space backface test for Mesh Shader per-triangle culling.
// Fixed-function culling is CullNone so that doubleSided materials render both
// sides; single-sided materials cull here instead. Front faces are
// counter-clockwise in NDC (cross > 0). Triangles crossing the w=0 plane are
// conservatively kept.
bool IsBackFace(float4 c0, float4 c1, float4 c2)
{
	if (c0.w <= 0.0 || c1.w <= 0.0 || c2.w <= 0.0)
	{
		return false;
	}
	float2 p0 = c0.xy / c0.w;
	float2 p1 = c1.xy / c1.w;
	float2 p2 = c2.xy / c2.w;
	float area = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
	return area <= 0.0;
}

// Distance-based LOD selection. Cluster lodError values are cumulative and
// monotonically increasing along a submesh's LOD chain, so exactly one cluster
// per chain passes: pick the coarsest cluster whose own screen-space error is
// below the threshold while the next coarser one is not. LOD 0 has error 0 and
// the last LOD carries FLT_MAX as its next error, so the chain always yields a
// selection. Instances that must never be dropped (e.g. skinned, LOD0-only)
// carry next error FLT_MAX and always pass.
bool IsLodSelected(float lod_error, float lod_error_next, float3 world_position, float world_scale, float3 camera_position, float projection_scale_y, float screen_height, float threshold_pixels)
{
	float view_distance = max(length(world_position - camera_position), 0.0001);
	float pixels_per_unit = projection_scale_y * screen_height * 0.5 / view_distance;
	float error_pixels = lod_error * world_scale * pixels_per_unit;
	float error_pixels_next = lod_error_next * world_scale * pixels_per_unit;
	return error_pixels <= threshold_pixels && error_pixels_next > threshold_pixels;
}

// Hi-Z occlusion test against the depth pyramid built from the depth prepass.
// The pyramid stores the MINIMUM depth of each footprint; with reverse-Z that
// is the FARTHEST surface. A sphere is occluded when even its nearest point is
// farther (smaller depth) than everything already rasterised in its footprint.
// All rejections are conservative: any uncertain case returns visible.
bool IsVisibleHiZ(float3 world_center, float radius, SceneConstantBuffer scene, uint hi_z_index)
{
	if (hi_z_index == 0)
	{
		return true;
	}

	float3 to_camera = scene.camera_position_.xyz - world_center;
	float center_distance = length(to_camera);
	if (center_distance <= radius)
	{
		return true;
	}

	// Nearest point of the sphere towards the camera. Under reverse-Z its
	// post-projection depth is the sphere's maximum depth value.
	float3 nearest = world_center + to_camera / center_distance * radius;
	float4 nearest_clip = mul(float4(nearest, 1.0), scene.current_view_projection_);
	if (nearest_clip.w <= 0.0)
	{
		return true;
	}
	float nearest_depth = nearest_clip.z / nearest_clip.w;

	float4 center_clip = mul(float4(world_center, 1.0), scene.current_view_projection_);
	if (center_clip.w <= 0.0)
	{
		return true;
	}
	float2 center_ndc = center_clip.xy / center_clip.w;
	float2 center_pixel = float2(center_ndc.x * 0.5 + 0.5, 0.5 - center_ndc.y * 0.5) * scene.screen_size_;

	// Conservative projected radius in pixels (uses the projection diagonal).
	float radius_pixels = radius * scene.projection_._m11 * scene.screen_size_.y * 0.5 / max(center_clip.w, 0.0001);

	Texture2D<float> hi_z = ResourceDescriptorHeap[hi_z_index];
	uint hi_z_width;
	uint hi_z_height;
	uint hi_z_mip_count;
	hi_z.GetDimensions(0, hi_z_width, hi_z_height, hi_z_mip_count);

	// Choose the mip where the footprint spans at most ~2x2 texels, then take
	// the min of the 4 corner texels.
	uint mip_level = min((uint)max(ceil(log2(max(radius_pixels * 2.0, 1.0))), 0.0), hi_z_mip_count - 1);
	float2 mip_size = float2(max(hi_z_width >> mip_level, 1u), max(hi_z_height >> mip_level, 1u));

	float2 uv_min = saturate((center_pixel - radius_pixels) / scene.screen_size_);
	float2 uv_max = saturate((center_pixel + radius_pixels) / scene.screen_size_);
	int2 texel_min = int2(clamp(uv_min * mip_size, 0.0, mip_size - 1.0));
	int2 texel_max = int2(clamp(uv_max * mip_size, 0.0, mip_size - 1.0));

	float d00 = hi_z.Load(int3(texel_min.x, texel_min.y, mip_level));
	float d10 = hi_z.Load(int3(texel_max.x, texel_min.y, mip_level));
	float d01 = hi_z.Load(int3(texel_min.x, texel_max.y, mip_level));
	float d11 = hi_z.Load(int3(texel_max.x, texel_max.y, mip_level));
	float footprint_min_depth = min(min(d00, d10), min(d01, d11));

	return nearest_depth >= footprint_min_depth;
}

#endif // __CULLING_HLSL__
