#ifndef __REFRACTION_HLSL__
#define __REFRACTION_HLSL__

// Refraction tuning constant buffer, read by both RefractionRT.hlsl and
// Model/DeferredLightingPS.hlsl via structured_indices.refraction_.ray_constant_index_.
// Must match the C++ mirror in Renderer/RefractionRenderer.h byte-for-byte.
struct RefractionRayConstantBuffer
{
	float ray_t_max_;
	float normal_bias_;

	// Overall refraction intensity applied in Model/DeferredLightingPS.hlsl.
	float strength_;

	float refraction_padding_;
};

#endif // __REFRACTION_HLSL__
