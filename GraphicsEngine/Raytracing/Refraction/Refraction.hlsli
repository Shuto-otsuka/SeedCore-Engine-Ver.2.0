#ifndef __REFRACTION_HLSL__
#define __REFRACTION_HLSL__

/**
* Refraction tuning constant buffer, read by both RefractionRT.hlsl and
* Model/Opaque/DeferredLightingPS.hlsl via
* constant_indices.refraction_index_. Must match the C++
* mirror in Renderer/RefractionRenderer.h byte-for-byte.
*/
struct RefractionRayConstantBuffer
{
	/// Maximum distance a refraction ray travels before being treated as
	/// reaching the sky/environment.
	float ray_t_max_;

	/// Offset along the normal applied to each bounce's ray origin, to keep
	/// the ray from immediately re-hitting the interface it was cast from.
	float normal_bias_;

	/// Overall refraction intensity applied in
	/// Model/Opaque/DeferredLightingPS.hlsl.
	float strength_;

	/// Padding to keep the buffer's byte size aligned with the C++ mirror.
	float refraction_padding_;
};

struct RefractionShaderResourceIndices
{
	uint output_index_;
	uint3 refraction_shader_resource_padding_0_;
};

struct RefractionUnorderedAccessIndices
{
	uint output_index_;
	uint3 refraction_unordered_access_padding_0_;
};

#endif // __REFRACTION_HLSL__
