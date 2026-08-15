#ifndef __DENOISER_HLSL__
#define __DENOISER_HLSL__

#include "Sampler.hlsli"
#include "Normal.hlsli"

/**
* [EN]
* Reference:
* - https://jo.dreggn.org/home/2010_atrous.pdf
* - https://research.nvidia.com/publication/2017-07_spatiotemporal-variance-guided-filtering-real-time-reconstruction-path-traced
* - https://github.com/NVIDIAGameWorks/Falcor/tree/master/Source/RenderPasses/SVGFPass
*
* Shared building blocks for the RT spatio-temporal denoisers. Two
* independent groups live here:
*
*  1. The "classic" group (AO / GI): a depth+normal weighted bilateral spatial
*     filter, a variance-clipped temporal reprojection blend, and a multi-pass
*     A-Trous wavelet filter.
*  2. The SVGF group (Shadow / Reflection): Schied et al. 2017's spatiotemporal
*     variance-guided filter - moment accumulation, variance estimation and the
*     variance-driven edge-stopping function. Reflection extends this with a
*     dual (surface + hit-point virtual motion) reprojection - see
*     ReflectionDenoiseCS.hlsl.
*
* English only, because this file is #included by other shaders and DXC does
* not accept non-ASCII in an included translation unit.
*/

/**
* [EN]
* Depth+normal bilateral weight for one neighbor sample. expected_depth comes
* from extrapolating the center pixel's local depth gradient to the neighbor
* offset (a "plane fit"), so tilted-but-coplanar neighbors are not penalized
* just for having a different raw depth value - only neighbors that deviate
* from the extrapolated plane (i.e. a different surface) are.
*/
float DenoiserSpatialWeight(float center_depth, float2 depth_gradient, int2 offset, float neighbor_depth, float3 center_normal, float3 neighbor_normal, float depth_sharpness, float normal_power)
{
	float expected_depth = center_depth + dot(depth_gradient, float2(offset));
	float depth_difference = abs(neighbor_depth - expected_depth) / max(center_depth, 0.0001);
	float depth_weight = exp(-depth_difference * depth_sharpness);

	float normal_weight = pow(saturate(dot(center_normal, neighbor_normal)), normal_power);

	return depth_weight * normal_weight;
}

/**
* [EN]
* Central-difference depth gradient at pixel, used by DenoiserSpatialWeight
* above to extrapolate the local surface plane to each neighbor offset.
*/
float2 DenoiserDepthGradient(Texture2D<float> depth_texture, int2 pixel, int2 screen_max)
{
	float depth_right = depth_texture.Load(int3(clamp(pixel + int2(1, 0), int2(0, 0), screen_max), 0));
	float depth_left = depth_texture.Load(int3(clamp(pixel + int2(-1, 0), int2(0, 0), screen_max), 0));
	float depth_down = depth_texture.Load(int3(clamp(pixel + int2(0, 1), int2(0, 0), screen_max), 0));
	float depth_up = depth_texture.Load(int3(clamp(pixel + int2(0, -1), int2(0, 0), screen_max), 0));
	return float2(depth_right - depth_left, depth_down - depth_up) * 0.5;
}

/**
* [EN]
* Running 3x3 color-moment accumulator (mean/variance) feeding the variance
* clipping box below. Callers accumulate into this alongside their own
* (possibly wider, e.g. 5x5) spatial-filter loop, restricted to the inner 3x3
* neighborhood - widening the moment window softens history rejection too much.
*/
struct DenoiserMoments
{
	/// [EN] Sum of the per-neighbor weights, the divisor of both moment sums.
	float weight_sum_;

	/// [EN] Weighted sum of the neighbor values (first raw moment).
	float3 moment1_sum_;

	/// [EN] Weighted sum of the squared neighbor values (second raw moment).
	float3 moment2_sum_;

	/// [EN] Unweighted min/max of the raw neighborhood, the band the variance
	///      box below is intersected with.
	float3 neighborhood_min_;
	float3 neighborhood_max_;
};

/**
* [EN]
* Zero-initializes a DenoiserMoments accumulator.
*/
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

/**
* [EN]
* Folds one weighted neighbor sample into the accumulator.
*/
void DenoiserMomentsAccumulate(inout DenoiserMoments moments, float3 sample_value, float weight)
{
	moments.moment1_sum_ += sample_value * weight;
	moments.moment2_sum_ += sample_value * sample_value * weight;
	moments.weight_sum_ += weight;
	moments.neighborhood_min_ = min(moments.neighborhood_min_, sample_value);
	moments.neighborhood_max_ = max(moments.neighborhood_max_, sample_value);
}

/**
* [EN]
* Variance clipping box (Salvi/Karis-style): a mean +/- gamma*stddev box
* intersected with the raw 3x3 min/max band (widened to include filtered_raw so
* the box never inverts). Used to clamp the reprojected history sample before
* blending, rejecting ghosting from disocclusion, camera cuts, or the
* uninitialized first frame far more cheaply than a full history-rejection
* heuristic.
*/
float3 DenoiserVarianceClipMin(DenoiserMoments moments, float3 filtered_raw, float clip_gamma)
{
	float3 mean = moments.weight_sum_ > 0.0001 ? moments.moment1_sum_ / moments.weight_sum_ : filtered_raw;
	float3 mean_of_squares = moments.weight_sum_ > 0.0001 ? moments.moment2_sum_ / moments.weight_sum_ : filtered_raw * filtered_raw;
	float3 variance = max(mean_of_squares - mean * mean, 0.0);
	float3 clip_radius = clip_gamma * sqrt(variance);
	float3 band_min = min(moments.neighborhood_min_, filtered_raw);
	return max(mean - clip_radius, band_min);
}

/**
* [EN]
* Upper bound of the variance clipping box described above.
*/
float3 DenoiserVarianceClipMax(DenoiserMoments moments, float3 filtered_raw, float clip_gamma)
{
	float3 mean = moments.weight_sum_ > 0.0001 ? moments.moment1_sum_ / moments.weight_sum_ : filtered_raw;
	float3 mean_of_squares = moments.weight_sum_ > 0.0001 ? moments.moment2_sum_ / moments.weight_sum_ : filtered_raw * filtered_raw;
	float3 variance = max(mean_of_squares - mean * mean, 0.0);
	float3 clip_radius = clip_gamma * sqrt(variance);
	float3 band_max = max(moments.neighborhood_max_, filtered_raw);
	return min(mean + clip_radius, band_max);
}

/**
* [EN]
* Reprojects the previous frame's accumulated result at previous_uv, clamps it
* into [clip_min, clip_max], and exponentially blends it toward filtered_raw.
* Falls back to filtered_raw outright when previous_uv is off-screen (camera
* cut/edge) or the reprojected texel's alpha marks it as background
* (disocclusion) - in both cases the history is not trustworthy.
*/
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

/// [EN] Largest value representable in FP16, the format every RT signal
///      buffer here uses. Radiance above this stores as +Inf.
static const float DENOISER_FP16_MAX = 65504.0;

/**
* [EN]
* Front-end sanitization for a traced radiance sample. Not defensive padding -
* it is required. The signal buffers are FP16, so any radiance above 65504 (a
* bright specular highlight or an emissive surface reaches that easily) is
* stored as +Inf. A denoiser cannot survive that: computing moments does
* Inf - Inf and weighting a tap does Inf * 0, both of which are NaN, and
* because the result is fed back as history the NaN is latched in rather than
* passing. From there it leaks into the HDR target and spreads through bloom
* and lens flare, so the visible artifact is shaped like whatever post-process
* spread it and appears wherever a too-bright sample happens to be in frame -
* which makes it look view-dependent and unrelated to the denoiser.
*/
float3 DenoiserSanitizeRadiance(float3 radiance)
{
	bool invalid = any(isnan(radiance)) || any(isinf(radiance));
	return invalid ? float3(0, 0, 0) : clamp(radiance, 0.0, DENOISER_FP16_MAX);
}

/// [EN] Fixed 5-tap B3-spline kernel (Dammertz et al. 2010's standard A-Trous
///      weights), separable across x/y. Normalized to sum to 1 over the 1D taps
///      (0.0625+0.25+0.375+0.25+0.0625 = 1), so the 2D outer product used in
///      DenoiserATrousPass below also sums to 1 before edge-stopping is applied.
static const float DENOISER_ATROUS_KERNEL[5] = { 0.0625, 0.25, 0.375, 0.25, 0.0625 };

/**
* [EN]
* One A-Trous ("with holes") wavelet pass (Dammertz et al. 2010): samples a 5x5
* grid at 'step' pixel spacing (taps at step*{-2,-1,0,1,2} per axis) instead of
* the usual contiguous 5x5, weighting each tap by the B3-spline kernel above
* times the same depth/normal edge-stopping weight DenoiserSpatialWeight uses
* elsewhere (passed the UNSCALED 1-pixel depth gradient - its "expected depth"
* extrapolation already accounts for the tap offset, including when that offset
* is scaled by 'step'). Invalid taps (source.a == 0, e.g. background) are
* excluded from the average, same exclusion rule as every other spatial filter
* here.
*
* Calling this repeatedly with step doubling each time (1, 2, 4, ...) grows the
* effective spatial support radius geometrically (N passes cover a
* (2^(N+1)-1)-pixel radius) while the per-pixel tap count stays fixed at 25, so
* cost grows linearly with pass count instead of quadratically with radius - the
* whole reason A-Trous exists over one big single-pass blur. Each pass is a
* separate compute dispatch (a thread cannot see pixels another thread in the
* same dispatch hasn't finished writing yet), so the caller is responsible for
* ping-ponging source/dest textures and issuing one Dispatch() per pass.
*/
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

/**
* [EN]
* View-space position of a pixel from its UV and its raw (reverse-Z) depth. The
* SVGF group below works in view/world space rather than in raw depth, because
* the reprojection consistency tests it uses are metric.
*/
float3 DenoiserViewPosition(float4x4 inverse_projection, float2 uv, float depth)
{
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 view_position = mul(float4(ndc, depth, 1.0), inverse_projection);
	return view_position.xyz / view_position.w;
}

/**
* [EN]
* World-space position of a pixel from its UV and its raw (reverse-Z) depth.
*/
float3 DenoiserWorldPosition(float4x4 inverse_view_projection, float2 uv, float depth)
{
	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 world_position = mul(float4(ndc, depth, 1.0), inverse_view_projection);
	return world_position.xyz / world_position.w;
}

/**
* [EN]
* Reprojects a pixel to its position in the previous frame. The G-Buffer
* velocity is written as (current_ndc - previous_ndc) * 0.5, and the NDC->UV
* flip negates y, so the UV-space displacement is (velocity.x, -velocity.y).
*/
float2 DenoiserPreviousUv(float2 uv, float2 velocity)
{
	return uv - float2(velocity.x, -velocity.y);
}

/**
* [EN]
* ===========================================================================
* SVGF - Spatiotemporal Variance-Guided Filtering (Schied et al., HPG 2017).
* ===========================================================================
*
* The idea the whole filter rests on: how wide a blur a pixel needs is not a
* constant, it is a function of how noisy that pixel actually is right now.
* SVGF measures that per pixel by accumulating the first and second temporal
* moments of the signal, deriving the variance from them, and using the
* variance to size the luminance edge-stopping term of an A-Trous wavelet
* filter. Converged pixels end up with near-zero variance, which collapses the
* luminance term and stops the filter from blurring them at all; freshly
* disoccluded pixels have high variance and get filtered aggressively.
*/

/**
* [EN]
* Packing for the per-pixel geometry the SVGF passes need from the PREVIOUS
* frame (the temporal consistency test compares this frame's surface against
* the reprojected pixel's surface, so a copy has to survive the frame): view
* depth, its screen-space derivative, and the octahedral normal.
*/
float4 SvgfPackDepthNormal(float view_z, float view_z_derivative, float3 normal)
{
	return float4(view_z, view_z_derivative, OctNormalEncode(normal));
}

/**
* [EN]
* Decodes the normal out of a texel packed by SvgfPackDepthNormal.
*/
float3 SvgfUnpackNormal(float4 packed_depth_normal)
{
	return OctNormalDecode(packed_depth_normal.zw);
}

/**
* [EN]
* Screen-space rate of change of view depth, central differences. Used as the
* scale of the depth edge-stopping term: a steeply slanted surface has a large
* raw depth difference between neighbors without being a different surface, so
* the tolerance has to follow the local slope rather than be a fixed epsilon.
*/
float SvgfViewDepthDerivative(float left, float right, float up, float down)
{
	return max(abs(right - left), abs(down - up)) * 0.5;
}

/**
* [EN]
* Depth * normal edge-stopping weight. phi_depth already includes the A-Trous
* step size and the tap distance, so a tap that is 4 pixels away is allowed 4x
* the depth deviation of an adjacent one.
*/
float SvgfDepthNormalWeight(float center_view_z, float neighbor_view_z, float phi_depth, float3 center_normal, float3 neighbor_normal, float phi_normal)
{
	float normal_weight = pow(saturate(dot(center_normal, neighbor_normal)), phi_normal);
	float depth_weight = phi_depth <= 0.0 ? 0.0 : abs(center_view_z - neighbor_view_z) / phi_depth;

	return exp(-max(depth_weight, 0.0)) * normal_weight;
}

/**
* [EN]
* The variance-guided half of the edge-stopping function, evaluated per channel.
* phi is sigma_l * sqrt(variance): as a pixel converges, variance falls, phi
* collapses, and any luminance difference at all drives the weight to zero -
* which is exactly the "stop filtering what is already clean" behaviour that
* separates SVGF from a plain bilateral A-Trous.
*/
float2 SvgfLuminanceWeight(float2 center_luminance, float2 neighbor_luminance, float2 phi_luminance)
{
	return exp(-abs(center_luminance - neighbor_luminance) / phi_luminance);
}

/**
* [EN]
* 3x3 Gaussian prefilter of the variance channels before they are used to size
* phi_luminance above. Variance is itself a noisy per-pixel estimate; without
* this smoothing the filter width flickers pixel to pixel and the result boils.
*/
float2 SvgfVarianceCenter(Texture2D<float4> source_texture, int2 pixel, int2 screen_max)
{
	const float kernel[3] = { 1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0 };

	float2 sum = float2(0, 0);

	[unroll]
	for (int dy = -1; dy <= 1; ++dy)
	{
		[unroll]
		for (int dx = -1; dx <= 1; ++dx)
		{
			int2 neighbor = clamp(pixel + int2(dx, dy), int2(0, 0), screen_max);
			sum += source_texture.Load(int3(neighbor, 0)).ba * kernel[abs(dx) + abs(dy)];
		}
	}

	return sum;
}

/// [EN] Separable 5-tap A-Trous kernel used by the SVGF wavelet passes. This is
///      the paper's h = (1/16, 1/4, 3/8, 1/4, 1/16) expressed relative to the
///      center tap, because SVGF weights the center explicitly at 1 and never
///      lets edge-stopping reject it.
static const float SVGF_ATROUS_KERNEL[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

#endif // __DENOISER_HLSL__
