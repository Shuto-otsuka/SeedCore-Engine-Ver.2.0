#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	struct GroundTruthAmbientOcclusionSettings
	{
		Float radius_ = 0.5f;
		Float falloffRange_ = 0.615f;
		Float power_ = 1.0f;
		Uint32 sliceCount_ = 3;
		Uint32 stepsPerSlice_ = 3;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("radius", radius_);
			archive.TryField("falloffRange", falloffRange_);
			archive.TryField("power", power_);
			archive.TryField("sliceCount", sliceCount_);
			archive.TryField("stepsPerSlice", stepsPerSlice_);
		}
	};

	struct ScreenSpaceAmbientOcclusionSettings
	{
		Float radius_ = 0.5f;
		Float bias_ = 0.025f;
		Float intensity_ = 1.0f;
		Float power_ = 1.0f;
		Uint32 sampleCount_ = 16;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("radius", radius_);
			archive.TryField("bias", bias_);
			archive.TryField("intensity", intensity_);
			archive.TryField("power", power_);
			archive.TryField("sampleCount", sampleCount_);
		}
	};

	struct ScreenSpaceGlobalIlluminationSettings
	{
		Float intensity_ = 1.0f;
		Float rayLength_ = 2.0f;
		Float thickness_ = 0.25f;
		Uint32 sampleCount_ = 8;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("intensity", intensity_);
			archive.TryField("rayLength", rayLength_);
			archive.TryField("thickness", thickness_);
			archive.TryField("sampleCount", sampleCount_);
		}
	};

	struct ScreenSpaceReflectionSettings
	{
		Float strength_ = 1.0f;
		Float maxRoughness_ = 0.6f;
		Float thickness_ = 0.25f;
		Uint32 maxStepCount_ = 64;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("strength", strength_);
			archive.TryField("maxRoughness", maxRoughness_);
			archive.TryField("thickness", thickness_);
			archive.TryField("maxStepCount", maxStepCount_);
		}
	};

	struct ScreenSpaceContext
	{
		Bool groundTruthAmbientOcclusionEnabled_ = false;
		GroundTruthAmbientOcclusionSettings groundTruthAmbientOcclusion_;

		Bool ambientOcclusionEnabled_ = false;
		ScreenSpaceAmbientOcclusionSettings ambientOcclusion_;

		Bool globalIlluminationEnabled_ = false;
		ScreenSpaceGlobalIlluminationSettings globalIllumination_;

		Bool reflectionEnabled_ = false;
		ScreenSpaceReflectionSettings reflection_;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("groundTruthAmbientOcclusionEnabled", groundTruthAmbientOcclusionEnabled_);
			archive.TryField("groundTruthAmbientOcclusion", groundTruthAmbientOcclusion_);
			archive.TryField("ambientOcclusionEnabled", ambientOcclusionEnabled_);
			archive.TryField("ambientOcclusion", ambientOcclusion_);
			archive.TryField("globalIlluminationEnabled", globalIlluminationEnabled_);
			archive.TryField("globalIllumination", globalIllumination_);
			archive.TryField("reflectionEnabled", reflectionEnabled_);
			archive.TryField("reflection", reflection_);
		}
	};

	inline String SerializeScreenSpaceContext(const ScreenSpaceContext& settings)
	{
		JsonOutputArchive archive;
		archive.Field("screenSpace", settings);
		return archive.Dump();
	}

	inline ScreenSpaceContext DeserializeScreenSpaceContext(const String& json)
	{
		ScreenSpaceContext settings;

		if (json.view().empty())
		{
			return settings;
		}

		JsonInputArchive archive;
		if (!archive.Parse(json))
		{
			return ScreenSpaceContext();
		}

		archive.TryField("screenSpace", settings);

		return settings;
	}
}
