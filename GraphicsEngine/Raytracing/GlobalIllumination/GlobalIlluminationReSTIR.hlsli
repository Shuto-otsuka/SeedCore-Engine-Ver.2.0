#ifndef __GLOBAL_ILLUMINATION_RESTIR_HLSL__
#define __GLOBAL_ILLUMINATION_RESTIR_HLSL__

/**
* Reference:
* - https://cs.dartmouth.edu/~wjarosz/publications/bitterli20spatiotemporal.html
*   (Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray
*   tracing with dynamic direct lighting", SIGGRAPH 2020 - the streaming RIS
*   combine below.)
* - https://research.nvidia.com/publication/2021-06_restir-gi-path-resampling-real-time-path-tracing
*   (Ouyang et al., "ReSTIR GI: Path Resampling for Real-Time Path Tracing",
*   HPG 2021 - the canonical receiver-side target function + reconnection
*   Jacobian this file's combine deliberately does NOT implement; see the
*   comment on GlobalIlluminationReservoirCombine below for why.)
*
* Cap on the reservoir's confidence (candidate count folded in so far) -
* shared between GlobalIlluminationRT.hlsl's raygen (which clamps the
* reprojected history's M before combining) and
* GlobalIlluminationReservoirSpatialCS.hlsl (which normalizes the final M
* into a 0..1 confidence signal for GlobalIlluminationDenoiseCS.hlsl - see
* that file's use of structured_indices.global_illumination_.confidence_).
*
* This directly trades noise for responsiveness: the streaming RIS combine's
* probability of accepting a brand-new frame's candidate over the existing
* reservoir is ~1/(M+1), so at steady state (M == cap) the reservoir itself
* is an IIR filter with an effective time constant of roughly `cap` frames,
* independent of anything downstream (the confidence signal above only
* controls how much the SEPARATE SVGF blend adds on top - it does not shorten
* the reservoir's own memory). A lower cap converges slower per pixel (more
* visible 1spp noise survives longer) but tracks scene change - camera
* motion, moving reflected/bounced content - much faster.
*/
static const float GI_RESERVOIR_M_CAP = 8.0;

/**
* Per-pixel ReSTIR reservoir for the single cosine-weighted hemisphere sample
* GlobalIlluminationRayGeneration traces per frame. 48 bytes - must match the
* C++ side (GlobalIlluminationRenderer::reservoirElementSizeInBytes)
* byte-for-byte.
*/
struct GlobalIlluminationReservoir
{
	/// The secondary hit point's world-space position (kept for a future
	/// visibility-reuse pass; the combine below does not need it).
	float3 sample_position_;

	/// Confidence: how many candidates (this frame's fresh sample plus every
	/// prior frame/neighbor folded in via streaming combine) this reservoir
	/// represents, clamped by callers to GI_RESERVOIR_M_CAP.
	float sample_m_;

	/// The secondary hit point's world-space normal (kept for the same
	/// future use as sample_position_).
	float3 sample_normal_;

	/// Final unbiased resampling weight for sample_radiance_ - multiplying
	/// sample_radiance_ * sample_w_ gives this reservoir's unbiased estimate
	/// of the incoming radiance.
	float sample_w_;

	/// That candidate's already-pdf-divided Monte Carlo estimate (what
	/// payload.radiance_ used to be written directly).
	float3 sample_radiance_;

	/// Padding to keep the struct's byte size matching the C++ mirror.
	float sample_padding_;
};

/**
* Wraps one fresh Monte Carlo estimate (already divided by its own pdf, e.g.
* GlobalIlluminationRayGeneration's cosine-weighted hemisphere sample) as a
* trivial single-sample reservoir: M = 1, and W = 1 because a reservoir of
* exactly one sample resamples itself with probability 1 by definition
* (wsum == target, so W = wsum / (M * target) = 1).
*/
GlobalIlluminationReservoir GlobalIlluminationReservoirFromSample(float3 position, float3 normal, float3 radiance)
{
	GlobalIlluminationReservoir reservoir;
	reservoir.sample_position_ = position;
	reservoir.sample_normal_ = normal;
	reservoir.sample_radiance_ = radiance;
	reservoir.sample_m_ = 1.0;
	reservoir.sample_w_ = 1.0;
	reservoir.sample_padding_ = 0.0;
	return reservoir;
}

/**
* Streaming RIS combine of two already-resolved reservoirs (Bitterli,
* "Spatiotemporal reservoir resampling for real-time ray tracing"): each
* input reservoir is itself an unbiased estimate over M samples with weight
* W, so its own RIS weight when folded into a bigger reservoir is
* target(sample) * W * M. Calling this repeatedly - once against the
* reprojected temporal history, then once per accepted spatial neighbor -
* accumulates an arbitrarily large effective sample count one reservoir at a
* time (see GlobalIlluminationRT.hlsl's call site). b_valid selects out
* disocclusion/off-screen/rejected-neighbor cases without a branch at the
* call site.
*
* This is a biased simplification: a fully unbiased spatiotemporal ReSTIR
* also re-verifies the winning sample's visibility/Jacobian against the
* current pixel. Skipped here - see GlobalIlluminationRT.hlsl's call site
* comment.
*
* A receiver-side target function (luminance * cosine at the receiving pixel,
* per Ouyang et al. 2021, see the Reference block above) plus a reconnection
* Jacobian were tried here and reverted: both terms depend on the receiver's
* position/normal, which drift a little every frame under temporal
* reprojection (rounding to the nearest previous pixel, small camera motion)
* even on a static surface. That turns the RESAMPLING DECISION itself - which
* candidate wins, not just its radiance - into something that flickers frame
* to frame, which reads as noise no denoiser downstream is tuned to absorb.
* Plain luminance has no such receiver dependency, so the decision is stable;
* the cost is the corner/narrow-interior brightening bias the omitted terms
* would have fixed. Reflection.hlsli's own reservoir makes the identical
* trade for the identical reason - keep them consistent.
*/
GlobalIlluminationReservoir GlobalIlluminationReservoirCombine(GlobalIlluminationReservoir a, GlobalIlluminationReservoir b, bool b_valid, inout uint rng_state)
{
	if (!b_valid)
	{
		return a;
	}

	GlobalIlluminationReservoir result = a;

	/// Target function p_hat: plain luminance of the candidate's estimate -
	/// deliberately not receiver/Jacobian-aware, see the block comment
	/// above.
	float a_target = dot(a.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	float a_weight = a_target * a.sample_w_ * a.sample_m_;

	float b_target = dot(b.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	float b_weight = b_target * b.sample_w_ * b.sample_m_;

	float weight_sum = a_weight + b_weight;
	result.sample_m_ = a.sample_m_ + b.sample_m_;

	if (b_weight > 0.0 && Rand(rng_state) < b_weight / weight_sum)
	{
		result.sample_position_ = b.sample_position_;
		result.sample_normal_ = b.sample_normal_;
		result.sample_radiance_ = b.sample_radiance_;
	}

	float selected_target = dot(result.sample_radiance_, float3(0.2126, 0.7152, 0.0722));
	result.sample_w_ = selected_target > 0.0 ? (weight_sum / (result.sample_m_ * selected_target)) : 0.0;
	result.sample_padding_ = 0.0;

	return result;
}

#endif // __GLOBAL_ILLUMINATION_RESTIR_HLSL__
