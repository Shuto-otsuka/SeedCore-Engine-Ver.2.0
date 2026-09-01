#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/ScreenSpace/ScreenSpaceContext.h>
#include <GraphicsEngine/Rasterization/RasterizationContext.h>

namespace SeedCore
{
	enum class GraphicsQualityPreset : Uint32
	{
		Custom = 0,
		High = 1,
		Medium = 2,
		Low = 3,
	};

	enum class GraphicsEffect : Uint32
	{
		Shadow = 0,
		Reflection = 1,
		GlobalIllumination = 2,
		AmbientOcclusion = 3,
	};

	enum class GraphicsEffectFamily : Uint32
	{
		Raytracing = 0,
		ScreenSpace = 1,
		Rasterization = 2,
	};

	class SEEDCORE_API GraphicsQuality
	{
	public:
		static Bool IsEffectEnabledInFamily(GraphicsEffect effect, GraphicsEffectFamily family, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization);

		static Bool IsEffectClaimedOutside(GraphicsEffect effect, GraphicsEffectFamily family, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization);

		static Bool IsEnableCheckboxInteractive(GraphicsEffect effect, GraphicsEffectFamily family, GraphicsQualityPreset preset, const RaytracingContext& raytracing, const ScreenSpaceContext& screenSpace, const RasterizationContext& rasterization);

		static void ApplyPreset(GraphicsQualityPreset preset, RaytracingContext& raytracing, ScreenSpaceContext& screenSpace, RasterizationContext& rasterization);
	};
}
