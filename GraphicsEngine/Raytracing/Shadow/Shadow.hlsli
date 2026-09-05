#ifndef __SHADOW_HLSL__
#define __SHADOW_HLSL__

/**
* [EN]
* Shadow tuning constant buffer, read by both ShadowRT.hlsl (the raytrace
* pass itself) and DeferredLightingPS.hlsl (the composite pass, which reads
* shadow_strength_ to blend the traced visibility into the directional
* term). Must match the C++ mirror in Renderer/ShadowRenderer.h byte-for-byte
* - field order and size cannot change on one side without changing the
* other.
*/
struct ShadowRayConstantBuffer
{
	/// Maximum distance a shadow ray travels before being treated as
	/// unoccluded. Keeps punctual-light shadow rays from tracing across the
	/// whole scene when the light itself is close.
	float ray_t_max_;

	/// Offset along the surface normal applied to the ray origin, to keep the
	/// ray from immediately re-hitting the triangle it was cast from
	/// (shadow-acne avoidance).
	float normal_bias_;

	/// How strongly the traced visibility affects lighting: 0 = ignore
	/// (always lit), 1 = apply visibility as-is (hard binary shadow). Used as
	/// lerp(1.0, visibility, shadow_strength_) in DeferredLightingPS.hlsl.
	float shadow_strength_;

	/// Half-angle (radians) of the directional light's disk, for soft
	/// shadows: ShadowRT.hlsl jitters the shadow ray within this cone. 0 =
	/// hard shadow (no cone jitter).
	float sun_angular_radius_;

	/// World-space radius used to soften point/spot light shadows the same
	/// way (bigger radius = softer penumbra, scaled by distance to the
	/// light).
	float punctual_light_radius_;

	/// Per-frame counter driving the RNG seed (SeedFromPixel) - the ShadowRT
	/// ray direction changes every frame so temporal accumulation converges
	/// to a smooth penumbra instead of a fixed dithered pattern. Set by
	/// ShadowRenderer, not the UI.
	uint frame_index_;

	/// 0 = Temporal (own reprojected accumulation), 1 = DlssRR (not yet
	/// wired; ShadowRenderer falls back to Temporal and logs once). C++-side
	/// dispatch decision only - ShadowRT.hlsl itself doesn't read this field.
	uint denoise_mode_;

	/// Padding to keep the buffer's byte size aligned with the C++ mirror.
	float shadow_ray_padding_;
};

#endif // __SHADOW_HLSL__
