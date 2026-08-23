#ifndef __MATERIAL_HLSL__
#define __MATERIAL_HLSL__

#include "../Model/Model.hlsli"
#include "Constants.hlsli"
#include "Normal.hlsli"
#include "Light.hlsli"
#include "Sampler.hlsli"

// One entry per material slot in the mesh's Crister::Materials() list.
// Uploaded once per unique Crister (RaytracingRenderer::BuildReflectionMaterialTable,
// cached like the BLAS - not rebuilt every frame). Must match the C++ mirror in
// Renderer/RaytracingRenderer.h byte-for-byte.
struct ReflectionMaterial
{
	float3 base_color_;

	// Bindless SRV of that material's base-color texture, or 0xFFFFFFFF when it
	// has none / is not resident yet (Crister::TextureBindlessIndex returns the
	// sentinel while a streaming mip is still loading).
	uint base_color_texture_index_;

	// KHR_materials_ior/transmission/volume - read by Refraction/RefractionRT.hlsl
	// (Snell refraction needs ior_; the Fresnel/transmit-vs-absorb decision needs
	// transmission_factor_; Beer-Lambert absorption over the traveled distance
	// inside the medium needs volume_attenuation_color_/distance_). Reflection
	// itself does not use these - they live here rather than in a separate
	// table so ResolveReflectionMaterial's per-triangle lookup can be shared.
	float ior_;
	float transmission_factor_;
	float3 volume_attenuation_color_;
	float volume_attenuation_distance_;

	// glTF alphaMode in the loader's encoding: 0 OPAQUE, 1 MASK, 2 BLEND.
	uint alpha_mode_;
	float alpha_cutoff_;
	float base_color_alpha_;

	// KHR_materials_volume thickness. Zero means THIN-WALLED: the surface
	// encloses no volume, so Refraction/RefractionRT.hlsl must skip Beer-Lambert
	// absorption entirely for it - otherwise a pane of window glass gets tinted
	// like a solid block of the same material.
	float thickness_factor_;
	uint thickness_texture_index_;   // .g scales thickness_factor_ per pixel
	float material_padding_;
};

// One entry per TLAS instance, indexed by InstanceID() in the closesthit
// shader. Uploaded per frame by RaytracingRenderer alongside the TLAS (same
// order as the instance descs). Must match the C++ mirror in
// Renderer/ReflectionRenderer.h byte-for-byte (structured buffer - tight
// packing, no cbuffer 16-byte rules).
struct ReflectionInstanceData
{
	// Bindless SRV of the mesh's StructuredBuffer<ReflectionVertex> (Crister
	// compressed vertex buffer - 16 bytes, see Model/Crister.h CompressedVertex).
	uint vertex_buffer_index_;

	// Bindless SRV of the mesh's flat 32-bit triangle index buffer
	// (PrimitiveIndex() * 3 + n -> vertex index).
	uint index_buffer_index_;

	// Bindless SRV of StructuredBuffer<ReflectionMaterial> - the mesh's full
	// material list (Crister::Materials(), or a single white fallback entry if
	// the mesh has none), indexed via triangle_material_index_buffer_index_
	// below rather than a flat per-instance color. A multi-material mesh (e.g.
	// a Cornell box modeled as one Crister with a red/green/white wall per
	// submesh) needs this - using only materials()[0] for the whole instance
	// was the bug that made every ray-traced bounce off such a mesh come back
	// the same single colour regardless of which wall it actually hit,
	// silently killing color bleeding.
	uint material_data_index_;

	// Bindless SRV of StructuredBuffer<uint>, one entry per triangle in the
	// flat index buffer above, giving that triangle's index into
	// material_data_index_'s array (built from each SubMesh's materialIndex_
	// over its own [indexOffset_/3, (indexOffset_+indexCount_)/3) triangle
	// range - see BuildReflectionMaterialTable).
	uint triangle_material_index_buffer_index_;

	// The compressed vertex UVs are UNORM within this AABB, so decoding them
	// needs the per-mesh bounds (Crister::TexcoordMin / TexcoordExtent) - the
	// same values Model.hlsli gets through its instance data.
	float2 texcoord_min_;
	float2 texcoord_extent_;
};

// Resolves the actual hit triangle's material via the two-step per-triangle
// lookup above. primitive_index is PrimitiveIndex() from the closesthit shader.
ReflectionMaterial ResolveReflectionMaterial(ReflectionInstanceData instance, uint primitive_index)
{
	StructuredBuffer<uint> triangle_material_index = ResourceDescriptorHeap[instance.triangle_material_index_buffer_index_];
	uint material_index = triangle_material_index[primitive_index];

	StructuredBuffer<ReflectionMaterial> materials = ResourceDescriptorHeap[instance.material_data_index_];
	return materials[material_index];
}

// Weather's reshaping of the base surface (WeatherSystem::ReadGpuState),
// applied before any KHR material texture is sampled so wetness/puddle/snow
// affect the analytic terms and every extension consistently. wetness_/
// puddle_ are also carried out for the grazing-angle F0 boost in
// ResolveGBufferMaterial below - a wet/puddled surface reflects more at
// grazing angles than its dry F0 alone would give it.
struct WeatherMaterial
{
	float3 base_color_;
	float roughness_;
	float wetness_;
	float puddle_;
};

// wetness only lifts a surface toward wet/glossy on faces that actually
// catch rain (upward-facing, weighted by normal.y); puddles further need
// near-flat ground (a wall only gets the thin wet film, never standing
// water). roughness is floored to MIN_PERCEPTUAL_ROUGHNESS afterward so the
// puddle floor (0.03) can't slip under it - see that constant's comment for
// why a roughness near 0 is a breakdown, not a sharp highlight.
WeatherMaterial ResolveWeatherMaterial(float3 base_color, float roughness, float3 normal)
{
	ConstantBuffer<LightConstantData> weather_light = ResourceDescriptorHeap[constant_indices.light_index_];
	float weather_up_facing = saturate(normal.y);

	float wetness = weather_light.wetness_ * weather_up_facing;
	roughness = lerp(roughness, roughness * 0.25, wetness);
	base_color = lerp(base_color, base_color * 0.8, wetness * 0.6);

	float flatness = saturate((normal.y - 0.85) / 0.15);
	float puddle = wetness * flatness;
	roughness = lerp(roughness, 0.03, puddle);
	base_color = lerp(base_color, base_color * 0.5, puddle * 0.5);

	float snow = weather_light.snow_coverage_ * weather_up_facing;
	base_color = lerp(base_color, float3(0.95, 0.96, 1.0), snow);
	roughness = lerp(roughness, 0.9, snow);

	// Below this, roughness is a breakdown, not a sharp highlight: the GGX
	// normal distribution's peak (N.H = 1) is D = 1 / (PI * alpha^2) with
	// alpha = roughness^2, so it diverges as the 4th power as roughness ->
	// 0 (roughness 0.01 -> D ~= 3.2e7, roughness 0 -> D = Inf), which lands
	// in the RGBA16F HDR buffer (max 65504) as +Inf. Filament/Frostbite use
	// this same 0.045 floor for the same half-float-overflow reason. Applied
	// AFTER weather so the puddle floor above (0.03) can't slip under it.
	const float MIN_PERCEPTUAL_ROUGHNESS = 0.045;
	roughness = max(roughness, MIN_PERCEPTUAL_ROUGHNESS);

	WeatherMaterial result;
	result.base_color_ = base_color;
	result.roughness_ = roughness;
	result.wetness_ = wetness;
	result.puddle_ = puddle;
	return result;
}

// Fully-resolved PBR material at one surface point, ready for
// EvalDirectLightDispatch/EvalDirectLight: weather (wetness/puddle/snow) and
// every KHR material extension (specular, iridescence, transmission,
// clearcoat, anisotropy, sheen) already folded in. Produced by
// ResolveGBufferMaterial below - shared by Model/Opaque/DeferredLightingPS.hlsl
// (the primary G-Buffer surface) and any raytraced pass that needs the same
// surface's material (e.g. Raytracing/Shadow/ShadowRT.hlsl's ReSTIR DI), so
// a punctual light's specular response matches between the two.
struct GBufferMaterial
{
	float3 diffuse_color_;
	float3 f0_;
	float roughness_;

	float clearcoat_factor_;
	float clearcoat_roughness_;
	float3 clearcoat_normal_;

	float3 sheen_color_;
	float sheen_roughness_;

	float3 anisotropy_tangent_;
	float3 anisotropy_bitangent_;
	float anisotropy_strength_;
};

// Resolves everything EvalDirectLightDispatch needs from the raw G-Buffer
// values plus this surface's ModelInstance, applying (in order): weather
// (wetness/puddle/snow reshape base_color/roughness before any KHR texture is
// sampled, so both the analytic terms below and the KHR extensions read the
// weathered surface), the minimum perceptual roughness clamp (avoids the
// GGX D term overflowing to +Inf in the RGBA16F HDR buffer at roughness -> 0,
// which later resurfaces as NaN through Bloom's Karis average), then every
// KHR extension's texture sample and per-instance factor.
//
// base_color/metallic/roughness are the RAW (pre-weather) G-Buffer values;
// normal/view are already reconstructed by the caller; tangent_angle is
// RT1.a (PackTangentAngle's output - see Normal.hlsli), needed to rebuild the
// tangent basis for the clearcoat normal map and anisotropy direction, both
// of which can differ from the base normal map's own tangent space.
// material_texcoord is the model UV recovered from the VisibilityBuffer
// (UnpackVisibilityTexcoord) - the deferred/raytraced callers have no
// interpolated texcoord of their own, only the G-Buffer/VisibilityBuffer.
GBufferMaterial ResolveGBufferMaterial(float3 base_color, float metallic, float roughness, float3 normal, float3 view, ModelInstance material_instance, float2 material_texcoord, float tangent_angle)
{
	WeatherMaterial weather_material = ResolveWeatherMaterial(base_color, roughness, normal);
	base_color = weather_material.base_color_;
	roughness = weather_material.roughness_;
	float wetness = weather_material.wetness_;
	float puddle = weather_material.puddle_;

	float specular_factor = material_instance.specular_factor_;
	if (material_instance.specular_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> specular_texture = ResourceDescriptorHeap[material_instance.specular_texture_index_];
		specular_factor *= specular_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).a;
	}

	float3 specular_color = material_instance.specular_color_;
	if (material_instance.specular_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> specular_color_texture = ResourceDescriptorHeap[material_instance.specular_color_texture_index_];
		specular_color *= specular_color_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).rgb;
	}

	float iridescence_factor = material_instance.iridescence_factor_;
	if (material_instance.iridescence_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> iridescence_texture = ResourceDescriptorHeap[material_instance.iridescence_texture_index_];
		iridescence_factor *= iridescence_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).r;
	}

	float iridescence_thickness = material_instance.iridescence_thickness_;
	if (material_instance.iridescence_thickness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> iridescence_thickness_texture = ResourceDescriptorHeap[material_instance.iridescence_thickness_texture_index_];
		iridescence_thickness *= iridescence_thickness_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).g;
	}

	float transmission_factor = material_instance.transmission_factor_;
	if (material_instance.transmission_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> transmission_texture = ResourceDescriptorHeap[material_instance.transmission_texture_index_];
		transmission_factor *= transmission_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).r;
	}

	// KHR_materials_ior/specular: dielectric F0 computed on the spot from IOR.
	// Metals use base_color instead (see f0_ below).
	float dielectric = (material_instance.ior_ - 1.0) / (material_instance.ior_ + 1.0);
	dielectric *= dielectric;
	float3 dielectric_f0 = saturate(dielectric * specular_color * specular_factor);

	// KHR_materials_iridescence: a simplified approximation (a phase shift
	// driven by thickness/view angle, not physically accurate thin-film
	// interference) that tints F0 with a pseudo-iridescent color.
	if (iridescence_factor > 0.0)
	{
		float normal_dot_view_for_iridescence = saturate(dot(normal, view));
		float phase = iridescence_thickness * 0.01 * normal_dot_view_for_iridescence + material_instance.iridescence_ior_;
		float3 iridescence_shift = sin(float3(phase, phase + 2.094395, phase + 4.18879)) * 0.5 + 0.5;
		dielectric_f0 = lerp(dielectric_f0, iridescence_shift, saturate(iridescence_factor));
	}

	// KHR_materials_transmission: transmitted light does not diffusely
	// reflect. The actual see-through refraction (screen-space/raytraced
	// Refraction) is separate - this only attenuates the diffuse response.
	float3 diffuse_color = base_color * (1.0 - metallic) * (1.0 - transmission_factor);
	float3 f0 = lerp(dielectric_f0, base_color, metallic);

	// A wet surface's thin water film raises grazing-angle reflectance; a
	// puddle (standing water) pushes it further, toward the water itself.
	f0 = lerp(f0, max(f0, 0.02), wetness);
	f0 = lerp(f0, max(f0, 0.05), puddle);

	float clearcoat_factor = material_instance.clearcoat_factor_;
	if (material_instance.clearcoat_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> clearcoat_texture = ResourceDescriptorHeap[material_instance.clearcoat_texture_index_];
		clearcoat_factor *= clearcoat_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).r;
	}

	float clearcoat_roughness = material_instance.clearcoat_roughness_;
	if (material_instance.clearcoat_roughness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> clearcoat_roughness_texture = ResourceDescriptorHeap[material_instance.clearcoat_roughness_texture_index_];
		clearcoat_roughness *= clearcoat_roughness_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).g;
	}

	// KHR_materials_clearcoat's normal map needs its own tangent basis - the
	// clearcoat layer can carry a normal independent of the base surface, so
	// only the coat's normal is swapped here, never the base normal.
	float3 clearcoat_normal = normal;
	if (material_instance.clearcoat_normal_texture_index_ != 0xFFFFFFFF)
	{
		float3 clearcoat_tangent;
		float clearcoat_handedness;
		UnpackTangentAngle(normal, tangent_angle, clearcoat_tangent, clearcoat_handedness);

		float3 clearcoat_bitangent = cross(normal, clearcoat_tangent) * clearcoat_handedness;
		float3x3 clearcoat_tbn = float3x3(clearcoat_tangent, clearcoat_bitangent, normal);

		Texture2D<float4> clearcoat_normal_texture = ResourceDescriptorHeap[material_instance.clearcoat_normal_texture_index_];
		float3 clearcoat_normal_sample = clearcoat_normal_texture.SampleLevel(sampler_linear_wrap, material_texcoord, 0).xyz * 2.0 - 1.0;
		clearcoat_normal = normalize(mul(clearcoat_normal_sample, clearcoat_tbn));
	}

	// KHR_materials_anisotropy: direction is a 2D vector in tangent space,
	// combined from anisotropyRotation (constant) and anisotropyTexture.rg
	// (per-pixel); strength is anisotropyStrength * texture.b. Reuses the
	// same RT1.a-derived tangent basis as the clearcoat normal above.
	float3 anisotropy_tangent = float3(1, 0, 0);
	float3 anisotropy_bitangent = float3(0, 1, 0);
	float anisotropy_strength = material_instance.anisotropy_;
	if (abs(anisotropy_strength) > 0.0)
	{
		float2 anisotropy_direction = float2(cos(material_instance.anisotropy_rotation_), sin(material_instance.anisotropy_rotation_));
		if (material_instance.anisotropy_texture_index_ != 0xFFFFFFFF)
		{
			Texture2D<float4> anisotropy_texture = ResourceDescriptorHeap[material_instance.anisotropy_texture_index_];
			float3 anisotropy_sample = anisotropy_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).rgb;

			float2 texture_direction = anisotropy_sample.rg * 2.0 - 1.0;
			anisotropy_direction = float2(texture_direction.x * anisotropy_direction.x - texture_direction.y * anisotropy_direction.y, texture_direction.x * anisotropy_direction.y + texture_direction.y * anisotropy_direction.x);
			anisotropy_strength *= anisotropy_sample.b;
		}

		float3 base_tangent;
		float base_handedness;
		UnpackTangentAngle(normal, tangent_angle, base_tangent, base_handedness);
		float3 base_bitangent = cross(normal, base_tangent) * base_handedness;

		anisotropy_tangent = normalize(base_tangent * anisotropy_direction.x + base_bitangent * anisotropy_direction.y);
		anisotropy_bitangent = normalize(cross(normal, anisotropy_tangent));
	}

	float3 sheen_color = material_instance.sheen_color_;
	if (material_instance.sheen_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> sheen_color_texture = ResourceDescriptorHeap[material_instance.sheen_color_texture_index_];
		sheen_color *= sheen_color_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).rgb;
	}

	float sheen_roughness_value = material_instance.sheen_roughness_;
	if (material_instance.sheen_roughness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> sheen_roughness_texture = ResourceDescriptorHeap[material_instance.sheen_roughness_texture_index_];
		sheen_roughness_value *= sheen_roughness_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).a;
	}
	float sheen_roughness = max(sheen_roughness_value, 0.001);

	GBufferMaterial result;
	result.diffuse_color_ = diffuse_color;
	result.f0_ = f0;
	result.roughness_ = roughness;
	result.clearcoat_factor_ = clearcoat_factor;
	result.clearcoat_roughness_ = clearcoat_roughness;
	result.clearcoat_normal_ = clearcoat_normal;
	result.sheen_color_ = sheen_color;
	result.sheen_roughness_ = sheen_roughness;
	result.anisotropy_tangent_ = anisotropy_tangent;
	result.anisotropy_bitangent_ = anisotropy_bitangent;
	result.anisotropy_strength_ = anisotropy_strength;
	return result;
}

#endif // __MATERIAL_HLSL__
