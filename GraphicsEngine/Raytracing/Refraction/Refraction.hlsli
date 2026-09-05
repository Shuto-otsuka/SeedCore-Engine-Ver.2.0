#ifndef __REFRACTION_HLSL__
#define __REFRACTION_HLSL__

/**
* Refraction tuning constant buffer, read by both RefractionRT.hlsl and
* Model/Opaque/DeferredLightingPS.hlsl via
* structured_indices.refraction_.ray_constant_index_. Must match the C++
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

#endif // __REFRACTION_HLSL__
