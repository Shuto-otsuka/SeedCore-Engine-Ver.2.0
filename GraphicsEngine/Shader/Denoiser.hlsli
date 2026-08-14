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
* - https://github.com/NVIDIA-RTX/NRD
* - https://link.springer.com/chapter/10.1007/978-1-4842-7185-8_49
*
* Shared building blocks for the RT spatio-temporal denoisers. Three
* independent groups live here:
*
*  1. The "classic" group (AO / GI): a depth+normal weighted bilateral spatial
*     filter, a variance-clipped temporal reprojection blend, and a multi-pass
*     A-Trous wavelet filter.
*  2. The SVGF group (Shadow): Schied et al. 2017's spatiotemporal
*     variance-guided filter - moment accumulation, variance estimation and the
*     variance-driven edge-stopping function.
*  3. The ReBLUR group (Reflection): NVIDIA NRD's hierarchical recurrent
*     denoiser - hit-distance-driven blur radius, accumulation-speed
*     bookkeeping, the anisotropic world-space Poisson kernel and the specular
*     weight family it filters with.
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
* SVGF and ReBLUR groups below work in view/world space rather than in raw
* depth, because the plane-distance and curvature tests they use are metric.
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
* Ordered dithering used to rotate the ReBLUR Poisson disk per pixel per frame,
* so its 8 fixed taps do not bake a static pattern into the result.
*/
float DenoiserBayer4x4(uint2 pixel, uint frame_index)
{
	uint2 wrapped = pixel & 3;
	uint a = 2068378560 * (1 - (wrapped.y >> 1)) + 1500172770 * (wrapped.y >> 1);
	uint b = (wrapped.x + ((wrapped.y & 1) << 2)) << 2;
	uint bayer = ((a >> b) + frame_index) & 0xF;
	return float(bayer) / 16.0;
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

/**
* [EN]
* ===========================================================================
* ReBLUR - NVIDIA Real-time Denoisers' hierarchical recurrent denoiser.
* ===========================================================================
*
* Where SVGF sizes its filter from measured variance, ReBLUR sizes it from
* geometry: the blur radius of a specular pixel is derived from the distance
* the reflection ray travelled and from how many frames of history the pixel
* already has. A short ray means the reflected detail is close to the surface
* and must stay sharp; a long ray means the lobe footprint is wide and can be
* blurred. The kernel is a world-space ellipse oriented along the specular
* dominant direction, so the blur follows the shape of the GGX lobe instead of
* being an isotropic screen-space disk.
*/

/// [EN] "Special 8" Poisson-like tap set: 4 taps on the unit circle plus 4
///      inner taps at half radius. z is the normalized radius, fed to
///      ReblurGaussianWeight below.
static const float3 REBLUR_POISSON_SAMPLES[8] =
{
	float3(-1.0, 0.0, 1.0),
	float3(0.0, 1.0, 1.0),
	float3(1.0, 0.0, 1.0),
	float3(0.0, -1.0, 1.0),
	float3(-0.25 * 1.41421356, 0.25 * 1.41421356, 0.5),
	float3(0.25 * 1.41421356, 0.25 * 1.41421356, 0.5),
	float3(0.25 * 1.41421356, -0.25 * 1.41421356, 0.5),
	float3(-0.25 * 1.41421356, -0.25 * 1.41421356, 0.5)
};

/// [EN] Fraction of the GGX lobe volume the normal rejection width is taken
///      from, and the tighter fraction the pre-pass radius clamp uses.
static const float REBLUR_MAX_PERCENT_OF_LOBE_VOLUME = 0.75;
static const float REBLUR_MAX_PERCENT_OF_LOBE_VOLUME_FOR_PRE_PASS = 0.3;

/// [EN] Angular error floor of the octahedral normal encoding. Any rejection
///      angle narrower than this would be rejecting quantization noise.
static const float REBLUR_NORMAL_ENCODING_ERROR = 1.5 / 255.0;

/// [EN] Smallest roughness delta the roughness weight can resolve - the
///      denominator floor that keeps mirrors from accepting rough neighbors.
static const float REBLUR_ROUGHNESS_SENSITIVITY = 0.01;

/// [EN] Steepness of the exponential weight, chosen so its energy matches the
///      smoothstep weight it is interchangeable with.
static const float REBLUR_EXP_WEIGHT_SCALE = 3.0;

static const float REBLUR_EPSILON = 1e-6;

/**
* [EN]
* Roughness remap that governs almost every roughness-dependent quantity in
* ReBLUR. It is ~0 for mirror-like surfaces (no blur, no relaxation of any
* weight) and rises to 1 for fully rough ones, but unlike sqrt(roughness) it
* stays correct below roughness 0.1, where a mirror must not be touched.
*/
float ReblurSpecMagicCurve(float roughness, float power)
{
	float f = 1.0 - exp2(-200.0 * roughness * roughness);
	return f * pow(saturate(roughness), power);
}

/**
* [EN]
* Half-angle of the cone containing percent_of_volume of the GGX lobe, as a
* tangent. Everything that has to stay "in lobe" - the pre-pass radius clamp,
* the normal rejection width - is expressed through this.
*/
float ReblurSpecularLobeTanHalfAngle(float roughness, float percent_of_volume)
{
	percent_of_volume = saturate(percent_of_volume);
	return saturate(roughness) * sqrt(percent_of_volume / (1.0 - percent_of_volume + REBLUR_EPSILON));
}

/**
* [EN]
* Karis' G2 fit for how far the GGX lobe's dominant direction leans from the
* normal toward the mirror direction. 1 = pure mirror, 0 = pure normal.
*/
float ReblurSpecularDominantFactor(float normal_dot_view, float roughness)
{
	roughness = saturate(roughness);
	float a = 0.298475 * log(39.4115 - 39.0029 * roughness);
	return saturate(pow(saturate(1.0 - normal_dot_view), 10.8649) * (1.0 - a) + a);
}

/**
* [EN]
* Specular dominant direction: xyz = the direction itself, w = the dominant
* factor it was built from (ReblurVirtualPosition needs the factor as an
* elongation term, so it is returned rather than recomputed).
*/
float4 ReblurSpecularDominantDirection(float3 normal, float3 view, float roughness)
{
	float normal_dot_view = abs(dot(normal, view));
	float dominant_factor = ReblurSpecularDominantFactor(normal_dot_view, roughness);
	float3 reflection = reflect(-view, normal);

	return float4(normalize(lerp(normal, reflection, dominant_factor)), dominant_factor);
}

/**
* [EN]
* Branchless orthonormal basis around n (Frisvad/Duff), rows = (T, B, N).
*/
float3x3 ReblurOrthoBasis(float3 n)
{
	float sign_z = n.z >= 0.0 ? 1.0 : -1.0;
	float a = 1.0 / (sign_z + n.z);
	float ya = n.y * a;
	float b = n.x * ya;
	float c = n.x * sign_z;

	float3 tangent = float3(c * n.x * a - 1.0, sign_z * b, c);
	float3 bitangent = float3(b, n.y * ya - sign_z, n.y);

	return float3x3(tangent, bitangent, n);
}

/**
* [EN]
* Kernel basis for the spatial filter: the tangent is put PERPENDICULAR to the
* reflected direction and the bitangent along it, so scaling the two axes apart
* (see the skew factor at the call site) stretches the sampling ellipse along
* the direction the specular lobe is actually elongated in at grazing angles.
* Falls back to an arbitrary basis when the direction is (anti)parallel to the
* normal, where the lobe is rotationally symmetric and orientation is
* meaningless.
*/
float2x3 ReblurKernelBasis(float3 direction, float3 normal)
{
	float3x3 basis = ReblurOrthoBasis(normal);
	float3 tangent = basis[0];
	float3 bitangent = basis[1];

	if (abs(dot(direction, normal)) < 0.999)
	{
		float3 reflection = reflect(-direction, normal);
		tangent = normalize(cross(normal, reflection));
		bitangent = cross(reflection, tangent);
	}

	return float2x3(tangent, bitangent);
}

/**
* [EN]
* World units a single pixel spans per unit of view depth, derived from the
* projection. Every pixel-space radius in the ReBLUR passes goes through this to
* become a world-space kernel axis.
*/
float ReblurUnproject(float projection_m11, float screen_height)
{
	return 1.0 / (0.5 * screen_height * projection_m11);
}

/**
* [EN]
* World-space size of a pixel_radius-wide disk at view_z.
*/
float ReblurPixelRadiusToWorld(float unproject, float pixel_radius, float view_z)
{
	return pixel_radius * unproject * view_z;
}

/**
* [EN]
* World-space size of the view frustum at view_z, the yardstick the hit distance
* and the plane-distance tolerance are both measured against.
*/
float ReblurFrustumSize(float unproject, float2 screen_size, float view_z)
{
	return min(screen_size.x, screen_size.y) * unproject * view_z;
}

/**
* [EN]
* Hit distances are stored normalized to [0;1] so they survive an FP16 alpha
* channel at any scene scale. The normalization is depth- and roughness-aware:
* params.x is a constant floor in world units, params.y scales it with view
* depth, and params.z stretches the range for low roughness, where a mirror
* legitimately reflects things very far away.
*/
float ReblurHitDistanceNormalization(float view_z, float3 hit_distance_params, float roughness)
{
	float smc = ReblurSpecMagicCurve(roughness, 0.5);
	return (hit_distance_params.x + abs(view_z) * hit_distance_params.y) * lerp(hit_distance_params.z, 1.0, smc);
}

/**
* [EN]
* How large the reflection's footprint is relative to the view frustum at this
* depth. This is the primary driver of the blur radius: a ray that travelled a
* distance comparable to the frustum reflects something far away and blurry.
*/
float ReblurHitDistFactor(float hit_distance, float frustum_size)
{
	return saturate(hit_distance / frustum_size);
}

/**
* [EN]
* Plane-distance rejection parameters. Measures how far a neighbor is from the
* center pixel's TANGENT PLANE rather than from its depth, so coplanar neighbors
* on a slanted surface are kept and only genuinely different surfaces are
* rejected. Returns (a, b) for the |x * a + b| form the weight evaluators take.
*/
float2 ReblurGeometryWeightParams(float plane_distance_sensitivity, float frustum_size, float3 view_position, float3 view_normal)
{
	float a = 1.0 / (plane_distance_sensitivity * frustum_size);
	return float2(a, -dot(view_normal, view_position) * a);
}

/**
* [EN]
* Normal rejection width, expressed as the reciprocal of an angle taken from the
* GGX lobe itself. It widens as history shortens (non_linear_accum_speed -> 1),
* because a pixel with no history needs neighbors more than it needs precision.
*/
float ReblurNormalWeightParam(float non_linear_accum_speed, float lobe_angle_fraction, float roughness)
{
	float percent_of_volume = REBLUR_MAX_PERCENT_OF_LOBE_VOLUME * lerp(saturate(lobe_angle_fraction), 1.0, non_linear_accum_speed);
	float tan_half_angle = ReblurSpecularLobeTanHalfAngle(roughness, percent_of_volume);

	return 1.0 / max(atan(tan_half_angle), REBLUR_NORMAL_ENCODING_ERROR);
}

/**
* [EN]
* Roughness rejection parameters - keeps a mirror from borrowing a rough
* neighbor's wide highlight and vice versa.
*/
float2 ReblurRoughnessWeightParams(float roughness, float fraction)
{
	float a = 1.0 / lerp(REBLUR_ROUGHNESS_SENSITIVITY, 1.0, saturate(roughness * fraction));
	return float2(a, -roughness * a);
}

/**
* [EN]
* Hit-distance rejection parameters. The tolerance is the reciprocal of the
* non-linear accumulation speed, so a converged pixel demands neighbors that
* reflect something at nearly the same distance, while a fresh one accepts
* anything.
*/
float2 ReblurHitDistanceWeightParams(float hit_distance, float non_linear_accum_speed)
{
	float a = 1.0 / non_linear_accum_speed;
	return float2(a, -hit_distance * a);
}

/**
* [EN]
* Smoothstep-shaped weight, for data that is not noisy (normals, roughness,
* geometry) - it reaches exactly 0 at the rejection threshold instead of
* trailing off, which keeps unrelated surfaces fully out.
*/
float ReblurComputeWeight(float x, float px, float py)
{
	return smoothstep(0.0, 1.0, saturate(1.0 - abs(x * px + py)));
}

/**
* [EN]
* Exponential weight, for noisy data (hit distances). Uses the standard rational
* approximation of exp, valid for the negative arguments used here.
*/
float ReblurComputeExponentialWeight(float x, float px, float py)
{
	float t = -REBLUR_EXP_WEIGHT_SCALE * abs(x * px + py);
	return rcp(t * t - t + 1.0);
}

/**
* [EN]
* Radial falloff across the Poisson disk, applied to each tap's normalized
* radius so the outer ring contributes less than the inner one.
*/
float ReblurGaussianWeight(float radius)
{
	return exp(-0.66 * radius * radius);
}

/**
* [EN]
* Mirrors a UV back inside [0;1] instead of clamping it, so taps that fall off
* screen land on real neighboring data rather than all collapsing onto the same
* edge texel.
*/
float2 ReblurMirrorUv(float2 uv)
{
	return min(1.0 - abs(1.0 - frac(uv * 0.5) * 2.0), 0.99999);
}

/**
* [EN]
* Projects one kernel tap (a world-space offset along the ellipse axes) back to
* screen UV. Sampling in view space and projecting is what makes the kernel
* foreshorten correctly on surfaces seen at an angle. rotator is
* (cos angle, sin angle) of the per-pixel disk rotation.
*/
float2 ReblurKernelSampleUv(float4x4 view_to_clip, float3 offset, float3 view_position, float3 tangent, float3 bitangent, float2 rotator)
{
	float2 rotated = float2(offset.x * rotator.x - offset.y * rotator.y, offset.x * rotator.y + offset.y * rotator.x);

	float3 sample_position = view_position + tangent * rotated.x + bitangent * rotated.y;
	float4 clip = mul(float4(sample_position, 1.0), view_to_clip);

	float2 ndc = clip.xy / max(clip.w, REBLUR_EPSILON);
	return float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

/**
* [EN]
* Where the reflected image APPEARS to live, via the thin-lens equation with the
* surface acting as a mirror of the given curvature. Reflections do not move
* with the surface they sit on - they move with the reflected geometry - so
* reprojecting them along the G-Buffer motion vector alone smears them under
* camera motion. ReBLUR reprojects a virtual point behind/in front of the mirror
* instead, which is what keeps specular history aligned.
*/
float3 ReblurVirtualPosition(float hit_distance, float curvature, float3 world_position, float3 previous_world_position, float3 normal, float3 view, float roughness)
{
	float4 dominant = ReblurSpecularDominantDirection(normal, view, roughness);
	float3 reflection_ray = dominant.xyz * hit_distance;

	float3x3 reflector_basis = ReblurOrthoBasis(normal);
	float3 object_position = mul(reflector_basis, reflection_ray);
	object_position.z = -object_position.z;

	float magnification = 1.0 / (2.0 * curvature * object_position.z - 1.0);

	/// [EN] Convex silhouettes magnify without bound, which smears the history
	///      across the object's edge - damp the magnification there.
	float normal_dot_view = abs(dot(normal, view));
	float damping = length(world_position) * saturate(1.0 - normal_dot_view) * max(curvature, 0.0);
	magnification *= 1.0 / (1.0 + damping);

	float3 image_position = object_position * magnification;
	float elongation = dominant.w * length(image_position);

	/// [EN] When the virtual image focuses back onto the surface, surface motion
	///      is the better estimate of where it went - blend toward it.
	float closeness_to_surface = saturate(elongation / (hit_distance + REBLUR_EPSILON));
	float3 anchor = lerp(previous_world_position, world_position, closeness_to_surface);

	return anchor + view * elongation * (magnification >= 0.0 ? 1.0 : -1.0);
}

#endif // __DENOISER_HLSL__
