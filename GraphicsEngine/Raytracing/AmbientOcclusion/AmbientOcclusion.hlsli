#ifndef __AMBIENT_OCCLUSION_HLSL__
#define __AMBIENT_OCCLUSION_HLSL__

/**
* [EN]
* AO tuning constant buffer, read by both AmbientOcclusionRT.hlsl (the
* raytrace pass) and DeferredLightingPS.hlsl (the composite pass, which reads
* power_ to shape the contrast curve). Must match the C++ mirror in
* Renderer/AmbientOcclusionRenderer.h byte-for-byte.
*/
struct AmbientOcclusionRayConstantBuffer
{
	/// World-space AO radius: occluders farther than this along the traced
	/// ray don't darken this pixel at all.
	float ray_length_;

	/// Offset along the surface normal applied to the ray origin, to keep
	/// the ray from immediately re-hitting the triangle it was cast from.
	float normal_bias_;

	/// Contrast exponent applied in DeferredLightingPS.hlsl:
	/// ao = pow(ao, power_). 1 = as-is, >1 = darker/stronger.
	float power_;

	/// Per-frame counter driving the RNG seed - set by
	/// AmbientOcclusionRenderer, not the UI.
	uint frame_index_;
};

#endif // __AMBIENT_OCCLUSION_HLSL__
