#ifndef __UNORDERED_ACCESSES_HLSL__
#define __UNORDERED_ACCESSES_HLSL__

#include "Denoiser.hlsli"
#include "../Model/Opaque/GeometryBuffer.hlsli"
#include "Material.hlsli"
#include "../PostProcess/PostProcess.hlsli"
#include "../DLSS/Dlss.hlsli"
#include "../Light/Cluster.hlsli"
#include "../Model/Model.hlsli"
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

struct UnorderedAccessIndices
{
	ClusterAssignUnorderedAccessIndices cluster_assign_;
	ShadowAccumulationUnorderedAccessIndices shadow_accumulation_;
	AmbientOcclusionAccumulationUnorderedAccessIndices ambient_occlusion_accumulation_;
	GlobalIlluminationAccumulationUnorderedAccessIndices global_illumination_accumulation_;
	ReflectionAccumulationUnorderedAccessIndices reflection_accumulation_;
	DlssUnorderedAccessIndices dlss_;

	PostProcessUnorderedAccessIndices post_process_;

	OitUnorderedAccessIndices oit_;
	GeometryBufferUnorderedAccessIndices geometry_buffer_;
	MaterialSortUnorderedAccessIndices material_sort_;
	ShadowUnorderedAccessIndices shadow_;
	AmbientOcclusionUnorderedAccessIndices ambient_occlusion_;
	SubsurfaceScatteringUnorderedAccessIndices subsurface_scattering_;
	ReflectionUnorderedAccessIndices reflection_;
	RefractionUnorderedAccessIndices refraction_;
	GlobalIlluminationUnorderedAccessIndices global_illumination_;
	CloudUnorderedAccessIndices cloud_;
	StarUnorderedAccessIndices star_;
	WeatherParticleUnorderedAccessIndices weather_particle_;
	VolumetricLightUnorderedAccessIndices volumetric_light_;
};
ConstantBuffer<UnorderedAccessIndices> unordered_access_indices : register(b1, space1);

#endif // __UNORDERED_ACCESSES_HLSL__
