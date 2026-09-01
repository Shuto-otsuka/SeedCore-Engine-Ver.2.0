#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	struct VirtualShadowMapSettings
	{
		Uint32 virtualResolutionLog2_ = 14;
		Uint32 pagePoolSizeMib_ = 128;
		Uint32 clipmapLevelCount_ = 6;
		Float softnessRadius_ = 0.5f;
		Float depthBias_ = 0.002f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("virtualResolutionLog2", virtualResolutionLog2_);
			archive.TryField("pagePoolSizeMib", pagePoolSizeMib_);
			archive.TryField("clipmapLevelCount", clipmapLevelCount_);
			archive.TryField("softnessRadius", softnessRadius_);
			archive.TryField("depthBias", depthBias_);
		}
	};

	struct CascadedShadowMapSettings
	{
		Uint32 cascadeCount_ = 4;
		Float splitLambda_ = 0.85f;
		Uint32 pcfKernelSize_ = 3;
		Float cascadeBlendWidth_ = 0.1f;
		Float depthBias_ = 0.002f;
		Bool stabilize_ = true;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("cascadeCount", cascadeCount_);
			archive.TryField("splitLambda", splitLambda_);
			archive.TryField("pcfKernelSize", pcfKernelSize_);
			archive.TryField("cascadeBlendWidth", cascadeBlendWidth_);
			archive.TryField("depthBias", depthBias_);
			archive.TryField("stabilize", stabilize_);
		}
	};

	struct SignedDistanceFieldReflectionSettings
	{
		Float strength_ = 1.0f;
		Float maxDistance_ = 1000.0f;
		Float coneAngle_ = 0.05f;
		Float normalBias_ = 0.05f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("strength", strength_);
			archive.TryField("maxDistance", maxDistance_);
			archive.TryField("coneAngle", coneAngle_);
			archive.TryField("normalBias", normalBias_);
		}
	};

	struct DynamicDiffuseGlobalIlluminationSettings
	{
		Float intensity_ = 1.0f;
		Float probeSpacing_ = 2.0f;
		Float hysteresis_ = 0.97f;
		Float normalBias_ = 0.25f;
		Uint32 raysPerProbe_ = 128;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("intensity", intensity_);
			archive.TryField("probeSpacing", probeSpacing_);
			archive.TryField("hysteresis", hysteresis_);
			archive.TryField("normalBias", normalBias_);
			archive.TryField("raysPerProbe", raysPerProbe_);
		}
	};

	struct RasterizationContext
	{
		Bool virtualShadowMapEnabled_ = false;
		VirtualShadowMapSettings virtualShadowMap_;

		Bool cascadedShadowMapEnabled_ = false;
		CascadedShadowMapSettings cascadedShadowMap_;

		Bool signedDistanceFieldReflectionEnabled_ = false;
		SignedDistanceFieldReflectionSettings signedDistanceFieldReflection_;

		Bool dynamicDiffuseGlobalIlluminationEnabled_ = false;
		DynamicDiffuseGlobalIlluminationSettings dynamicDiffuseGlobalIllumination_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("virtualShadowMapEnabled", virtualShadowMapEnabled_);
			archive.TryField("virtualShadowMap", virtualShadowMap_);
			archive.TryField("cascadedShadowMapEnabled", cascadedShadowMapEnabled_);
			archive.TryField("cascadedShadowMap", cascadedShadowMap_);
			archive.TryField("signedDistanceFieldReflectionEnabled", signedDistanceFieldReflectionEnabled_);
			archive.TryField("signedDistanceFieldReflection", signedDistanceFieldReflection_);
			archive.TryField("dynamicDiffuseGlobalIlluminationEnabled", dynamicDiffuseGlobalIlluminationEnabled_);
			archive.TryField("dynamicDiffuseGlobalIllumination", dynamicDiffuseGlobalIllumination_);
		}
	};

	inline String SerializeRasterizationContext(const RasterizationContext& settings)
	{
		JsonOutputArchive archive;
		archive.Field("rasterization", settings);
		return archive.Dump();
	}

	inline RasterizationContext DeserializeRasterizationContext(const String& json)
	{
		RasterizationContext settings;

		if (json.view().empty())
		{
			return settings;
		}

		JsonInputArchive archive;
		if (!archive.Parse(json))
		{
			return RasterizationContext();
		}

		archive.TryField("rasterization", settings);

		return settings;
	}
}
