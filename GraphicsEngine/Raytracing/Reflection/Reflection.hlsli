#ifndef __REFLECTION_HLSL__
#define __REFLECTION_HLSL__

#include "../../Shader/Normal.hlsli"
#include "../../Shader/Vertex.hlsli"
#include "../../Shader/Scene.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Light/Light.hlsli"
#include "../../Shader/Material.hlsli"
#include "../../Light/Cluster.hlsli"
#include "../../Shader/ShaderResources.hlsli"

/**
* Reflection tuning constant buffer, read by both ReflectionRT.hlsl and
* DeferredLightingPS.hlsl via
* constant_indices.reflection_index_. Must match the C++
* mirror in Renderer/ReflectionRenderer.h byte-for-byte.
*/
struct ReflectionRayConstantBuffer
{
	/// Maximum distance a reflection ray travels before being treated as
	/// reaching the sky/environment.
	float ray_t_max_;

	/// Offset along the surface normal applied to the ray origin, to keep
	/// the ray from immediately re-hitting the triangle it was cast from.
	float normal_bias_;

	/// Overall reflection intensity applied in DeferredLightingPS.hlsl.
	float strength_;

	/// Incremented once per frame by ReflectionRenderer (not the UI) -
	/// rotates the GGX visible-normal sample so the roughness-driven 1spp
	/// noise averages out over time instead of being a fixed pattern.
	/// Unused at roughness 0 (the sampled half vector degenerates to the
	/// surface normal there regardless of the sample), so this only matters
	/// for rough surfaces.
	uint frame_index_;

	uint temporal_reuse_enabled_;

	float3 reflection_ray_padding_;
};

// ReflectionMaterial/ReflectionInstanceData/ResolveReflectionMaterial now
// live in Shader/Material.hlsli, shared with the G-Buffer material resolve
// path (ResolveGBufferMaterial) - included above.

// CompressedVertex/DecodeCompressedVertex* are now CompressedVertex and
// DecodeCompressedVertex{Normal,Texcoord} in Shader/Vertex.hlsli, shared with
// the other raytracing hit paths. IsMaterialPassthrough now lives at the end of
// Shader/Material.hlsli, next to the material resolve it depends on.

/**
* Occlusion query with the alpha test in IsMaterialPassthrough applied
* per candidate hit. Inline
* raytracing has no any-hit stage, so the candidate loop is the only place a
* RayQuery can run it.
*/
bool IsReflectionRayOccluded(RaytracingAccelerationStructure tlas, RayDesc ray_desc, uint instance_data_index)
{
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray_desc);

	while (query.Proceed())
	{
		if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
		{
			if (!IsMaterialPassthrough(instance_data_index, query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
			{
				query.CommitNonOpaqueTriangleHit();
			}
		}
	}

	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

/**
* Sums the Lambertian contribution of every point/spot/rect light in the hit
* point's screen-space light cluster, gated by N.L (world_normal) so lights
* behind the surface never contribute. Shared by ReflectionRT.hlsl and
* GlobalIlluminationRT.hlsl closesthit - both already do this for the
* directional light only; this adds the punctual lights the same way
* ShadowRT.hlsl already does for the primary G-Buffer surface.
*
* The light cluster grid is built around the camera frustum (screen tiles x
* depth slices), so world_position is reprojected through the NON-jittered
* view-projection to find its tile/slice. That reprojection is an
* approximation for points outside the visible frustum (a reflection can hit
* geometry the camera can't see) - it degrades gracefully by clamping to the
* nearest edge cluster rather than lighting incorrectly. Non-jittered is
* deliberate: both call sites are meant to be temporally stable (reflection
* has no denoiser-side jitter it needs to match; GI's hemisphere sample
* already carries its own per-frame jitter), so using the jittered matrix
* here would flicker the cluster tile boundary every frame independently of
* that.
*
* No shadow ray per light here (only the directional light gets one at each
* call site) - tracing one per punctual light would multiply ray count by
* however many lights share this cluster.
*/
float3 ComputeClusteredPunctualLighting(float3 world_position, float3 world_normal, SceneConstantBuffer scene, LightConstantBuffer light)
{
	float3 lighting = float3(0, 0, 0);

	float4 clip = mul(float4(world_position, 1.0), scene.non_jitter_view_projection_);
	if (clip.w <= 0.0)
	{
		/// Behind the camera - the frustum-aligned cluster grid has no
		/// meaningful cell for this point.
		return lighting;
	}

	float2 ndc = clip.xy / clip.w;
	float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	uint2 pixel = uint2(saturate(uv) * scene.screen_size_);

	float4 view_position = mul(float4(world_position, 1.0), scene.view_);
	float linear_depth = view_position.z;

	uint3 cluster_count = ComputeClusterCount(scene.screen_size_);
	uint tile_x = min(pixel.x / CLUSTER_TILE_SIZE, cluster_count.x - 1);
	uint tile_y = min(pixel.y / CLUSTER_TILE_SIZE, cluster_count.y - 1);
	uint slice = ComputeDepthSlice(linear_depth, scene.near_plane_, scene.far_plane_);
	uint cluster_index = ClusterIndex(uint3(tile_x, tile_y, slice), cluster_count);

	StructuredBuffer<ClusterInstance> cluster_data = ResourceDescriptorHeap[shader_resource_indices.light_.cluster_data_index_];
	ClusterInstance cluster = cluster_data[cluster_index];

	uint point_count = min(cluster.point_count_, CLUSTER_MAX_POINT_LIGHTS);
	uint spot_count = min(cluster.spot_count_, CLUSTER_MAX_SPOT_LIGHTS);
	uint rect_count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);
	uint total_punctual = point_count + spot_count + rect_count;

	if (total_punctual == 0)
	{
		return lighting;
	}

	ByteAddressBuffer light_list = ResourceDescriptorHeap[shader_resource_indices.light_.cluster_light_list_index_];
	uint base = cluster_index * CLUSTER_STRIDE;

	StructuredBuffer<PointLightStructuredBuffer> point_lights = GetPointLightStructuredBuffer(shader_resource_indices.light_.point_light_index_);
	StructuredBuffer<SpotLightStructuredBuffer> spot_lights = GetSpotLightStructuredBuffer(shader_resource_indices.light_.spot_light_index_);
	StructuredBuffer<RectLightStructuredBuffer> rect_lights = GetRectLightStructuredBuffer(shader_resource_indices.light_.rect_light_index_);

	for (uint index = 0; index < total_punctual; index++)
	{
		if (index < point_count)
		{
			uint light_index = light_list.Load((base + index) * 4);
			PointLightStructuredBuffer point_light = point_lights[light_index];

			float3 to_light = point_light.position_ - world_position;
			float distance_to_light = length(to_light);
			float3 light_direction = to_light / max(distance_to_light, 0.0001);

			float normal_dot_light = saturate(dot(world_normal, light_direction));
			if (normal_dot_light > 0.0)
			{
				float attenuation_ratio = saturate(distance_to_light / point_light.range_);
				float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
				float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
				attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001);
				lighting += point_light.color_.rgb * point_light.intensity_ * attenuation * normal_dot_light;
			}
		}
		else if (index < point_count + spot_count)
		{
			uint local_index = index - point_count;
			uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + local_index) * 4);
			SpotLightStructuredBuffer spot_light = spot_lights[light_index];

			float3 to_light = spot_light.position_ - world_position;
			float distance_to_light = length(to_light);
			float3 light_direction = to_light / max(distance_to_light, 0.0001);

			float normal_dot_light = saturate(dot(world_normal, light_direction));
			if (normal_dot_light > 0.0)
			{
				float cos_angle = dot(-light_direction, spot_light.direction_);
				float spot_fade = saturate((cos_angle - spot_light.cos_half_angle_) / max(spot_light.softness_ * (1.0 - spot_light.cos_half_angle_), 0.0001));
				float attenuation_ratio = saturate(distance_to_light / spot_light.range_);
				float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
				float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
				attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001) * spot_fade;
				lighting += spot_light.color_.rgb * spot_light.intensity_ * attenuation * normal_dot_light;
			}
		}
		else
		{
			uint local_index = index - point_count - spot_count;
			uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + local_index) * 4);
			RectLightStructuredBuffer rect_light = rect_lights[light_index];

			if (dot(world_position - rect_light.position_, rect_light.normal_) > 0.0)
			{
				float3 rect_delta = world_position - rect_light.position_;
				float rect_local_x = clamp(dot(rect_delta, rect_light.right_), -rect_light.half_width_, rect_light.half_width_);
				float rect_local_y = clamp(dot(rect_delta, rect_light.up_), -rect_light.half_height_, rect_light.half_height_);
				float3 representative_point = rect_light.position_ + rect_light.right_ * rect_local_x + rect_light.up_ * rect_local_y;
				float3 to_light = representative_point - world_position;
				float distance_to_light = length(to_light);
				float3 light_direction = to_light / max(distance_to_light, 0.0001);

				float normal_dot_light = saturate(dot(world_normal, light_direction));
				if (normal_dot_light > 0.0)
				{
					float attenuation_ratio = saturate(distance_to_light / rect_light.range_);
					float attenuation_ratio2 = attenuation_ratio * attenuation_ratio;
					float attenuation = saturate(1.0 - attenuation_ratio2 * attenuation_ratio2);
					attenuation = attenuation * attenuation / max(distance_to_light * distance_to_light, 0.0001);
					lighting += rect_light.color_.rgb * rect_light.intensity_ * attenuation * normal_dot_light;
				}
			}
		}
	}

	/// Same Lambertian BRDF 1/PI convention as the directional term at each
	/// call site (BrdfLambertian returns albedo/PI) - without it punctual
	/// lights would be PI times too bright relative to the sun term.
	const float lambert_normalization = 1.0 / 3.14159265358979;
	return lighting * lambert_normalization;
}

#endif // __REFLECTION_HLSL__
