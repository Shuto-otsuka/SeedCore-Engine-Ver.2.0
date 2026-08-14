#ifndef __CONSTANTS_HLSL__
#define __CONSTANTS_HLSL__

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

// Per-view auto-exposure indices/tuning (3 rows / 48 bytes). Same
// one-effect-one-group shape as ShadowAccumulationIndices etc. below.
struct ExposureIndices
{
	uint histogram_uav_index_;
	uint exposure_uav_index_;
	uint auto_exposure_enabled_;
	float exposure_compensation_;

	float min_log_luminance_;
	float max_log_luminance_;
	float key_value_;
	float adapt_speed_to_bright_;

	float adapt_speed_to_dark_;
	uint exposure_padding_0_;
	uint exposure_padding_1_;
	uint exposure_padding_2_;
};

// Per-view tone-mapping indices/tuning (1 row / 16 bytes).
struct ToneMappingIndices
{
	uint tone_mapping_enabled_;
	uint tone_mapping_mode_;
	uint tone_mapping_padding_0_;
	uint tone_mapping_padding_1_;
};

// Per-view lens-flare indices/tuning (4 rows / 64 bytes). unordered_access_
// view_index_/shader_resource_view_index_ are LensFlareCS.hlsl's quarter-res
// write target and the bindless SRV ToneMappingCS.hlsl samples to add the
// flare into the HDR color before exposure. enabled_ gates both: when off,
// PostProcessRenderer's Dispatch skips LensFlareCS entirely, so
// ToneMappingCS must also skip the read to avoid sampling stale leftover
// data from when it was last enabled.
struct LensFlareIndices
{
	uint enabled_;
	uint unordered_access_view_index_;
	uint shader_resource_view_index_;
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
	uint lens_flare_padding_0_;
};

// Per-view lens-flare working buffers (8 rows / 128 bytes): one ping/pong
// pair per spike axis (LensFlareCS.hlsl walks LensFlareIndices::axis_count_
// axes, each a line of two opposing arms, each independently multi-pass
// blurred), plus the shared bright_ buffer every other lens-flare pass
// reads. bright_ is what
// LensFlareCS.hlsl's Downsample entry point writes: a quarter-res,
// bright-passed copy of the scene produced with a 4-tap bilinear filter
// covering the full 4x4 source footprint. Both BlurPass1 (streaks) and
// Ghost (ghost chain + halo) sample it instead of the full-res HDR source,
// because point-sampling mip 0 at quarter density skips 15 of every 16
// source pixels and so drops small bright sources entirely - the sun disc
// (a few pixels wide, see VolumetricCloudScapes.hlsli::ProceduralSkyColor)
// would otherwise never produce a flare. Set once per frame alongside
// LensFlareIndices above (PostProcessRenderer::CreateView allocates them,
// PrepareView registers the bindless indices) - not re-registered
// mid-frame, since LensFlareCS.hlsl's BlurPass1..4 entry points read/write
// them by a compile-time-fixed ping/pong parity per pass, not a
// per-dispatch index.
struct LensFlareStreakIndices
{
	// One row per axis: .x = ping UAV, .y = ping SRV, .z = pong UAV,
	// .w = pong SRV. A uint4 array is used rather than flat uint fields
	// because a uint4 array element is exactly one 16-byte cbuffer row, so
	// this is directly indexable by a runtime axis number - flat fields
	// would need a 7-way if-chain per lookup. LENS_FLARE_MAX_AXIS_COUNT
	// rows are always allocated; only the first axis_count_ of them are
	// read (see LensFlareIndices::axis_count_).
	uint4 axis_indices_[LENS_FLARE_MAX_AXIS_COUNT];

	uint bright_uav_index_;
	uint bright_srv_index_;
	uint lens_flare_streak_padding_0_;
	uint lens_flare_streak_padding_1_;
};

// Per-view bloom indices/tuning (5 rows / 80 bytes). level0..5 are the 6
// levels of KawaseBloomCS.hlsl's downsample/upsample chain, level0 being
// half the native resolution and each subsequent level half of the one
// before. The chain writes DOWN through the levels (DownsamplePrefilter then
// Downsample1..5) then accumulates back UP additively (Upsample4..0), so
// level0 ends up holding the final bloom that ToneMappingCS.hlsl samples and
// adds into the HDR color before exposure. enabled_ gates both the dispatch
// and that read, so a stale buffer from when bloom was last on is never
// sampled. filter_radius_ is the 3x3 tent radius in UV used by the upsample
// passes; soft_knee_ widens the threshold_ transition so pixels sitting at
// the cutoff fade in instead of popping.
struct BloomIndices
{
	uint level0_uav_index_;
	uint level1_uav_index_;
	uint level2_uav_index_;
	uint level3_uav_index_;

	uint level4_uav_index_;
	uint level5_uav_index_;
	uint level0_srv_index_;
	uint level1_srv_index_;

	uint level2_srv_index_;
	uint level3_srv_index_;
	uint level4_srv_index_;
	uint level5_srv_index_;

	uint enabled_;
	float threshold_;
	float soft_knee_;
	float intensity_;

	float filter_radius_;
	uint bloom_padding_0_;
	uint bloom_padding_1_;
	uint bloom_padding_2_;
};

// Per-view anamorphic-flare indices/tuning (4 rows / 64 bytes). ping_/pong_
// are AnamorphicFlareCS.hlsl's HORIZONTALLY SQUEEZED working buffers - half
// the width of the other quarter-res post-process buffers, so they carry a
// baked 2:1 anamorphic squeeze. That squeeze is why the streak comes out
// horizontal at all: the flare is blurred as an ordinary round shape inside
// the squeezed space and Compose samples it back with normal UVs, which
// stretches it 2x horizontally for free. output_ is Compose's own target,
// which ToneMappingCS.hlsl samples and adds into the HDR color before
// exposure. enabled_ gates both the dispatch and that read, so a stale
// buffer from when the effect was last on is never sampled.
struct AnamorphicFlareIndices
{
	uint enabled_;
	uint output_uav_index_;
	uint output_srv_index_;
	float threshold_;

	uint ping_uav_index_;
	uint ping_srv_index_;
	uint pong_uav_index_;
	uint pong_srv_index_;

	float intensity_;
	float streak_length_;
	float attenuation_;
	uint anamorphic_flare_padding_0_;

	float4 tint_;
};

// Per-view chromatic-aberration indices/tuning (2 rows / 32 bytes) and
// vignette indices/tuning (3 rows / 48 bytes). Both are LENS stage effects:
// they run before auto-exposure and tone mapping, because both describe
// what reaches the sensor rather than how the sensor is developed. They
// chain through one shared buffer - source_srv_index_ is resolved on the
// CPU in PostProcessRenderer::PrepareView, so if chromatic aberration is on
// the vignette reads its output, otherwise it reads the depth-of-field
// output or the raw scene color. Vignette is a pure per-pixel multiply and
// so may read and write the same texture in place; chromatic aberration
// reads neighbours and may not, which is why it always writes the shared
// lens-stage buffer.
// One tonal range's colour grading controls (2 rows / 32 bytes), in the
// order they are applied. All scalars rather than per-channel: the colour
// axis is handled by temperature_ as a chromatic adaptation instead (see
// ColorGradingRangeSettings in PostProcess.h for why). Neutral is 1 for
// saturation/contrast/gamma/gain and 0 for offset and temperature.
struct ColorGradingRangeIndices
{
	float temperature_;
	float saturation_;
	float contrast_;
	float gamma_;

	float gain_;
	float offset_;
	uint color_grading_range_padding_0_;
	uint color_grading_range_padding_1_;
};

// Unreal-style colour grading (10 rows / 160 bytes): four tonal ranges each
// with their own wheels, blended by luminance with smooth crossovers at
// shadows_max_ and highlights_min_. Runs in scene-referred linear space
// AFTER exposure and BEFORE the tone curve, which is forced by the 0.18
// contrast pivot - 0.18 only means middle grey once exposure has placed the
// scene there, and means nothing after the curve has compressed the range.
// Because of that position this pass also owns the additive contributions
// (bloom, lens flare, anamorphic) and the exposure multiply, which
// ToneMappingCS.hlsl skips whenever enabled_ is set.
struct ColorGradingIndices
{
	uint enabled_;
	uint source_srv_index_;
	uint destination_uav_index_;
	float shadows_max_;

	float highlights_min_;
	uint output_srv_index_;
	uint color_grading_padding_0_;
	uint color_grading_padding_1_;

	ColorGradingRangeIndices global_;
	ColorGradingRangeIndices shadows_;
	ColorGradingRangeIndices midtones_;
	ColorGradingRangeIndices highlights_;
};

// Radial half of the Brown-Conrady distortion model (2 rows / 32 bytes),
// the first stage of the lens chain since it displaces geometry. k1
// dominates, k2 refines the corners, k3 barely moves anything. scale_ zooms
// in before distorting so barrel distortion does not leave empty corners.
struct LensDistortionIndices
{
	uint enabled_;
	uint source_srv_index_;
	uint destination_uav_index_;
	float k1_;

	float k2_;
	float k3_;
	float scale_;
	uint lens_distortion_padding_0_;
};

struct ChromaticAberrationIndices
{
	uint enabled_;
	uint source_srv_index_;
	uint destination_uav_index_;
	float intensity_;

	uint sample_count_;
	uint chromatic_aberration_padding_0_;
	uint chromatic_aberration_padding_1_;
	uint chromatic_aberration_padding_2_;
};

struct VignetteIndices
{
	uint enabled_;
	uint source_srv_index_;
	uint destination_uav_index_;
	float intensity_;

	float exponent_;
	uint vignette_padding_0_;
	uint vignette_padding_1_;
	uint vignette_padding_2_;

	float4 color_;
};

// Per-view depth-of-field indices/tuning (2 rows / 32 bytes).
// unordered_access_view_index_/shader_resource_view_index_ are
// DepthOfFieldCS.hlsl's native-res write target (BokehCS.hlsl
// read-modify-writes the same UAV, it has no resources of its own). Unlike
// LensFlareIndices this is not an additive contribution - it is a whole
// replacement HDR buffer. When enabled_ is set, LensFlareCS.hlsl and
// ToneMappingCS.hlsl read shader_resource_view_index_ instead of
// PostProcessIndices::source_color_index_ (see those files' source-select).
struct DepthOfFieldIndices
{
	uint enabled_;
	uint unordered_access_view_index_;
	uint shader_resource_view_index_;
	float focus_distance_;

	float focus_range_;
	float max_blur_radius_;
	uint depth_of_field_padding_0_;
	uint depth_of_field_padding_1_;
};

// Per-view bokeh-highlight indices/tuning (1 row / 16 bytes). BokehCS.hlsl
// only runs when this AND DepthOfFieldIndices.enabled_ are both set - it has
// no resources of its own, it scatters shaped highlights into
// DepthOfFieldIndices' output buffer.
struct BokehIndices
{
	uint enabled_;
	float highlight_threshold_;
	float highlight_intensity_;
	uint blade_count_;
};

// Per-view film-grain indices/tuning (2 rows / 32 bytes). Runs LAST, after
// SharpnessCS.hlsl, and read-modify-writes that pass's output in place -
// safe because grain is a per-pixel operation with no neighbour taps, and
// deliberate so the sharpen pass does not amplify the grain it was given.
// Unlike the lens-stage effects this is applied after tone mapping: grain is
// the developed emulsion's density variation, so the tonal position driving
// luminance_response_ only means anything post-curve.
struct FilmGrainIndices
{
	uint enabled_;
	uint destination_uav_index_;
	uint colored_;
	float intensity_;

	float size_;
	float luminance_response_;
	uint film_grain_padding_0_;
	uint film_grain_padding_1_;
};

// Per-view sharpness indices/tuning (1 row / 16 bytes). SharpnessCS.hlsl runs
// last, after ToneMappingCS.hlsl - source_srv_index_ is the bindless SRV of
// ToneMappingCS.hlsl's tone-mapped/sRGB-encoded output (now an intermediate
// buffer rather than the final display texture), destination_uav_index_ is
// this pass's own output, which becomes the new final display texture
// (PostProcessRenderer::OutputResource et al. now point at it). Runs
// unconditionally every frame like ToneMappingCS.hlsl; enabled_ just gates
// whether the shader applies the sharpen offset or passes the source through
// unchanged.
struct SharpnessIndices
{
	uint source_srv_index_;
	uint destination_uav_index_;
	uint enabled_;
	float amount_;
};

// Per-view post-process indices (49 rows / 784 bytes). Lives inside
// ConstantIndices, not StructuredIndices, for the same reason the shadow/AO
// accumulation chains do: the histogram/persistent-exposure buffers and the
// display output texture are genuinely per-camera state (editor and game can
// be looking at wildly different scenes with independently-adapting
// exposure), and StructuredIndices is one buffer shared by every view. Each
// effect gets its own group struct (ExposureIndices/ToneMappingIndices/
// LensFlareIndices/LensFlareStreakIndices/BloomIndices/AnamorphicFlareIndices/
// DepthOfFieldIndices/BokehIndices/SharpnessIndices above) instead of flat
// fields here, matching the
// Shadow/AmbientOcclusion/GlobalIllumination/Dlss grouping convention below.
struct PostProcessIndices
{
	uint output_uav_index_;
	uint source_color_index_;

	// Set when the lens stage (chromatic aberration and/or vignette) ran, in
	// which case lens_stage_srv_index_ is the buffer it left the scene in and
	// ToneMappingCS.hlsl must read that instead of source_color_index_ or the
	// depth-of-field output. Resolved on the CPU in
	// PostProcessRenderer::PrepareView so the shader needs one branch rather
	// than a chain of them.
	uint lens_stage_enabled_;
	uint lens_stage_srv_index_;

	ExposureIndices exposure_;
	ToneMappingIndices tone_mapping_;
	LensFlareIndices lens_flare_;
	LensFlareStreakIndices lens_flare_streak_;
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

// Ray-traced shadow SVGF chain (ShadowDenoiseCS.hlsl). These live inside
// ConstantIndices (per-view constant buffer) instead of StructuredIndices
// because the editor and game views each need their own temporal-accumulation
// chain: the shadow signal is screen-space and per-camera, and StructuredIndices
// is a single buffer shared by every view, so per-view values placed there
// would clobber each other.
//
// history_/accumulated_ are the SVGF feedback tap (ATrousPass2's output), i.e.
// what next frame reprojects, NOT the final image - denoised_/visibility_ is
// the fully filtered result ATrousPass3 writes and DeferredLightingPS.hlsl
// samples. moments_ carries (1st.x, 2nd.x, 1st.y, 2nd.y) of the two visibility
// channels, history_length_ the accumulated frame count, and depth_normal_ a
// packed copy of this frame's view depth / depth derivative / normal, which the
// temporal consistency test needs from the PREVIOUS frame (the engine's
// G-Buffer is single-buffered, so it cannot be read back).
//
// All of moments_/history_length_/depth_normal_ ping-pong exactly like
// history_/accumulated_ do; atrous_scratch0_/atrous_scratch1_ are pure scratch
// registered once in Create()/Resize().
struct ShadowAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint accumulated_srv_index_;
	uint visibility_srv_index_;

	uint atrous_scratch0_srv_index_;
	uint atrous_scratch0_uav_index_;
	uint atrous_scratch1_srv_index_;
	uint atrous_scratch1_uav_index_;

	uint moments_history_srv_index_;
	uint moments_srv_index_;
	uint moments_uav_index_;
	uint history_length_history_srv_index_;

	uint history_length_srv_index_;
	uint history_length_uav_index_;
	uint depth_normal_history_srv_index_;
	uint depth_normal_srv_index_;

	uint depth_normal_uav_index_;
	uint denoised_uav_index_;
	uint shadow_accumulation_padding_0_;
	uint shadow_accumulation_padding_1_;
};

// Per-view ray-traced AO accumulation chain - same scheme as
// ShadowAccumulationIndices above.
struct AmbientOcclusionAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint openness_srv_index_;
	uint ambient_occlusion_accumulation_padding_;
};

// Per-view ray-traced GI accumulation chain - same scheme as shadow/AO
// above. The raw 1spp radiance (structured_indices.global_illumination_) is
// shared/single-buffered across views; this chain is the per-view denoised
// (spatio-temporal) result DeferredLightingPS.hlsl samples.
//
// atrous_scratch0_/atrous_scratch1_ are the two per-view ping-pong textures
// GlobalIlluminationDenoiseCS.hlsl's ATrousPass1/2/3 entry points read/write
// between (see that file). Unlike history_/accumulated_/radiance_ above,
// these are set ONCE by GlobalIlluminationRenderer::Create/Resize rather
// than every frame in PrepareFrame - they are pure scratch, always fully
// overwritten by the A-Trous passes themselves, so nothing needs to update
// their bindless index frame to frame.
struct GlobalIlluminationAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint radiance_srv_index_;
	uint global_illumination_accumulation_padding_;

	uint atrous_scratch0_srv_index_;
	uint atrous_scratch0_uav_index_;
	uint atrous_scratch1_srv_index_;
	uint atrous_scratch1_uav_index_;
};

// Per-view ray-traced reflection ReBLUR chain (ReflectionDenoiseCS.hlsl) -
// same per-view reasoning as shadow/AO/GI above. The raw 1spp GGX-sampled
// radiance (structured_indices.reflection_) is shared/single-buffered across
// views; this chain is the per-view denoised result DeferredLightingPS.hlsl
// samples.
//
// history_/accumulated_/radiance_ all refer to the ReBLUR output: rgb =
// radiance, a = the accumulated NORMALIZED HIT DISTANCE (not a validity flag -
// ReBLUR derives its whole blur radius from that distance, so it has to survive
// into the history). a == 0 still means "nothing traced here", because both the
// background branch and the renderer's disabled-path clear write 0.
//
// accum_speed_ is the per-pixel accumulated frame count, the state that drives
// both the temporal blend factor and every spatial radius; depth_normal_ is a
// packed copy of this frame's view depth / normal / roughness, needed next
// frame by the disocclusion and virtual-motion tests. Both ping-pong like
// history_/accumulated_ do; scratch0_/scratch1_ are pure scratch registered
// once in Create()/Resize().
struct ReflectionAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint radiance_srv_index_;
	uint reflection_accumulation_padding_0_;

	uint scratch0_srv_index_;
	uint scratch0_uav_index_;
	uint scratch1_srv_index_;
	uint scratch1_uav_index_;

	uint accum_speed_history_srv_index_;
	uint accum_speed_srv_index_;
	uint accum_speed_uav_index_;
	uint depth_normal_history_srv_index_;

	uint depth_normal_uav_index_;

	// Short-history luma (ReBLUR's "fast history"), ping-ponged like the rest.
	// Only the luminance is kept - that is all the clamp below needs, and it is
	// what NRD stores too. The slow 30-frame history is clamped into this
	// 6-frame history's local luma box every frame; without that clamp a long
	// exponential history visibly trails the camera even when the reprojection
	// itself is correct, because nothing ever forces it to catch up.
	uint fast_history_history_srv_index_;
	uint fast_history_srv_index_;
	uint fast_history_uav_index_;
};

// DLSS Ray Reconstruction's synthesized RGB=normal/A=roughness buffer for
// this view. Genuinely per-view (Editor's and Game's G-Buffer content
// differ) - set once at DlssRayReconstructionRenderer::Create() time, not
// per-frame (single-buffered per view, never ping-ponged).
struct DlssIndices
{
	uint normal_roughness_uav_index_;
	uint specular_albedo_uav_index_;
	uint diffuse_albedo_uav_index_;
	uint dlss_padding_;
};

struct ConstantIndices
{
	uint scene_index_;
	uint light_index_;
	uint cluster_constant_index_;
	uint view_mode_;

	ShadowAccumulationIndices shadow_;
	AmbientOcclusionAccumulationIndices ambient_occlusion_;
	GlobalIlluminationAccumulationIndices global_illumination_;
	ReflectionAccumulationIndices reflection_;
	DlssIndices dlss_;

	PostProcessIndices post_process_;
};
ConstantBuffer<ConstantIndices> constant_indices : register(b0, space1);

struct SceneConstantBuffer
{
	row_major float4x4 view_;
	row_major float4x4 inverse_view_;
	
	row_major float4x4 projection_;
	row_major float4x4 inverse_projection_;
	row_major float4x4 non_jitter_projection_;
	
	row_major float4x4 current_view_projection_;
	row_major float4x4 previous_view_projection_;
	row_major float4x4 inverse_view_projection_;
	row_major float4x4 non_jitter_view_projection_;
	row_major float4x4 previous_non_jitter_view_projection_;

	float4 camera_position_;
	float4 camera_focus_;
	
	float field_of_view_;
	float near_plane_;
	float far_plane_;
	float scene_constant_0_padding_;
	
	float total_time_;
	float delta_time_;

	// Resolution of whatever the post-tonemap debug overlay (collider wireframes, selection outline) actually draws onto - screen_size_ normally, or PostProcessRenderer's DLSS-RR-upscaled output resolution while DLSS-RR is active. SelectionOutlinePS.hlsl scales SV_Position by screen_size_/display_size_ before indexing the (native-resolution) selection mask.
	float2 display_size_;

	float2 screen_size_;
	float2 inverse_screen_size_;
};
ConstantBuffer<SceneConstantBuffer> GetSceneConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.scene_index_];
}

struct ClusterAssignConstantBuffer
{
	uint cluster_data_unordered_access_view_index_;
	uint cluster_light_list_unordered_access_view_index_;
	uint point_light_shader_resource_view_index_;
	uint spot_light_shader_resource_view_index_;
	uint rect_light_shader_resource_view_index_;
	uint point_light_count_;
	uint spot_light_count_;
	uint rect_light_count_;
	uint total_clusters_;
	uint cluster_count_x_;
	uint cluster_count_y_;
	float cluster_assign_0_padding_;

	float near_plane_;
	float far_plane_;
	float2 cluster_assign_1_padding_;
};
ConstantBuffer<ClusterAssignConstantBuffer> GetClusterConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.cluster_constant_index_];
}

#endif // __CONSTANTS_HLSL__