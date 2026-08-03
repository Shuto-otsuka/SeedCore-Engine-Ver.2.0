#ifndef __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__
#define __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__

#include "ImageBasedLighting.hlsli"

float3 FresnelSchlick(float3 f0, float3 f90, float view_dot_half)
{
	return f0 + (f90 - f0) * pow(clamp(1.0 - view_dot_half, 0.0, 1.0), 5.0);
}

float VisibilityGgx(float normal_dot_light, float normal_dot_view, float alpha_roughness)
{
	float alpha_roughness_squared = alpha_roughness * alpha_roughness;

	float ggx_view = normal_dot_light * sqrt(normal_dot_view * normal_dot_view * (1.0 - alpha_roughness_squared) + alpha_roughness_squared);
	float ggx_light = normal_dot_view * sqrt(normal_dot_light * normal_dot_light * (1.0 - alpha_roughness_squared) + alpha_roughness_squared);

	float ggx = ggx_view + ggx_light;
	return (ggx > 0.0) ? 0.5 / ggx : 0.0;
}

float DistributionGgx(float normal_dot_half, float alpha_roughness)
{
	const float PI = 3.14159265358979;
	float alpha_roughness_squared = alpha_roughness * alpha_roughness;
	float f = (normal_dot_half * normal_dot_half) * (alpha_roughness_squared - 1.0) + 1.0;
	return alpha_roughness_squared / (PI * f * f);
}

float3 BrdfSpecularGgx(float3 f0, float3 f90, float alpha_roughness, float view_dot_half, float normal_dot_light, float normal_dot_view, float normal_dot_half)
{
	float3 fresnel = FresnelSchlick(f0, f90, view_dot_half);
	float visibility = VisibilityGgx(normal_dot_light, normal_dot_view, alpha_roughness);
	float distribution = DistributionGgx(normal_dot_half, alpha_roughness);

	return fresnel * visibility * distribution;
}

float3 BrdfLambertian(float3 f0, float3 f90, float3 diffuse_color, float view_dot_half)
{
	const float PI = 3.14159265358979;
	return (1.0 - FresnelSchlick(f0, f90, view_dot_half)) * (diffuse_color / PI);
}

float3 ImageBasedLightingRadianceLambertian(float3 normal, float3 view, float roughness, float3 diffuse_color, float3 f0)
{
	float normal_dot_view = clamp(dot(normal, view), 0.0, 1.0);

	float2 brdf_sample_point = clamp(float2(normal_dot_view, roughness), 0.0, 1.0);
	float2 f_ab = SampleGgxLookupTable(brdf_sample_point).rg;

	float3 irradiance = SampleDiffuseIrradianceEnvironmentMap(normal).rgb;

	float3 fresnel_roughness = max(1.0 - roughness, f0) - f0;
	float3 specular_color = f0 + fresnel_roughness * pow(1.0 - normal_dot_view, 5.0);
	float3 single_scattering = specular_color * f_ab.x + f_ab.y;

	float energy_missing = (1.0 - (f_ab.x + f_ab.y));
	float3 fresnel_average = (f0 + (1.0 - f0) / 21.0);
	float3 multi_scattering = energy_missing * single_scattering * fresnel_average / (1.0 - fresnel_average * energy_missing);
	float3 diffuse_contribution = diffuse_color * (1.0 - single_scattering + multi_scattering);

	return (multi_scattering + diffuse_contribution) * irradiance;
}

float3 ImageBasedLightingRadianceGgx(float3 normal, float3 view, float roughness, float3 f0)
{
	float normal_dot_view = clamp(dot(normal, view), 0.0, 1.0);

	float2 brdf_sample_point = clamp(float2(normal_dot_view, roughness), 0.0, 1.0);
	float2 f_ab = SampleGgxLookupTable(brdf_sample_point).rg;

	float3 reflection = normalize(reflect(-view, normal));
	float3 specular_light = SampleSpecularPrefilteredRadianceEnvironmentMap(reflection, roughness).rgb;

	float3 fresnel_roughness = max(1.0 - roughness, f0) - f0;
	float3 specular_color = f0 + fresnel_roughness * pow(1.0 - normal_dot_view, 5.0);
	float3 single_scattering = specular_color * f_ab.x + f_ab.y;

	return specular_light * single_scattering;
}

#endif // __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__
