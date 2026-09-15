#ifndef __VOLUMETRIC_LIGHT_HLSL__
#define __VOLUMETRIC_LIGHT_HLSL__

#include "../../Shader/Dispatch.hlsli"
#include "../Froxel/Froxel.hlsli"

/**
* Fog / volumetric light tuning constant buffer, read by the three froxel
* passes (FogInjectionCS / VolumetricLightScatteringRT / FroxelIntegrationCS)
* and DeferredCompositePS via
* constant_indices.volumetric_light_index_. Must match the
* C++ mirror in Renderer/VolumetricLightRenderer.h byte-for-byte.
*/
struct VolumetricLightRayConstantBuffer
{
	/// Base fog density (scattering strength).
	float density_;

	/// Absorption coefficient (extinction = density + absorption).
	float absorption_;

	/// Height-fog falloff (0 = uniform fog).
	float height_falloff_;

	/// Height-fog reference Y (density = base at this height).
	float height_reference_;

	/// Scattering tint of the medium.
	float3 fog_albedo_;

	/// Henyey-Greenstein anisotropy (higher = stronger god rays toward sun).
	float scattering_g_;

	/// Max shadow-ray length for the froxel sun-occlusion test.
	float ray_t_max_;

	/// Multiplier on the sun in-scattering term (god-ray strength).
	float godray_strength_;

	uint froxel_dimension_x_;
	uint froxel_dimension_y_;

	uint froxel_dimension_z_;

	/// 1 = attenuate the sun by a short cloud lightmarch (crepuscular rays
	/// through cloud gaps); needs the procedural cloud system enabled.
	uint cloud_shadow_enabled_;

	/// Padding to keep the buffer's byte size aligned with the C++ mirror.
	float2 volumetric_light_padding_;
};

struct VolumetricLightDispatchConstantBuffer
{
	uint scattering_write_unordered_access_view_index_;
	uint scattering_history_shader_resource_view_index_;
	uint history_valid_;
	uint frame_index_;

	float3 jitter_;
	float volumetric_light_dispatch_padding_;
};

ConstantBuffer<VolumetricLightDispatchConstantBuffer> GetVolumetricLightDispatchConstantBuffer()
{
	return ResourceDescriptorHeap[dispatch_buffer_index_];
}

/**
* Exact froxel -> world reconstruction: builds the view-space position from
* the NDC xy and the linear view Z using the projection's diagonal, then
* transforms by the inverse view. (Froxel.hlsli's FroxelToWorld assumes an
* infinite-far reverse-Z projection; this version matches the engine's actual
* projection.)
*/
float3 FroxelToWorldExact(float3 froxel_coordinate, uint3 froxel_dimensions, float near_plane, float far_plane, float4x4 projection, float4x4 inverse_view)
{
	float2 uv = froxel_coordinate.xy / float2(froxel_dimensions.xy);
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	float slice_t = froxel_coordinate.z / float(froxel_dimensions.z);
	float view_z = FroxelSliceToViewZ(slice_t, near_plane, far_plane);

	float3 view_position;
	view_position.x = ndc.x * view_z / projection[0][0];
	view_position.y = ndc.y * view_z / projection[1][1];
	view_position.z = view_z;

	return mul(float4(view_position, 1.0), inverse_view).xyz;
}

struct VolumetricLightShaderResourceIndices
{
	uint integration_index_;
	uint3 volumetric_light_shader_resource_padding_0_;
};

struct VolumetricLightUnorderedAccessIndices
{
	uint density_index_;
	uint integration_index_;
	uint2 volumetric_light_unordered_access_padding_0_;
};

#endif // __VOLUMETRIC_LIGHT_HLSL__
