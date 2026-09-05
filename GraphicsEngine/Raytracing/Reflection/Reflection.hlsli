#ifndef __REFLECTION_HLSL__
#define __REFLECTION_HLSL__

#include "../../Shader/Normal.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Material.hlsli"
#include "../../Light/Cluster.hlsli"

/**
* Reflection tuning constant buffer, read by both ReflectionRT.hlsl and
* DeferredLightingPS.hlsl via
* structured_indices.reflection_.ray_constant_index_. Must match the C++
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
};

// ReflectionMaterial/ReflectionInstanceData/ResolveReflectionMaterial now
// live in Shader/Material.hlsli, shared with the G-Buffer material resolve
// path (ResolveGBufferMaterial) - included above.

/// Mirrors the C++ CompressedVertex struct (Model/Crister.h) - 16 bytes.
/// The closesthit shader only needs the normal, decoded from the octahedral
/// 16+16-bit field (Normal.hlsli: OctNormalDecode).
struct ReflectionVertex
{
	uint position_xy_;
	uint position_z_texu_;
	uint texv_tangent_;
	uint normal_;
};

/**
* Decodes ReflectionVertex's octahedral-packed normal_ field back into a
* world/object-space unit vector.
*/
float3 DecodeReflectionVertexNormal(ReflectionVertex vertex)
{
	return OctNormalDecode(float2(vertex.normal_ & 0xFFFF, vertex.normal_ >> 16) / 65535.0);
}

/**
* Same unpacking as Model.hlsli's DecodeVertex - u is the high half of
* position_z_texu_, v the low half of texv_tangent_ - rescaled out of the
* mesh's UV AABB. Kept in sync with that: a mismatch here shows up as
* reflections sampling the wrong part of the texture.
*/
float2 DecodeReflectionVertexTexcoord(ReflectionVertex vertex, float2 texcoord_min, float2 texcoord_extent)
{
	float2 texcoord01 = float2(vertex.position_z_texu_ >> 16, vertex.texv_tangent_ & 0xFFFF) / 65535.0;
	return texcoord_min + texcoord01 * texcoord_extent;
}

/**
* True when a ray should pass THROUGH this candidate triangle: OPAQUE always
* blocks, MASK blocks only where base color alpha reaches alphaCutoff, BLEND
* never blocks (the PPLL in Model/Transparent draws those surfaces instead).
* Only reached on meshes whose BLAS geometry was declared non-opaque, which
* RaytracingRenderer does whenever any of the mesh's materials is MASK or
* BLEND (see geometryDesc.opaque_); a fully OPAQUE mesh keeps
* D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE and never reaches here.
* instance_data_index is structured_indices.raytracing_.instance_data_index_,
* passed in so this header needs no extra includes.
*/
bool IsReflectionMaterialPassthrough(uint instance_data_index, uint instance_id, uint primitive_index, float2 barycentrics)
{
	StructuredBuffer<ReflectionInstanceData> instances = ResourceDescriptorHeap[instance_data_index];
	ReflectionInstanceData instance = instances[instance_id];

	ReflectionMaterial material = ResolveReflectionMaterial(instance, primitive_index);

	/// alpha_mode_: 0 = OPAQUE, 1 = MASK, 2 = BLEND (glTF convention).
	if (material.alpha_mode_ == 0)
	{
		return false;
	}

	if (material.alpha_mode_ == 2)
	{
		return true;
	}

	float alpha = material.base_color_alpha_;

	if (material.base_color_texture_index_ != 0xFFFFFFFF)
	{
		StructuredBuffer<uint> triangle_indices = ResourceDescriptorHeap[instance.index_buffer_index_];
		StructuredBuffer<ReflectionVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];

		uint base_index = primitive_index * 3;
		ReflectionVertex vertex0 = vertices[triangle_indices[base_index + 0]];
		ReflectionVertex vertex1 = vertices[triangle_indices[base_index + 1]];
		ReflectionVertex vertex2 = vertices[triangle_indices[base_index + 2]];

		float weight0 = 1.0 - barycentrics.x - barycentrics.y;
		float weight1 = barycentrics.x;
		float weight2 = barycentrics.y;

		float2 texcoord =
			DecodeReflectionVertexTexcoord(vertex0, instance.texcoord_min_, instance.texcoord_extent_) * weight0 +
			DecodeReflectionVertexTexcoord(vertex1, instance.texcoord_min_, instance.texcoord_extent_) * weight1 +
			DecodeReflectionVertexTexcoord(vertex2, instance.texcoord_min_, instance.texcoord_extent_) * weight2;

		Texture2D<float4> base_color_texture = ResourceDescriptorHeap[material.base_color_texture_index_];
		alpha *= base_color_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).a;
	}

	return alpha < material.alpha_cutoff_;
}

/**
* Occlusion query with the alpha test above applied per candidate hit. Inline
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
			if (!IsReflectionMaterialPassthrough(instance_data_index, query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
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
float3 ComputeClusteredPunctualLighting(float3 world_position, float3 world_normal, SceneConstantBuffer scene, LightConstantData light)
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

	StructuredBuffer<ClusterData> cluster_data = ResourceDescriptorHeap[light.cluster_data_shader_resource_view_index_];
	ClusterData cluster = cluster_data[cluster_index];

	uint point_count = min(cluster.point_count_, CLUSTER_MAX_POINT_LIGHTS);
	uint spot_count = min(cluster.spot_count_, CLUSTER_MAX_SPOT_LIGHTS);
	uint rect_count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);
	uint total_punctual = point_count + spot_count + rect_count;

	if (total_punctual == 0)
	{
		return lighting;
	}

	ByteAddressBuffer light_list = ResourceDescriptorHeap[light.cluster_light_list_shader_resource_view_index_];
	uint base = cluster_index * CLUSTER_STRIDE;

	StructuredBuffer<PointLightData> point_lights = ResourceDescriptorHeap[light.point_light_index_];
	StructuredBuffer<SpotLightData> spot_lights = ResourceDescriptorHeap[light.spot_light_index_];
	StructuredBuffer<RectLightData> rect_lights = ResourceDescriptorHeap[light.rect_light_index_];

	for (uint index = 0; index < total_punctual; index++)
	{
		if (index < point_count)
		{
			uint light_index = light_list.Load((base + index) * 4);
			PointLightData point_light = point_lights[light_index];

			float3 to_light = point_light.position - world_position;
			float distance_to_light = length(to_light);
			float3 light_direction = to_light / max(distance_to_light, 0.0001);

			float normal_dot_light = saturate(dot(world_normal, light_direction));
			if (normal_dot_light > 0.0)
			{
				float attenuation = AttenuateDistance(distance_to_light, point_light.range);
				lighting += point_light.color.rgb * point_light.intensity * attenuation * normal_dot_light;
			}
		}
		else if (index < point_count + spot_count)
		{
			uint local_index = index - point_count;
			uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + local_index) * 4);
			SpotLightData spot_light = spot_lights[light_index];

			float3 to_light = spot_light.position - world_position;
			float distance_to_light = length(to_light);
			float3 light_direction = to_light / max(distance_to_light, 0.0001);

			float normal_dot_light = saturate(dot(world_normal, light_direction));
			if (normal_dot_light > 0.0)
			{
				float cos_angle = dot(-light_direction, spot_light.direction);
				float spot_fade = saturate((cos_angle - spot_light.cos_half_angle) / max(spot_light.softness * (1.0 - spot_light.cos_half_angle), 0.0001));
				float attenuation = AttenuateDistance(distance_to_light, spot_light.range) * spot_fade;
				lighting += spot_light.color.rgb * spot_light.intensity * attenuation * normal_dot_light;
			}
		}
		else
		{
			uint local_index = index - point_count - spot_count;
			uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + local_index) * 4);
			RectLightData rect_light = rect_lights[light_index];

			if (dot(world_position - rect_light.position, rect_light.normal) > 0.0)
			{
				float3 representative_point = ClosestPointOnRect(world_position, rect_light.position, rect_light.right, rect_light.up, rect_light.half_width, rect_light.half_height);
				float3 to_light = representative_point - world_position;
				float distance_to_light = length(to_light);
				float3 light_direction = to_light / max(distance_to_light, 0.0001);

				float normal_dot_light = saturate(dot(world_normal, light_direction));
				if (normal_dot_light > 0.0)
				{
					float attenuation = AttenuateDistance(distance_to_light, rect_light.range);
					lighting += rect_light.color.rgb * rect_light.intensity * attenuation * normal_dot_light;
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
