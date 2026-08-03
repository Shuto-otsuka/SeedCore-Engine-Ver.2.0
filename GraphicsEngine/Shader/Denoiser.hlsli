#ifndef __DENOISER_HLSL__
#define __DENOISER_HLSL__

#include "Sampler.hlsli"
#include "Normal.hlsli"

// Shared building blocks for the RT spatio-temporal denoisers (AO / GI /
// Reflection / ...): a depth+normal weighted bilateral spatial filter, an
// SVGF-style variance-clipped temporal reprojection blend, and a multi-pass
// A-Trous wavelet filter. Each denoiser CS still owns its own neighborhood
// loop for the FIRST spatial pass (raw texture, extra per-effect weights
// like roughness, and payload channel count all differ) and its own
// temporal blend call site, but the per-neighbor weight formula, the
// temporal clamp/blend math, and the A-Trous pass are identical across
// effects, so they live here once instead of being copy-pasted per pass.
// ASCII only (included by shaders).

// Depth+normal bilateral weight for one neighbor sample. expected_depth
// comes from extrapolating the center pixel's local depth gradient to the
// neighbor offset (a "plane fit"), so tilted-but-coplanar neighbors are not
// penalized just for having a different raw depth value - only neighbors
// that deviate from the extrapolated plane (i.e. a different surface) are.
float DenoiserSpatialWeight(float center_depth, float2 depth_gradient, int2 offset, float neighbor_depth, float3 center_normal, float3 neighbor_normal, float depth_sharpness, float normal_power)
{
	float expected_depth = center_depth + dot(depth_gradient, float2(offset));
	float depth_difference = abs(neighbor_depth - expected_depth) / max(center_depth, 0.0001);
	float depth_weight = exp(-depth_difference * depth_sharpness);

	float normal_weight = pow(saturate(dot(center_normal, neighbor_normal)), normal_power);

	return depth_weight * normal_weight;
}

// Central-difference depth gradient at pixel, used by DenoiserSpatialWeight
// above to extrapolate the local surface plane to each neighbor offset.
float2 DenoiserDepthGradient(Texture2D<float> depth_texture, int2 pixel, int2 screen_max)
{
	float depth_right = depth_texture.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), screen_max), 0));
	float depth_left = depth_texture.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max), 0));
	float depth_down = depth_texture.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), screen_max), 0));
	float depth_up = depth_texture.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), screen_max), 0));
	return float2(depth_right - depth_left, depth_down - depth_up) * 0.5;
}

// Running 3x3 color-moment accumulator (mean/variance) feeding the variance
// clipping box below. Callers accumulate into this alongside their own
// (possibly wider, e.g. 5x5) spatial-filter loop, restricted to the inner
// 3x3 neighborhood - widening the moment window softens history rejection
// too much (same tradeoff noted in the original GI denoiser).
struct DenoiserMoments
{
	float weight_sum_;
	float3 moment1_sum_;
	float3 moment2_sum_;
	float3 neighborhood_min_;
	float3 neighborhood_max_;
};

DenoiserMoments DenoiserMomentsInit()
{
	DenoiserMoments moments;
	moments.weight_sum_ = 0.0;
	moments.moment1_sum_ = float3(0, 0, 0);
	moments.moment2_sum_ = float3(0, 0, 0);
	moments.neighborhood_min_ = float3(1e5, 1e5, 1e5);
	moments.neighborhood_max_ = float3(0, 0, 0);
	return moments;
}

void DenoiserMomentsAccumulate(inout DenoiserMoments moments, float3 sample_value, float weight)
{
	moments.moment1_sum_ += sample_value * weight;
	moments.moment2_sum_ += sample_value * sample_value * weight;
	moments.weight_sum_ += weight;
	moments.neighborhood_min_ = min(moments.neighborhood_min_, sample_value);
	moments.neighborhood_max_ = max(moments.neighborhood_max_, sample_value);
}

// Variance clipping box (Salvi/Karis-style): a mean +/- gamma*stddev box
// intersected with the raw 3x3 min/max band (widened to include
// filtered_raw so the box never inverts). Used to clamp the reprojected
// history sample before blending, rejecting ghosting from disocclusion,
// camera cuts, or the uninitialized first frame far more cheaply than a
// full history-rejection heuristic.
float3 DenoiserVarianceClipMin(DenoiserMoments moments, float3 filtered_raw, float clip_gamma)
{
	float3 mean = moments.weight_sum_ > 0.0001 ? moments.moment1_sum_ / moments.weight_sum_ : filtered_raw;
	float3 mean_of_squares = moments.weight_sum_ > 0.0001 ? moments.moment2_sum_ / moments.weight_sum_ : filtered_raw * filtered_raw;
	float3 variance = max(mean_of_squares - mean * mean, 0.0);
	float3 clip_radius = clip_gamma * sqrt(variance);
	float3 band_min = min(moments.neighborhood_min_, filtered_raw);
	return max(mean - clip_radius, band_min);
}

float3 DenoiserVarianceClipMax(DenoiserMoments moments, float3 filtered_raw, float clip_gamma)
{
	float3 mean = moments.weight_sum_ > 0.0001 ? moments.moment1_sum_ / moments.weight_sum_ : filtered_raw;
	float3 mean_of_squares = moments.weight_sum_ > 0.0001 ? moments.moment2_sum_ / moments.weight_sum_ : filtered_raw * filtered_raw;
	float3 variance = max(mean_of_squares - mean * mean, 0.0);
	float3 clip_radius = clip_gamma * sqrt(variance);
	float3 band_max = max(moments.neighborhood_max_, filtered_raw);
	return min(mean + clip_radius, band_max);
}

// Reprojects the previous frame's accumulated result at previous_uv, clamps
// it into [clip_min, clip_max], and exponentially blends it toward
// filtered_raw. Falls back to filtered_raw outright when previous_uv is
// off-screen (camera cut/edge) or the reprojected texel's alpha marks it as
// background (disocclusion) - in both cases the history is not trustworthy.
float3 DenoiserTemporalBlend(Texture2D<float4> history_texture, float2 previous_uv, float3 clip_min, float3 clip_max, float3 filtered_raw, float blend_alpha)
{
	if (previous_uv.x < 0.0 || previous_uv.x > 1.0 || previous_uv.y < 0.0 || previous_uv.y > 1.0)
	{
		return filtered_raw;
	}

	float4 history_sample = history_texture.SampleLevel(sampler_linear_clamp, previous_uv, 0);
	if (history_sample.a <= 0.5)
	{
		return filtered_raw;
	}

	float3 history_value = clamp(history_sample.rgb, clip_min, clip_max);
	return lerp(history_value, filtered_raw, blend_alpha);
}

// Fixed 5-tap B3-spline kernel (Dammertz et al. 2010's standard A-Trous
// weights), separable across x/y. Normalized to sum to 1 over the 1D taps
// (0.0625+0.25+0.375+0.25+0.0625 = 1), so the 2D outer product used in
// DenoiserATrousPass below also sums to 1 before edge-stopping is applied.
static const float DENOISER_ATROUS_KERNEL[5] = { 0.0625, 0.25, 0.375, 0.25, 0.0625 };

// One A-Trous ("with holes") wavelet pass (Dammertz et al. 2010): samples a
// 5x5 grid at 'step' pixel spacing (taps at step*{-2,-1,0,1,2} per axis)
// instead of the usual contiguous 5x5, weighting each tap by the B3-spline
// kernel above times the same depth/normal edge-stopping weight
// DenoiserSpatialWeight uses elsewhere (passed the UNSCALED 1-pixel depth
// gradient - its "expected depth" extrapolation already accounts for the
// tap offset, including when that offset is scaled by 'step'). Invalid taps
// (source.a == 0, e.g. background) are excluded from the average, same
// exclusion rule as every other spatial filter here.
//
// Calling this repeatedly with step doubling each time (1, 2, 4, ...) grows
// the effective spatial support radius geometrically (N passes cover a
// (2^(N+1)-1)-pixel radius) while the per-pixel tap count stays fixed at 25,
// so cost grows linearly with pass count instead of quadratically with
// radius - the whole reason A-Trous exists over one big single-pass blur.
// Each pass is a separate compute dispatch (a thread cannot see pixels
// another thread in the same dispatch hasn't finished writing yet), so the
// caller is responsible for ping-ponging source/dest textures and issuing
// one Dispatch() per pass - see GlobalIlluminationATrousCS.hlsl.
float3 DenoiserATrousPass(Texture2D<float4> source_texture, Texture2D<float> depth_texture, Texture2D<float4> normal_texture, int2 pixel, int2 screen_max, float center_depth, float2 depth_gradient, float3 center_normal, int step, float depth_sharpness, float normal_power)
{
	float3 sum = float3(0, 0, 0);
	float weight_sum = 0.0;

	[unroll]
	for (int ty = -2; ty <= 2; ++ty)
	{
		[unroll]
		for (int tx = -2; tx <= 2; ++tx)
		{
			int2 offset = int2(tx, ty) * step;
			int2 neighbor = clamp(pixel + offset, int2(0, 0), screen_max);

			float4 neighbor_sample = source_texture.Load(int3(neighbor, 0));
			float neighbor_depth = depth_texture.Load(int3(neighbor, 0));
			float3 neighbor_normal = OctNormalDecode(normal_texture.Load(int3(neighbor, 0)).rg);

			float kernel_weight = DENOISER_ATROUS_KERNEL[tx + 2] * DENOISER_ATROUS_KERNEL[ty + 2];
			float edge_weight = DenoiserSpatialWeight(center_depth, depth_gradient, offset, neighbor_depth, center_normal, neighbor_normal, depth_sharpness, normal_power);

			float weight = kernel_weight * edge_weight * neighbor_sample.a;
			sum += neighbor_sample.rgb * weight;
			weight_sum += weight;
		}
	}

	return weight_sum > 0.0001 ? sum / weight_sum : source_texture.Load(int3(pixel, 0)).rgb;
}

#endif // __DENOISER_HLSL__
