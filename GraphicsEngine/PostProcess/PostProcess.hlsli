#ifndef __POST_PROCESS_HLSL__
#define __POST_PROCESS_HLSL__

#include "../Shader/Constants.hlsli"

// Upper bound on LensFlareCS.hlsl's independently-blurred spike axes (one
// axis = one line = two opposing arms). Diffraction through an n-bladed
// iris yields n spikes when n is even and 2n when n is odd, because each
// blade edge diffracts perpendicular to itself and on an even-bladed iris
// the opposing edges are parallel so their spikes coincide. That makes the
// axis count n/2 for even n and n for odd n, and BokehSettings::bladeCount_
// (the same physical iris) is clamped to 3..8, so the worst case is n = 7
// -> 14 spikes -> 7 axes. Buffers for all 7 are always allocated; only the
// active ones are dispatched over.
#define LENS_FLARE_MAX_AXIS_COUNT 7

/**
* [EN]
* Per-view auto-exposure tuning (2 rows / 32 bytes). The histogram/exposure
* UAVs live in ExposureUnorderedAccessIndices below - this struct is values
* only.
*/
struct ExposureIndices
{
	uint auto_exposure_enabled_;
	float exposure_compensation_;
	float min_log_luminance_;
	float max_log_luminance_;

	float key_value_;
	float adapt_speed_to_bright_;
	float adapt_speed_to_dark_;
	uint exposure_padding_0_;
};

/**
* [EN]
* Per-view auto-exposure write targets - AutoExposureHistogramCS.hlsl's
* luminance histogram and AutoExposureAverageCS.hlsl's persistent scalar
* exposure value it reduces into.
*/
struct ExposureUnorderedAccessIndices
{
	uint histogram_index_;
	uint exposure_index_;
	float2 exposure_unordered_access_padding_0_;
};

/**
* [EN]
* Per-view tone-mapping tuning (1 row / 16 bytes). No indices -
* ToneMappingCS.hlsl reads/writes through PostProcessConstantBuffer/
* PostProcessShaderResourceIndices/PostProcessUnorderedAccessIndices' own
* fields.
*/
struct ToneMappingIndices
{
	uint tone_mapping_enabled_;
	uint tone_mapping_mode_;
	float2 tone_mapping_padding_0_;
};

/**
* [EN]
* Per-view lens-flare tuning (4 rows / 64 bytes). enabled_ gates both the
* dispatch and ToneMappingCS.hlsl's read of the UAV/SRV pair in
* LensFlareUnorderedAccessIndices/LensFlareShaderResourceIndices below: when
* off, PostProcessRenderer's Dispatch skips LensFlareCS entirely, so
* ToneMappingCS must also skip the read to avoid sampling stale leftover
* data from when it was last enabled.
*/
struct LensFlareIndices
{
	uint enabled_;
	float threshold_;
	float intensity_;
	float streak_length_;

	float streak_attenuation_;
	float chromatic_aberration_;
	float angle_offset_;
	uint ghost_count_;

	float ghost_dispersal_;
	float ghost_intensity_;
	float halo_width_;
	uint axis_count_;

	float spike_variation_;
	float3 lens_flare_padding_0_;
};

/**
* [EN]
* LensFlareCS.hlsl's quarter-res write target and the bindless SRV
* ToneMappingCS.hlsl samples to add the flare into the HDR color before
* exposure.
*/
struct LensFlareUnorderedAccessIndices
{
	uint index_;
	float3 lens_flare_unordered_access_padding_0_;
};

struct LensFlareShaderResourceIndices
{
	uint index_;
	float3 lens_flare_shader_resource_padding_0_;
};

/**
* [EN]
* One spike axis' ping/pong pair (LensFlareCS.hlsl walks
* LensFlareIndices::axis_count_ axes, each a line of two opposing arms,
* each independently multi-pass blurred). Held in a StructuredBuffer rather
* than an inline cbuffer array (see LensFlareStreakUnorderedAccessIndices/
* LensFlareStreakShaderResourceIndices below) - a StructuredBuffer element
* packs tightly (8 bytes, no padding) instead of a cbuffer array element's
* forced 16-byte row, so splitting UAV/SRV into two of these costs no more
* total memory than the single combined array used to.
*/
struct LensFlareStreakAxisIndices
{
	uint ping_index_;
	uint pong_index_;
};

/**
* [EN]
* axis_buffer_index_ points at a StructuredBuffer<LensFlareStreakAxisIndices>
* of LENS_FLARE_MAX_AXIS_COUNT elements (only the first axis_count_ of them
* are read, see LensFlareIndices::axis_count_) holding every axis' UAV
* write targets. bright_index_ is LensFlareCS.hlsl's Downsample entry
* point's write target: a quarter-res, bright-passed copy of the scene
* produced with a 4-tap bilinear filter covering the full 4x4 source
* footprint. Set once per frame alongside LensFlareIndices
* (PostProcessRenderer::CreateView allocates them, PrepareView registers
* the bindless indices) - not re-registered mid-frame, since
* LensFlareCS.hlsl's BlurPass1..4 entry points read/write them by a
* compile-time-fixed ping/pong parity per pass, not a per-dispatch index.
*/
struct LensFlareStreakUnorderedAccessIndices
{
	uint axis_buffer_index_;
	uint bright_index_;
	float2 lens_flare_streak_unordered_access_padding_0_;
};

/**
* [EN]
* Read-side counterpart of LensFlareStreakUnorderedAccessIndices above -
* axis_buffer_index_ points at the SRV-view StructuredBuffer<
* LensFlareStreakAxisIndices>, bright_index_ the bindless SRV both
* BlurPass1 (streaks) and Ghost (ghost chain + halo) sample instead of the
* full-res HDR source, because point-sampling mip 0 at quarter density
* skips 15 of every 16 source pixels and so drops small bright sources
* entirely - the sun disc (a few pixels wide, see
* VolumetricCloudScapes.hlsli::ProceduralSkyColor) would otherwise never
* produce a flare.
*/
struct LensFlareStreakShaderResourceIndices
{
	uint axis_buffer_index_;
	uint bright_index_;
	float2 lens_flare_streak_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view bloom tuning (2 rows / 32 bytes). level0..5 are the 6 levels of
* KawaseBloomCS.hlsl's downsample/upsample chain, level0 being half the
* native resolution and each subsequent level half of the one before. The
* chain writes DOWN through the levels (DownsamplePrefilter then
* Downsample1..5) then accumulates back UP additively (Upsample4..0), so
* level0 ends up holding the final bloom that ToneMappingCS.hlsl samples and
* adds into the HDR color before exposure. enabled_ gates both the dispatch
* and that read, so a stale buffer from when bloom was last on is never
* sampled. filter_radius_ is the 3x3 tent radius in UV used by the upsample
* passes; soft_knee_ widens the threshold_ transition so pixels sitting at
* the cutoff fade in instead of popping.
*/
struct BloomIndices
{
	uint enabled_;
	float threshold_;
	float soft_knee_;
	float intensity_;

	float filter_radius_;
	float3 bloom_padding_0_;
};

/**
* [EN]
* Write targets of KawaseBloomCS.hlsl's 6-level downsample/upsample chain.
*/
struct BloomUnorderedAccessIndices
{
	uint level0_index_;
	uint level1_index_;
	uint level2_index_;
	uint level3_index_;

	uint level4_index_;
	uint level5_index_;
	float2 bloom_unordered_access_padding_0_;
};

/**
* [EN]
* Read views of the same chain as BloomUnorderedAccessIndices above.
*/
struct BloomShaderResourceIndices
{
	uint level0_index_;
	uint level1_index_;
	uint level2_index_;
	uint level3_index_;

	uint level4_index_;
	uint level5_index_;
	float2 bloom_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view anamorphic-flare tuning (3 rows / 48 bytes). The squeeze that
* makes the streak come out horizontal is baked into the ping_/pong_ working
* buffers themselves (half the width of the other quarter-res post-process
* buffers) - see AnamorphicFlareUnorderedAccessIndices/
* AnamorphicFlareShaderResourceIndices below. output_ (also below) is
* Compose's own target, which ToneMappingCS.hlsl samples and adds into the
* HDR color before exposure. enabled_ gates both the dispatch and that read,
* so a stale buffer from when the effect was last on is never sampled.
*/
struct AnamorphicFlareIndices
{
	uint enabled_;
	float threshold_;
	float intensity_;
	float streak_length_;

	float attenuation_;
	float3 anamorphic_flare_padding_0_;

	float4 tint_;
};

struct AnamorphicFlareUnorderedAccessIndices
{
	uint output_index_;
	uint ping_index_;
	uint pong_index_;
	uint anamorphic_flare_unordered_access_padding_0_;
};

struct AnamorphicFlareShaderResourceIndices
{
	uint output_index_;
	uint ping_index_;
	uint pong_index_;
	uint anamorphic_flare_shader_resource_padding_0_;
};

/**
* [EN]
* One tonal range's colour grading controls (2 rows / 32 bytes), in the
* order they are applied. All scalars rather than per-channel: the colour
* axis is handled by temperature_ as a chromatic adaptation instead (see
* ColorGradingRangeSettings in PostProcess.h for why). Neutral is 1 for
* saturation/contrast/gamma/gain and 0 for offset and temperature.
*/
struct ColorGradingRangeIndices
{
	float temperature_;
	float saturation_;
	float contrast_;
	float gamma_;

	float gain_;
	float offset_;
	float2 color_grading_range_padding_0_;
};

/**
* [EN]
* Unreal-style colour grading tuning (9 rows / 144 bytes): four tonal ranges
* each with their own wheels, blended by luminance with smooth crossovers at
* shadows_max_ and highlights_min_. Runs in scene-referred linear space
* AFTER exposure and BEFORE the tone curve, which is forced by the 0.18
* contrast pivot - 0.18 only means middle grey once exposure has placed the
* scene there, and means nothing after the curve has compressed the range.
* Because of that position this pass also owns the additive contributions
* (bloom, lens flare, anamorphic) and the exposure multiply, which
* ToneMappingCS.hlsl skips whenever enabled_ is set.
*/
struct ColorGradingIndices
{
	uint enabled_;
	float shadows_max_;
	float highlights_min_;
	uint color_grading_padding_0_;

	ColorGradingRangeIndices global_;
	ColorGradingRangeIndices shadows_;
	ColorGradingRangeIndices midtones_;
	ColorGradingRangeIndices highlights_;
};

struct ColorGradingUnorderedAccessIndices
{
	uint destination_index_;
	float3 color_grading_unordered_access_padding_0_;
};

/**
* [EN]
* source_index_/output_index_ - source_ is the pre-grade HDR input,
* output_ the graded result ToneMappingCS.hlsl reads back when this pass is
* enabled (part of the lens-stage source-select chain, see
* PostProcessShaderResourceIndices above).
*/
struct ColorGradingShaderResourceIndices
{
	uint source_index_;
	uint output_index_;
	float2 color_grading_shader_resource_padding_0_;
};

/**
* [EN]
* Radial half of the Brown-Conrady distortion model (2 rows / 32 bytes),
* the first stage of the lens chain since it displaces geometry. k1
* dominates, k2 refines the corners, k3 barely moves anything. scale_ zooms
* in before distorting so barrel distortion does not leave empty corners.
*/
struct LensDistortionIndices
{
	uint enabled_;
	float k1_;
	float k2_;
	float k3_;

	float scale_;
	float3 lens_distortion_padding_0_;
};

struct LensDistortionUnorderedAccessIndices
{
	uint destination_index_;
	float3 lens_distortion_unordered_access_padding_0_;
};

struct LensDistortionShaderResourceIndices
{
	uint source_index_;
	float3 lens_distortion_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view chromatic-aberration tuning (1 row / 16 bytes) and vignette
* tuning (2 rows / 32 bytes) below. Both are LENS stage effects: they run
* before auto-exposure and tone mapping, because both describe what reaches
* the sensor rather than how the sensor is developed. They chain through
* one shared buffer - source_index_ (in each SRV struct below) is resolved
* on the CPU in PostProcessRenderer::PrepareView, so if chromatic
* aberration is on the vignette reads its output, otherwise it reads the
* depth-of-field output or the raw scene color. Vignette is a pure
* per-pixel multiply and so may read and write the same texture in place;
* chromatic aberration reads neighbours and may not, which is why it always
* writes the shared lens-stage buffer.
*/
struct ChromaticAberrationIndices
{
	uint enabled_;
	float intensity_;
	uint sample_count_;
	uint chromatic_aberration_padding_0_;
};

struct ChromaticAberrationUnorderedAccessIndices
{
	uint destination_index_;
	float3 chromatic_aberration_unordered_access_padding_0_;
};

struct ChromaticAberrationShaderResourceIndices
{
	uint source_index_;
	float3 chromatic_aberration_shader_resource_padding_0_;
};

struct VignetteIndices
{
	uint enabled_;
	float intensity_;
	float exponent_;
	uint vignette_padding_0_;

	float4 color_;
};

struct VignetteUnorderedAccessIndices
{
	uint destination_index_;
	float3 vignette_unordered_access_padding_0_;
};

struct VignetteShaderResourceIndices
{
	uint source_index_;
	float3 vignette_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view depth-of-field tuning (1 row / 16 bytes). Unlike LensFlareIndices
* this is not an additive contribution - it is a whole replacement HDR
* buffer. When enabled_ is set, LensFlareCS.hlsl and ToneMappingCS.hlsl read
* DepthOfFieldShaderResourceIndices::index_ below instead of
* PostProcessShaderResourceIndices::source_color_index_ (see those files'
* source-select).
*/
struct DepthOfFieldIndices
{
	uint enabled_;
	float focus_distance_;
	float focus_range_;
	float max_blur_radius_;
};

/**
* [EN]
* DepthOfFieldCS.hlsl's native-res write target - BokehCS.hlsl
* read-modify-writes the same UAV, it has no resources of its own.
*/
struct DepthOfFieldUnorderedAccessIndices
{
	uint index_;
	float3 depth_of_field_unordered_access_padding_0_;
};

struct DepthOfFieldShaderResourceIndices
{
	uint index_;
	float3 depth_of_field_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view bokeh-highlight tuning (1 row / 16 bytes). No indices -
* BokehCS.hlsl only runs when this AND DepthOfFieldIndices.enabled_ are both
* set, and scatters shaped highlights into
* DepthOfFieldUnorderedAccessIndices::index_ above; it has no resources of
* its own.
*/
struct BokehIndices
{
	uint enabled_;
	float highlight_threshold_;
	float highlight_intensity_;
	uint blade_count_;
};

/**
* [EN]
* Per-view sharpness tuning (1 row / 16 bytes). SharpnessCS.hlsl runs last,
* after ToneMappingCS.hlsl.
*/
struct SharpnessIndices
{
	uint enabled_;
	float amount_;
	float2 sharpness_padding_0_;
};

/**
* [EN]
* destination_index_ becomes the new final display texture
* (PostProcessRenderer::OutputResource et al. now point at it).
*/
struct SharpnessUnorderedAccessIndices
{
	uint destination_index_;
	float3 sharpness_unordered_access_padding_0_;
};

/**
* [EN]
* source_index_ is the bindless SRV of ToneMappingCS.hlsl's tone-mapped/
* sRGB-encoded output (now an intermediate buffer rather than the final
* display texture).
*/
struct SharpnessShaderResourceIndices
{
	uint source_index_;
	float3 sharpness_shader_resource_padding_0_;
};

/**
* [EN]
* Per-view film-grain tuning (2 rows / 32 bytes). Runs LAST, after
* SharpnessCS.hlsl, and read-modify-writes that pass's output in place (see
* FilmGrainUnorderedAccessIndices below) - safe because grain is a
* per-pixel operation with no neighbour taps, and deliberate so the
* sharpen pass does not amplify the grain it was given. Unlike the
* lens-stage effects this is applied after tone mapping: grain is the
* developed emulsion's density variation, so the tonal position driving
* luminance_response_ only means anything post-curve.
*/
struct FilmGrainIndices
{
	uint enabled_;
	uint colored_;
	float intensity_;
	float size_;

	float luminance_response_;
	float3 film_grain_padding_0_;
};

struct FilmGrainUnorderedAccessIndices
{
	uint destination_index_;
	float3 film_grain_unordered_access_padding_0_;
};

/**
* [EN]
* Per-view post-process tuning. Reached through ConstantIndices, which is
* uploaded separately for each view, for the same reason the shadow/AO
* accumulation chains are (see Denoiser.hlsli): the histogram/persistent-
* exposure buffers and the display output texture are genuinely per-camera
* state (editor and game can be looking at wildly different scenes with
* independently-adapting exposure). The
* UAV/SRV indices for each effect (lens_flare_streak_ included, see
* LensFlareStreakUnorderedAccessIndices/LensFlareStreakShaderResourceIndices
* above) live in PostProcessUnorderedAccessIndices/
* PostProcessShaderResourceIndices below instead of here - this struct is
* values only, so unlike those two it is not embedded directly in
* ConstantIndices: ConstantIndices holds a plain post_process_index_ (like
* scene_index_) and GetPostProcessConstantBuffer() below reaches this
* struct through ResourceDescriptorHeap[], same pattern as
* Scene.hlsli's GetSceneConstantBuffer().
*/
struct PostProcessConstantBuffer
{
	uint lens_stage_enabled_;
	float3 post_process_padding_0_;

	ExposureIndices exposure_;
	ToneMappingIndices tone_mapping_;
	LensFlareIndices lens_flare_;
	BloomIndices bloom_;
	AnamorphicFlareIndices anamorphic_flare_;
	ColorGradingIndices color_grading_;
	LensDistortionIndices lens_distortion_;
	ChromaticAberrationIndices chromatic_aberration_;
	VignetteIndices vignette_;
	DepthOfFieldIndices depth_of_field_;
	BokehIndices bokeh_;
	SharpnessIndices sharpness_;
	FilmGrainIndices film_grain_;
};

ConstantBuffer<PostProcessConstantBuffer> GetPostProcessConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.post_process_index_];
}

/**
* [EN]
* UAV indices for every post-process effect. output_index_ is
* SharpnessCS.hlsl's own final-display write target's earlier alias used
* before that pass existed in the chain - kept here as the pipeline's
* overall output slot; see SharpnessUnorderedAccessIndices::destination_index_
* for the pass that actually owns it now.
*/
struct PostProcessUnorderedAccessIndices
{
	uint output_index_;
	float3 post_process_unordered_access_padding_0_;

	ExposureUnorderedAccessIndices exposure_;
	LensFlareUnorderedAccessIndices lens_flare_;
	LensFlareStreakUnorderedAccessIndices lens_flare_streak_;
	BloomUnorderedAccessIndices bloom_;
	AnamorphicFlareUnorderedAccessIndices anamorphic_flare_;
	ColorGradingUnorderedAccessIndices color_grading_;
	LensDistortionUnorderedAccessIndices lens_distortion_;
	ChromaticAberrationUnorderedAccessIndices chromatic_aberration_;
	VignetteUnorderedAccessIndices vignette_;
	DepthOfFieldUnorderedAccessIndices depth_of_field_;
	SharpnessUnorderedAccessIndices sharpness_;
	FilmGrainUnorderedAccessIndices film_grain_;
};

/**
* [EN]
* SRV indices for every post-process effect. source_color_index_ is the
* pre-post-process HDR scene color (ToneMappingCS.hlsl's default source
* when no lens-stage/depth-of-field override applies); lens_stage_index_ is
* set when the lens stage (chromatic aberration and/or vignette) ran, in
* which case it is the buffer it left the scene in and ToneMappingCS.hlsl
* must read that instead of source_color_index_ or the depth-of-field
* output. Resolved on the CPU in PostProcessRenderer::PrepareView so the
* shader needs one branch rather than a chain of them.
*/
struct PostProcessShaderResourceIndices
{
	uint source_color_index_;
	uint lens_stage_index_;
	float2 post_process_shader_resource_padding_0_;

	LensFlareShaderResourceIndices lens_flare_;
	LensFlareStreakShaderResourceIndices lens_flare_streak_;
	BloomShaderResourceIndices bloom_;
	AnamorphicFlareShaderResourceIndices anamorphic_flare_;
	ColorGradingShaderResourceIndices color_grading_;
	LensDistortionShaderResourceIndices lens_distortion_;
	ChromaticAberrationShaderResourceIndices chromatic_aberration_;
	VignetteShaderResourceIndices vignette_;
	DepthOfFieldShaderResourceIndices depth_of_field_;
	SharpnessShaderResourceIndices sharpness_;
};

#endif // __POST_PROCESS_HLSL__
