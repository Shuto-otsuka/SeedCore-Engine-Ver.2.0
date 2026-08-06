#ifndef __CONSTANTS_HLSL__
#define __CONSTANTS_HLSL__

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

// Per-view lens-flare indices/tuning (3 rows / 48 bytes). unordered_access_
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
	uint lens_flare_padding_0_;
	uint lens_flare_padding_1_;
	uint lens_flare_padding_2_;
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

// Per-view post-process indices (11 rows / 176 bytes). Lives inside
// ConstantIndices, not StructuredIndices, for the same reason the shadow/AO
// accumulation chains do: the histogram/persistent-exposure buffers and the
// display output texture are genuinely per-camera state (editor and game can
// be looking at wildly different scenes with independently-adapting
// exposure), and StructuredIndices is one buffer shared by every view. Each
// effect gets its own group struct (ExposureIndices/ToneMappingIndices/
// LensFlareIndices/DepthOfFieldIndices/BokehIndices above) instead of flat
// fields here, matching the Shadow/AmbientOcclusion/GlobalIllumination/Dlss
// grouping convention below.
struct PostProcessIndices
{
	uint output_uav_index_;
	uint source_color_index_;
	uint post_process_padding_0_;
	uint post_process_padding_1_;

	ExposureIndices exposure_;
	ToneMappingIndices tone_mapping_;
	LensFlareIndices lens_flare_;
	DepthOfFieldIndices depth_of_field_;
	BokehIndices bokeh_;
};

// Ray-traced shadow accumulation buffers. These live inside ConstantIndices
// (per-view constant buffer) instead of StructuredIndices because the editor
// and game views each need their own temporal-accumulation chain: the shadow
// signal is screen-space and per-camera, and StructuredIndices is a single
// buffer shared by every view, so per-view values placed there would clobber
// each other. history = previous frame's accumulated result (SRV),
// accumulated = this frame's write target (UAV), visibility = the same write
// target's SRV, sampled by DeferredLightingPS.hlsl after the denoise pass.
struct ShadowAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint visibility_srv_index_;
	uint shadow_accumulation_padding_;
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

// Per-view ray-traced reflection accumulation chain - same scheme as
// shadow/AO/GI above. The raw 1spp GGX-sampled radiance
// (structured_indices.reflection_) is shared/single-buffered across views;
// this chain is the per-view denoised (spatio-temporal) result
// DeferredLightingPS.hlsl samples.
struct ReflectionAccumulationIndices
{
	uint history_srv_index_;
	uint accumulated_uav_index_;
	uint radiance_srv_index_;
	uint reflection_accumulation_padding_;
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