#ifndef __REFLECTION_RESTIR_HLSL__
#define __REFLECTION_RESTIR_HLSL__

/**
* Reference:
* - https://cs.dartmouth.edu/~wjarosz/publications/bitterli20spatiotemporal.html
*   (Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray
*   tracing with dynamic direct lighting", SIGGRAPH 2020 - the streaming RIS
*   combine below.)
* - https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
*   (Ouyang et al., "ReSTIR GI: Path Resampling for Real-Time Path Tracing",
*   HPG 2021 - the closest published analogue for reusing a specular/glossy
*   sample this way; GlobalIlluminationReSTIR.hlsli explains why its
*   receiver-side target function is deliberately not adopted here either.)
*
* Cap on the reservoir's confidence (candidate count folded in so far) -
* shared between ReflectionRT.hlsl's raygen (which clamps the reprojected
* history's M before combining) and ReflectionReservoirSpatialCS.hlsl (which
* normalizes the final M into a 0..1 confidence signal for
* ReflectionDenoiseCS.hlsl - see that file's use of
* structured_indices.reflection_.confidence_).
*
* This directly trades noise for responsiveness: the streaming RIS combine's
* probability of accepting a brand-new frame's candidate over the existing
* reservoir is ~1/(M+1), so at steady state (M == cap) the reservoir itself
* is an IIR filter with an effective time constant of roughly `cap` frames,
* independent of anything downstream (the confidence signal above only
* controls how much the SEPARATE SVGF blend adds on top - it does not shorten
* the reservoir's own memory). A lower cap converges slower per pixel (more
* visible 1spp noise survives longer) but tracks scene change - camera
* motion, moving reflected content - much faster.
*/
static const float REFLECTION_RESERVOIR_M_CAP = 8.0;

/**
* Per-pixel ReSTIR reservoir for the single GGX-visible-normal-sampled
* reflection ray ReflectionRayGeneration traces per frame (replacing the old
* 4-rays-averaged-per-frame scheme - see ReflectionRT.hlsl). 48 bytes - must
* match the C++ side (ReflectionRenderer::reservoirElementSizeInBytes_)
* byte-for-byte.
*/
struct ReflectionReservoir
{
	/// The traced reflect direction, kept so a spatial neighbor's sample can
	/// be re-picked without re-tracing.
	float3 sample_direction_;

	/// Confidence: how many candidates (this frame's fresh sample plus every
	/// prior frame/neighbor folded in via streaming combine) this reservoir
	/// represents, clamped by callers to REFLECTION_RESERVOIR_M_CAP.
	float sample_m_;

	/// That candidate's traced radiance (same convention the old 4-ray
	/// average used - already includes the VNDF sample's Smith G2/G1 weight,
	/// see ReflectionRT.hlsl - no further BRDF/pdf division needed here).
	float3 sample_radiance_;

	/// Final unbiased resampling weight for sample_radiance_ - multiplying
	/// sample_radiance_ * sample_w_ gives this reservoir's unbiased estimate
	/// of the reflected radiance.
	float sample_w_;

	/// Carried through so ReflectionDenoiseCS.hlsl's existing hit-point
	/// virtual-motion reprojection keeps working unchanged downstream.
	float sample_hit_distance_;

	/// Padding to keep the struct's byte size matching the C++ mirror.
	float3 reflection_reservoir_padding_;
};

/**
* Wraps one fresh GGX-visible-normal-sampled candidate as a trivial
* single-sample reservoir: M = 1, and W = 1 because a reservoir of exactly
* one sample resamples itself with probability 1 by definition (wsum ==
* target, so W = wsum / (M * target) = 1).
*/
ReflectionReservoir ReflectionReservoirFromSample(float3 direction, float3 radiance, float hit_distance)
{
	ReflectionReservoir reservoir;
	reservoir.sample_direction_ = direction;
	reservoir.sample_radiance_ = radiance;
	reservoir.sample_hit_distance_ = hit_distance;
	reservoir.sample_m_ = 1.0;
	reservoir.sample_w_ = 1.0;
	reservoir.reflection_reservoir_padding_ = float3(0, 0, 0);
	return reservoir;
}

/**
* Streaming RIS combine of two already-resolved reservoirs (Bitterli,
* "Spatiotemporal reservoir resampling for real-time ray tracing") - same
* math as GlobalIlluminationReservoirCombine (GlobalIlluminationReSTIR.hlsli),
* just carrying sample_direction_/sample_hit_distance_ instead of
* sample_position_/sample_normal_. b_valid selects out disocclusion/
* off-screen/rejected-neighbor cases without a branch at the call site.
*
* This is a biased simplification: a fully unbiased spatiotemporal ReSTIR
* also re-verifies the winning sample's visibility/Jacobian against the
* current pixel, and for a specular lobe specifically would also need to
* reweight by how well the borrowed direction fits the CURRENT pixel's GGX
* lobe (its target function here is plain luminance, not a BRDF-aware one).
* Skipped here for the same reason GI's version skips it - see
* ReflectionRT.hlsl's call site comment: a target function that depends on
* the receiving pixel's own geometry drifts a little every frame under
* temporal reprojection, which turns the resampling DECISION itself into a
* new source of flicker/noise that outweighs the bias it would remove.
*/
ReflectionReservoir ReflectionReservoirCombine(ReflectionReservoir a, ReflectionReservoir b, bool b_valid, inout uint rng_state)
{
	if (!b_valid)
	{
		return a;
	}

	ReflectionReservoir result = a;

	/// [EN] Target function p_hat: plain luminance of the candidate's traced
	///      radiance - deliberately not BRDF/receiver-aware, see the block
	///      comment above.
	float a_target = dot(a.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	float a_weight = a_target * a.sample_w_ * a.sample_m_;

	float b_target = dot(b.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	float b_weight = b_target * b.sample_w_ * b.sample_m_;

	float weight_sum = a_weight + b_weight;
	result.sample_m_ = a.sample_m_ + b.sample_m_;

	if (b_weight > 0.0 && Rand(rng_state) < b_weight / weight_sum)
	{
		result.sample_direction_ = b.sample_direction_;
		result.sample_radiance_ = b.sample_radiance_;
		result.sample_hit_distance_ = b.sample_hit_distance_;
	}

	float selected_target = dot(result.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	result.sample_w_ = selected_target > 0.0 ? (weight_sum / (result.sample_m_ * selected_target)) : 0.0;
	result.reflection_reservoir_padding_ = float3(0, 0, 0);

	return result;
}

#endif // __REFLECTION_RESTIR_HLSL__
