#ifndef __SHADER_RESOURCES_HLSL__
#define __SHADER_RESOURCES_HLSL__

#include "Denoiser.hlsli"
#include "../Model/Opaque/GeometryBuffer.hlsli"
#include "../PostProcess/PostProcess.hlsli"
#include "../Light/Cluster.hlsli"
#include "../Shape/HUD/HUD.hlsli"
#include "../Texture/Image.hlsli"
#include "../Font/Font.hlsli"
#include "../Movie/Movie.hlsli"
#include "../Model/Model.hlsli"
#include "../Sky/Sky.hlsli"
#include "../Raytracing/Raytracing.hlsli"
#include "../Raytracing/Shadow/Shadow.hlsli"
#include "../Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli"
#include "../Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli"
#include "../Raytracing/Reflection/ReflectionReSTIR.hlsli"
#include "../Raytracing/Refraction/Refraction.hlsli"
#include "../Raytracing/GlobalIllumination/GlobalIlluminationReSTIR.hlsli"
#include "../Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../Raytracing/VolumetricStar/VolumetricStar.hlsli"
#include "../Environment/WeatherParticle.hlsli"
#include "../Raytracing/VolumetricLight/VolumetricLight.hlsli"

struct ShaderResourceIndices
{
	LightShaderResourceIndices light_;
	ClusterAssignShaderResourceIndices cluster_assign_;
	ShadowAccumulationShaderResourceIndices shadow_accumulation_;
	AmbientOcclusionAccumulationShaderResourceIndices ambient_occlusion_accumulation_;
	GlobalIlluminationAccumulationShaderResourceIndices global_illumination_accumulation_;
	ReflectionAccumulationShaderResourceIndices reflection_accumulation_;

	PostProcessShaderResourceIndices post_process_;
	HUDShaderResourceIndices hud_;

	ImageShaderResourceIndices image_;
	FontShaderResourceIndices font_;
	MovieShaderResourceIndices movie_;
	ModelShaderResourceIndices model_;
	GeometryBufferShaderResourceIndices geometry_buffer_;
	SkyShaderResourceIndices sky_;
	RaytracingShaderResourceIndices raytracing_;
	ShadowShaderResourceIndices shadow_;
	AmbientOcclusionShaderResourceIndices ambient_occlusion_;
	SubsurfaceScatteringShaderResourceIndices subsurface_scattering_;
	ReflectionShaderResourceIndices reflection_;
	RefractionShaderResourceIndices refraction_;
	GlobalIlluminationShaderResourceIndices global_illumination_;
	CloudShaderResourceIndices cloud_;
	StarShaderResourceIndices star_;
	WeatherParticleShaderResourceIndices weather_particle_;
	VolumetricLightShaderResourceIndices volumetric_light_;
};
ConstantBuffer<ShaderResourceIndices> shader_resource_indices : register(b0, space1);

#endif // __SHADER_RESOURCES_HLSL__
