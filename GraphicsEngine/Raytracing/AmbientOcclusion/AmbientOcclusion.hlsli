#ifndef __AMBIENT_OCCLUSION_HLSL__
#define __AMBIENT_OCCLUSION_HLSL__

// AO tuning constant buffer, read by both AmbientOcclusionRT.hlsl and
// DeferredLightingPS.hlsl via structured_indices.ambient_occlusion_.ray_constant_index_.
// Must match the C++ mirror in Renderer/AmbientOcclusionRenderer.h
// byte-for-byte.
struct AmbientOcclusionRayConstantBuffer
{
	// World-space AO radius: occluders farther than this don't darken.
	float ray_length_;

	float normal_bias_;

	// Contrast exponent applied in DeferredLightingPS.hlsl:
	// ao = pow(ao, power_). 1 = as-is, >1 = darker/stronger.
	float power_;

	// Per-frame counter driving the RNG seed - set by
	// AmbientOcclusionRenderer, not the UI.
	uint frame_index_;
};

#endif // __AMBIENT_OCCLUSION_HLSL__
