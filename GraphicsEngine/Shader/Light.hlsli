#ifndef __LIGHT_HLSL__
#define __LIGHT_HLSL__

#include "../Model/Model.hlsli"
#include "../Model/Opaque/PbrShading.hlsli"
#include "../Model/Opaque/PhongShading.hlsli"
#include "../Model/Opaque/ToonShading.hlsli"

struct LightConstantData
{
	float3 directional_direction_;
	float directional_intensity_;
	float4 directional_color_;

	// Moon light, populated only when DaySystem drives this frame (see
	// LightSystem::Gather's celestial parameter). Zero otherwise.
	float3 moon_direction_;
	float moon_intensity_;
	float4 moon_color_;
	float moon_phase_;

	// 0 = full day, 1 = full night. Drives star visibility/shooting-star
	// chance in VolumetricStarRT.hlsl. Zero when DaySystem is not driving.
	float night_factor_;
	float moon_angular_radius_;
	float light_constant_padding1_;

	uint point_light_count_;
	uint spot_light_count_;
	uint rect_light_count_;
	uint point_light_index_;
	uint spot_light_index_;
	uint rect_light_index_;
	uint cluster_data_shader_resource_view_index_;
	uint cluster_light_list_shader_resource_view_index_;
	uint cluster_count_x_;
	uint cluster_count_y_;
	float2 light_constant_padding0_;

	// Weather state (WeatherSystem via LightSystem::Gather's weather
	// parameter). Zero when the scene has no Weather component.
	float wetness_;         // 0..1, wet-surface shading (rain + a drying-out tail).
	float snow_coverage_;   // 0..1, snow accumulation shading (builds up, melts over time).
	float thunder_flash_;   // 0..1, brief sky/ambient brightness burst on a lightning strike.
	float weather_padding0_;

	float snow_intensity_;  // 0..1, "is it snowing right now" - drives the falling-snow screen overlay.
	float thunder_seed_;    // Re-rolled each strike - randomizes the lightning bolt shape.
	float2 weather_padding1_;
};

struct PointLightData
{
	float3 position;
	float range;
	float4 color;
	float intensity;
	float3 point_light_padding_;
};

struct SpotLightData
{
	float3 position;
	float range;
	float3 direction;
	float cos_half_angle;
	float4 color;
	float intensity;
	float softness;
	float2 spot_light_padding_;
};

struct RectLightData
{
	float3 position;
	float intensity;
	float3 right;
	float half_width;
	float3 up;
	float half_height;
	float3 normal;
	float range;
	float4 color;
};

// Closest point on the rectangle (center + local axes, half extents) to p.
// Used as the "most representative point" for area-light shading.
float3 ClosestPointOnRect(float3 p, float3 center, float3 right, float3 up, float half_width, float half_height)
{
	float3 delta = p - center;
	float local_x = clamp(dot(delta, right), -half_width, half_width);
	float local_y = clamp(dot(delta, up), -half_height, half_height);
	return center + right * local_x + up * local_y;
}

float AttenuateDistance(float distance, float range)
{
	float ratio = saturate(distance / range);
	float ratio2 = ratio * ratio;
	float attenuation = saturate(1.0 - ratio2 * ratio2);
	return attenuation * attenuation / max(distance * distance, 0.0001);
}

// Picks the per-shading-model direct light response for one light. Shared by
// Model/Opaque/DeferredLightingPS.hlsl (the primary G-Buffer surface, looping
// over every deterministic light) and Raytracing/Shadow/ShadowRT.hlsl (the
// ReSTIR-picked punctual light's stochastic full-BRDF estimate) so a punctual
// light's specular response is identical between the two - IBL/shadow/AO/
// reflection/GI/fog/weather stay each caller's own concern; only "how to shade
// against one direct light" lives here.
float3 EvalDirectLightDispatch(uint shading_model, float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness, float clearcoat, float clearcoat_roughness, float3 clearcoat_normal, float3 sheen_color, float sheen_roughness, float3 anisotropy_tangent, float3 anisotropy_bitangent, float anisotropy_strength)
{
	if (shading_model == SHADING_MODEL_PHONG)
	{
		float shininess = lerp(128.0, 2.0, roughness);
		return EvalDirectLightPhong(normal, view, light_direction, diffuse_color, f0, shininess);
	}

	if (shading_model == SHADING_MODEL_TOON)
	{
		float shininess = lerp(64.0, 4.0, roughness);
		return EvalDirectLightToon(normal, view, light_direction, diffuse_color, f0, shininess);
	}

	if (shading_model == SHADING_MODEL_LAMBERT)
	{
		return diffuse_color * saturate(dot(normal, light_direction));
	}

	return EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat, clearcoat_roughness, clearcoat_normal, sheen_color, sheen_roughness, anisotropy_tangent, anisotropy_bitangent, anisotropy_strength);
}

#endif // __LIGHT_HLSL__
