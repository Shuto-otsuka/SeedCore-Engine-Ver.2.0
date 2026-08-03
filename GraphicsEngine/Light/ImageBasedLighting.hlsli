#ifndef __IMAGE_BASED_LIGHTING_HLSL__
#define __IMAGE_BASED_LIGHTING_HLSL__

// Bindless IBL sampling. The environment / irradiance / prefilter cubes and the
// BRDF lookup table are addressed through StructuredIndices (set per frame by
// the SkyRenderer). ASCII only (this file is included by BRDF; DXC breaks on
// non-ASCII in .hlsli).

#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"

float4 SampleGgxLookupTable(float2 sample_point)
{
	Texture2D<float2> ggx_lookup_table = ResourceDescriptorHeap[structured_indices.sky_.brdf_lut_index_];
	return float4(ggx_lookup_table.Sample(sampler_linear_clamp, sample_point), 0.0, 0.0);
}

float4 SampleDiffuseIrradianceEnvironmentMap(float3 direction)
{
	TextureCube<float4> diffuse_irradiance = ResourceDescriptorHeap[structured_indices.sky_.diffuse_irradiance_index_];
	return diffuse_irradiance.Sample(sampler_linear_clamp, direction);
}

float4 SampleSpecularPrefilteredRadianceEnvironmentMap(float3 direction, float roughness)
{
	TextureCube<float4> specular_prefiltered = ResourceDescriptorHeap[structured_indices.sky_.specular_prefiltered_index_];

	uint width, height, number_of_levels;
	specular_prefiltered.GetDimensions(0, width, height, number_of_levels);

	float lod = roughness * float(number_of_levels - 1);
	return specular_prefiltered.SampleLevel(sampler_linear_clamp, direction, lod);
}

// Environment cube sampled for the skybox background (mip 0).
float4 SampleSkyboxEnvironment(float3 direction)
{
	TextureCube<float4> environment = ResourceDescriptorHeap[structured_indices.sky_.environment_cube_index_];
	return environment.SampleLevel(sampler_linear_clamp, direction, 0);
}

#endif // __IMAGE_BASED_LIGHTING_HLSL__
