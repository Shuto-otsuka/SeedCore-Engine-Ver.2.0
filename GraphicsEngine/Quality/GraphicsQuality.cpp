#include <GraphicsEngine/Quality/GraphicsQuality.h>

namespace SeedCore
{
	Bool GraphicsQuality::IsEffectEnabledInFamily(GraphicsEffect effect, GraphicsEffectFamily family, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization)
	{
		switch (family)
		{
		case GraphicsEffectFamily::Raytracing:
			switch (effect)
			{
			case GraphicsEffect::Shadow:
				return raytracing.shadowEnabled_;
			case GraphicsEffect::Reflection:
				return raytracing.reflectionEnabled_;
			case GraphicsEffect::GlobalIllumination:
				return raytracing.globalIlluminationEnabled_;
			case GraphicsEffect::AmbientOcclusion:
				return raytracing.ambientOcclusionEnabled_;
			}
			return false;

		case GraphicsEffectFamily::ScreenSpace:
			switch (effect)
			{
			case GraphicsEffect::Shadow:
				return false;
			case GraphicsEffect::Reflection:
				return screenSpace.reflectionEnabled_;
			case GraphicsEffect::GlobalIllumination:
				return screenSpace.globalIlluminationEnabled_;
			case GraphicsEffect::AmbientOcclusion:
				return screenSpace.groundTruthAmbientOcclusionEnabled_ || screenSpace.ambientOcclusionEnabled_;
			}
			return false;

		case GraphicsEffectFamily::Rasterization:
			switch (effect)
			{
			case GraphicsEffect::Shadow:
				return rasterization.virtualShadowMapEnabled_ || rasterization.cascadedShadowMapEnabled_;
			case GraphicsEffect::Reflection:
				return rasterization.signedDistanceFieldReflectionEnabled_;
			case GraphicsEffect::GlobalIllumination:
				return rasterization.dynamicDiffuseGlobalIlluminationEnabled_;
			case GraphicsEffect::AmbientOcclusion:
				return false;
			}
			return false;
		}

		return false;
	}

	Bool GraphicsQuality::IsEffectClaimedOutside(GraphicsEffect effect, GraphicsEffectFamily family, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization)
	{
		const GraphicsEffectFamily families[] = { GraphicsEffectFamily::Raytracing, GraphicsEffectFamily::ScreenSpace, GraphicsEffectFamily::Rasterization };

		for (GraphicsEffectFamily other : families)
		{
			if (other == family)
			{
				continue;
			}

			if (IsEffectEnabledInFamily(effect, other, raytracing, screenSpace, rasterization))
			{
				return true;
			}
		}

		return false;
	}

	Bool GraphicsQuality::IsEnableCheckboxInteractive(GraphicsEffect effect, GraphicsEffectFamily family, GraphicsQualityPreset preset, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization)
	{
		if (preset != GraphicsQualityPreset::Custom)
		{
			return false;
		}

		return !IsEffectClaimedOutside(effect, family, raytracing, screenSpace, rasterization);
	}

	void GraphicsQuality::ApplyPreset(GraphicsQualityPreset preset, RaytracingContext& raytracing, ScreenSpaceContext& screenSpace, RasterizationContext& rasterization)
	{
		if (preset == GraphicsQualityPreset::Custom)
		{
			return;
		}

		raytracing.shadowEnabled_ = preset == GraphicsQualityPreset::High;
		raytracing.reflectionEnabled_ = preset == GraphicsQualityPreset::High;
		raytracing.globalIlluminationEnabled_ = preset == GraphicsQualityPreset::High;
		raytracing.ambientOcclusionEnabled_ = preset == GraphicsQualityPreset::High;

		rasterization.virtualShadowMapEnabled_ = preset == GraphicsQualityPreset::Medium;
		rasterization.signedDistanceFieldReflectionEnabled_ = preset == GraphicsQualityPreset::Medium;
		rasterization.dynamicDiffuseGlobalIlluminationEnabled_ = preset == GraphicsQualityPreset::Medium;
		screenSpace.groundTruthAmbientOcclusionEnabled_ = preset == GraphicsQualityPreset::Medium;

		rasterization.cascadedShadowMapEnabled_ = preset == GraphicsQualityPreset::Low;
		screenSpace.globalIlluminationEnabled_ = preset == GraphicsQualityPreset::Low;
		screenSpace.reflectionEnabled_ = preset == GraphicsQualityPreset::Low;
		screenSpace.ambientOcclusionEnabled_ = preset == GraphicsQualityPreset::Low;
	}
}
