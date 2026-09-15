#ifndef __LIGHT_HLSL__
#define __LIGHT_HLSL__

#include "../Model/Model.hlsli"
#include "../Model/Opaque/PbrShading.hlsli"
#include "../Model/Opaque/PhongShading.hlsli"
#include "../Model/Opaque/ToonShading.hlsli"
#include "../Model/Opaque/FurShading.hlsli"
#include "../Shader/Constants.hlsli"

/**
* [EN]
* Sun and moon share one axis - moon_'s light travels exactly opposite
* direction_ (see CelestialSystem::Compute: moonDirection_ is always
* -sunDirection_), so there is only one direction_ field here. The
* moon_-prefixed fields are populated only when DaySystem drives this frame
* (see LightSystem::Gather's celestial parameter); zero otherwise.
* moon_phase_/moon_angular_radius_ are sky-disc rendering parameters
* (VolumetricStarRT.hlsl's MoonDiscColor), not lighting terms.
*/
struct DirectionalLightConstantBuffer
{
	float3 direction_;
	float directional_light_padding_0_;

	float sun_intensity_;
	float sun_angular_radius_;
	float4 sun_color_;
	
	float moon_intensity_;
	float moon_angular_radius_;
	float4 moon_color_;
	float moon_phase_;

};

ConstantBuffer<DirectionalLightConstantBuffer> GetDirectionalLightConstantBuffer()
{
	return ResourceDescriptorHeap[constant_indices.directional_light_index_];
}

struct PointLightStructuredBuffer
{
	float3 position_;
	float range_;
	float4 color_;
	float intensity_;
	float3 point_light_padding_0_;
};

StructuredBuffer<PointLightStructuredBuffer> GetPointLightStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct SpotLightStructuredBuffer
{
	float3 position_;
	float range_;
	float3 direction_;
	float cos_half_angle_;
	float4 color_;
	float intensity_;
	float softness_;
	float2 spot_light_padding_0_;
};

StructuredBuffer<SpotLightStructuredBuffer> GetSpotLightStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct RectLightStructuredBuffer
{
	float3 position_;
	float intensity_;
	float3 right_;
	float half_width_;
	float3 up_;
	float half_height_;
	float3 normal_;
	float range_;
	float4 color_;
};

StructuredBuffer<RectLightStructuredBuffer> GetRectLightStructuredBuffer(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct LightConstantBuffer
{
	// 0 = full day, 1 = full night. Drives star visibility/shooting-star
	// chance in VolumetricStarRT.hlsl. Zero when DaySystem is not driving.
	float night_factor_;
	uint point_light_count_;
	uint spot_light_count_;
	uint rect_light_count_;

	uint cluster_count_x_;
	uint cluster_count_y_;
	float2 light_constant_padding_0_;
};

// Picks the per-shading-model direct light response for one light. Shared by
// Model/Opaque/DeferredLightingPS.hlsl (the primary G-Buffer surface, looping
// over every deterministic light) and Raytracing/Shadow/ShadowRT.hlsl (the
// ReSTIR-picked punctual light's stochastic full-BRDF estimate) so a punctual
// light's specular response is identical between the two - IBL/shadow/AO/
// reflection/GI/fog/weather stay each caller's own concern; only "how to shade
// against one direct light" lives here.
float3 EvalDirectLightDispatch(uint shading_model, float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness, float clearcoat, float clearcoat_roughness, float3 clearcoat_normal, float3 sheen_color, float sheen_roughness, float3 anisotropy_tangent, float3 anisotropy_bitangent, float anisotropy_strength)
{
	const float PI = 3.14159265358979;

	if (shading_model == SHADING_MODEL_PHONG)
	{
		float phong_alpha = max(roughness * roughness, 0.01);
		float shininess = max(0.5 / (phong_alpha * phong_alpha) - 0.5, 1.0);
		return EvalDirectLightPhong(normal, view, light_direction, diffuse_color, f0, shininess);
	}

	if (shading_model == SHADING_MODEL_TOON)
	{
		float shininess = lerp(64.0, 4.0, roughness);
		return EvalDirectLightToon(normal, view, light_direction, diffuse_color, f0, shininess);
	}

	if (shading_model == SHADING_MODEL_LAMBERT)
	{
		return diffuse_color / PI * saturate(dot(normal, light_direction));
	}

	if (shading_model == SHADING_MODEL_FUR)
	{
		float fur_shininess = lerp(48.0, 4.0, roughness);
		return EvalDirectLightFur(normal, anisotropy_tangent, view, light_direction, diffuse_color, f0, fur_shininess);
	}

	return EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat, clearcoat_roughness, clearcoat_normal, sheen_color, sheen_roughness, anisotropy_tangent, anisotropy_bitangent, anisotropy_strength);
}

#endif // __LIGHT_HLSL__
