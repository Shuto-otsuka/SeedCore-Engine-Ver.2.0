#ifndef __SUBSURFACE_SCATTERING_HLSL__
#define __SUBSURFACE_SCATTERING_HLSL__

/**
* SSS tuning constant buffer, read by both SubsurfaceScatteringRT.hlsl and
* DeferredLightingPS.hlsl via
* constant_indices.subsurface_scattering_index_. Must match
* the C++ mirror in Renderer/SubsurfaceScatteringRenderer.h byte-for-byte.
*/
struct SubsurfaceScatteringRayConstantBuffer
{
	/// Characteristic distance of light falloff inside the object:
	/// transmittance = exp(-thickness / scatter_distance_). Smaller = more
	/// opaque.
	float scatter_distance_;

	/// Offset pushing the ray origin INTO the surface (along -normal) so the
	/// thickness ray doesn't immediately hit the surface it started on.
	float thickness_bias_;

	/// Max thickness to search: rays that exit farther than this count as
	/// "no translucency".
	float ray_t_max_;

	/// Overall intensity multiplier applied in DeferredLightingPS.hlsl.
	float strength_;

	/// Tint of the transmitted light (reddish for skin, greenish for
	/// leaves).
	float3 subsurface_color_;

	/// Padding to keep the buffer's byte size aligned with the C++ mirror.
	float subsurface_scattering_padding_;
};

struct SubsurfaceScatteringShaderResourceIndices
{
	uint transmittance_index_;
	uint3 subsurface_scattering_shader_resource_padding_0_;
};

struct SubsurfaceScatteringUnorderedAccessIndices
{
	uint transmittance_index_;
	uint3 subsurface_scattering_unordered_access_padding_0_;
};

#endif // __SUBSURFACE_SCATTERING_HLSL__
