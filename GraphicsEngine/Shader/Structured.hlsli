#ifndef __STRUCTURED_HLSL__
#define __STRUCTURED_HLSL__

// Bindless index table shared by every view (b1 space1).
//
// Split into one sub-struct per feature. Each sub-struct is deliberately a
// whole number of 16-byte cbuffer rows, with its padding written out
// explicitly - HLSL aligns a nested struct member inside a cbuffer to 16
// bytes, so keeping every group row-sized makes the C++ mirror in
// System/IndicesSystem.h match member-for-member with no hidden gaps.
// Add fields INSIDE a group and adjust that group's padding; never let a
// group stop being a multiple of 16 bytes.

struct SpriteIndices                      // 1 row
{
	uint image_index_;
	uint image_billboard_index_;
	uint font_index_;
	uint font_billboard_index_;
};

struct ModelIndices                       // 2 rows
{
	uint instance_index_;
	uint bone_matrix_index_;
	uint hi_z_index_;
	uint selection_mask_index_;

	// Shared per-frame morph target weight buffer (see Model.hlsli's
	// ApplyMorphBlend) - one flat StructuredBuffer<float>, every morphed
	// instance's weights appended back to back (ModelInstance::
	// morph_weight_offset_ is that instance's start).
	uint morph_weight_index_;
	uint model_padding_0_;
	uint model_padding_1_;
	uint model_padding_2_;
};

struct OitIndices                         // 1 row
{
	uint head_pointer_index_;
	uint fragment_buffer_index_;
	uint counter_index_;
	// OITFragment elements actually allocated. Supplied by OITBuffer, not
	// recomputed from screen size - the pool is clamped to a byte budget.
	uint fragment_capacity_;
};

struct GBufferIndices                     // 3 rows
{
	uint index_0_;                    // RT0: base_color.rgb + metallic
	uint index_1_;                    // RT1: octNormal.rg + roughness (.a unused)
	uint index_2_;                    // RT2: velocity
	uint index_3_;                    // RT3: emissive.rgb (raw, emissive_strength_ applied at lighting time)
	uint index_4_;                    // RT4: VisibilityBuffer id (instance/meshlet/triangle) + asuint(texcoord), R32G32B32A32_UINT
	uint depth_index_;
	uint velocity_uav_index_;         // RT2 UAV
	uint index_0_uav_;
	uint index_1_uav_;
	uint index_3_uav_;
	uint gbuffer_padding_0_;
	uint gbuffer_padding_1_;
};

struct MaterialSortIndices                // 1 row
{
	// RWStructuredBuffer<uint>[MATERIAL_SORT_BUCKET_COUNT] - per-bucket pixel
	// count, then in-place turned into exclusive-scan offsets, then further
	// turned into atomic write cursors by MaterialScatterCS.hlsl (see Model.hlsli).
	uint bucket_index_;
	// RWStructuredBuffer<uint>[screen_width * screen_height] - MaterialScatterCS.hlsl
	// writes each pixel's linear coordinate (y * width + x) into its bucket's
	// slot range; Model/Material/MaterialResolveCS.hlsl is dispatched 1D over this list
	// instead of raw screen order. Cleared to MATERIAL_SORT_INVALID_PIXEL each frame.
	uint sorted_pixel_list_index_;
	uint material_sort_padding_0_;
	uint material_sort_padding_1_;
};

struct SkyIndices                         // 2 rows
{
	uint environment_cube_index_;
	uint diffuse_irradiance_index_;
	uint specular_prefiltered_index_;
	uint brdf_lut_index_;
	float intensity_;
	uint3 sky_padding_;
};

// Shared by every ray-traced pass. instance_data_index_ is the per-TLAS-
// instance geometry/material table: reflection uploads it, GI reads the same
// one (same TLAS, same instance order).
struct RaytracingIndices                  // 1 row
{
	uint tlas_index_;
	uint instance_data_index_;
	uint2 raytracing_padding_;
};

struct ShadowIndices                      // 1 row
{
	uint raw_visibility_uav_index_;
	uint raw_visibility_srv_index_;
	uint ray_constant_index_;
	uint shadow_padding_;
};

struct AmbientOcclusionIndices            // 1 row
{
	uint raw_uav_index_;
	uint raw_srv_index_;
	uint ray_constant_index_;
	uint ambient_occlusion_padding_;
};

struct SubsurfaceScatteringIndices        // 1 row
{
	uint transmittance_uav_index_;
	uint transmittance_srv_index_;
	uint ray_constant_index_;
	uint subsurface_scattering_padding_;
};

struct ReflectionIndices                  // 1 row
{
	uint output_uav_index_;
	uint output_srv_index_;
	uint ray_constant_index_;
	uint reflection_padding_;
};

struct RefractionIndices                  // 1 row
{
	uint output_uav_index_;
	uint output_srv_index_;
	uint ray_constant_index_;
	uint refraction_padding_;
};

struct GlobalIlluminationIndices          // 1 row
{
	uint output_uav_index_;
	uint output_srv_index_;
	uint ray_constant_index_;
	uint global_illumination_padding_;
};

struct CloudIndices                       // 2 rows
{
	uint output_uav_index_;
	uint output_srv_index_;
	uint ray_constant_index_;
	uint shape_noise_uav_index_;
	uint shape_noise_srv_index_;
	uint detail_noise_uav_index_;
	uint detail_noise_srv_index_;
	uint cloud_padding_;
};

struct StarIndices                        // 1 row
{
	uint output_uav_index_;
	uint output_srv_index_;
	uint ray_constant_index_;
	uint star_padding_;
};

struct WeatherParticleIndices             // 2 rows
{
	uint rain_particle_uav_index_;
	uint rain_particle_srv_index_;
	uint snow_particle_uav_index_;
	uint snow_particle_srv_index_;
	uint ray_constant_index_;         // single combined WeatherParticleConstantBuffer (rain+snow tuning).
	uint3 weather_particle_padding_;
};

struct VolumetricLightIndices             // 2 rows
{
	uint density_uav_index_;
	uint scattering_uav_index_;
	uint integration_uav_index_;
	uint integration_srv_index_;
	uint ray_constant_index_;
	uint3 volumetric_light_padding_;
};

struct MovieIndices                       // 1 row
{
	uint sprite_index_;
	uint billboard_index_;
	uint fullscreen_index_;
	uint movie_padding_;
};

// 24 rows total (384 bytes).
struct StructuredIndices
{
	SpriteIndices sprite_;
	ModelIndices model_;
	OitIndices oit_;
	GBufferIndices gbuffer_;
	MaterialSortIndices material_sort_;
	SkyIndices sky_;
	RaytracingIndices raytracing_;
	ShadowIndices shadow_;
	AmbientOcclusionIndices ambient_occlusion_;
	SubsurfaceScatteringIndices subsurface_scattering_;
	ReflectionIndices reflection_;
	RefractionIndices refraction_;
	GlobalIlluminationIndices global_illumination_;
	CloudIndices cloud_;
	StarIndices star_;
	WeatherParticleIndices weather_particle_;
	VolumetricLightIndices volumetric_light_;
	MovieIndices movie_;
};
ConstantBuffer<StructuredIndices> structured_indices : register(b1, space1);

struct ImageSpriteInstance
{
	float2 position;
	float rotation;
	float2 scale;
	float image_sprite_instance_padding_0;
	float2 texture_size;
	float2 texture_position;
	float2 pivot;
	float2 image_sprite_instance_padding_1;
	float4 color;
	uint texture_index;
	float scroll_speed;
	float2 scroll_direction;
	uint motion_type;
	uint selected;
	float3 image_sprite_instance_padding_2;
};

struct ImageBillboardInstance
{
	float3 position;
	float3 rotation;
	float2 scale;
	float2 texture_size;
	float2 texture_position;
	float2 pivot;
	float2 image_billboard_instance_padding_0;
	float4 color;
	uint texture_index;
	float scroll_speed;
	float2 scroll_direction;
	uint motion_type;
	uint face_camera;
	uint selected;
	float2 image_billboard_instance_padding_1;
};

struct FontSpriteInstance
{
	float2 position;
	float2 size;
	float2 uv_min;
	float2 uv_max;
	float4 color;
	float4 outline_color;
	float4 glow_color;
	uint texture_index;
	float outline_width;
	float glow_power;
	float font_sprite_instance_padding_1;
	float2 unit_range;
	uint selected;
	float font_sprite_instance_padding_2;
};

struct FontBillboardInstance
{
	float3 position;
	float font_billboard_instance_padding_0;
	float3 rotation;
	float font_billboard_instance_padding_1;
	float2 local_position;
	float2 local_size;
	float2 uv_min;
	float2 uv_max;
	float4 color;
	float4 outline_color;
	float4 glow_color;
	uint texture_index;
	float outline_width;
	float glow_power;
	float font_billboard_instance_padding_2;
	float2 unit_range;
	uint face_camera;
	uint selected;
};

struct MovieSpriteInstance
{
	float2 position;
	float rotation;
	float2 scale;
	float movie_sprite_instance_padding_0;
	float2 size;
	float2 movie_sprite_instance_padding_1;
	float2 pivot;
	float2 movie_sprite_instance_padding_2;
	float4 color;
	uint texture_index;
	uint selected;
	uint2 movie_sprite_instance_padding_3;
};

struct MovieBillboardInstance
{
	float3 position;
	float3 rotation;
	float2 scale;
	float2 size;
	float2 movie_billboard_instance_padding_0;
	float2 pivot;
	float2 movie_billboard_instance_padding_1;
	float4 color;
	uint texture_index;
	uint face_camera;
	uint selected;
	float movie_billboard_instance_padding_2;
};

struct MovieFullscreenInstance
{
	float4 color;
	uint texture_index;
	float texture_aspect;
	uint2 movie_fullscreen_instance_padding_0;
};

#endif // __STRUCTURED_HLSL__