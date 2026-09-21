#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Assert.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/System/LightSystem.h>

namespace SeedCore
{
	inline constexpr Uint32 lensFlareMaxAxisCount = 7;

	struct ShadowAccumulationShaderResourceIndices
	{
		Uint directionalHistoryIndex_ = 0;
		Uint directionalAccumulatedIndex_ = 0;
		Uint directionalVisibilityIndex_ = 0;
		Uint directionalMomentsHistoryIndex_ = 0;

		Uint directionalMomentsIndex_ = 0;
		Uint directionalAtrousScratch0Index_ = 0;
		Uint directionalAtrousScratch1Index_ = 0;
		Uint punctualHistoryIndex_ = 0;

		Uint punctualAccumulatedIndex_ = 0;
		Uint punctualRadianceIndex_ = 0;
		Uint punctualMomentsHistoryIndex_ = 0;
		Uint punctualMomentsIndex_ = 0;

		Uint punctualAtrousScratch0Index_ = 0;
		Uint punctualAtrousScratch1Index_ = 0;
		Uint historyLengthHistoryIndex_ = 0;
		Uint historyLengthIndex_ = 0;

		Uint depthNormalHistoryIndex_ = 0;
		Uint depthNormalIndex_ = 0;
		Vector2 shadowAccumulationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ShadowAccumulationShaderResourceIndices, 80, "Shader/Denoiser.hlsli");

	struct ShadowAccumulationUnorderedAccessIndices
	{
		Uint directionalAccumulatedIndex_ = 0;
		Uint directionalMomentsIndex_ = 0;
		Uint directionalAtrousScratch0Index_ = 0;
		Uint directionalAtrousScratch1Index_ = 0;

		Uint directionalDenoisedIndex_ = 0;
		Uint punctualAccumulatedIndex_ = 0;
		Uint punctualMomentsIndex_ = 0;
		Uint punctualAtrousScratch0Index_ = 0;

		Uint punctualAtrousScratch1Index_ = 0;
		Uint punctualDenoisedIndex_ = 0;
		Uint historyLengthIndex_ = 0;
		Uint depthNormalIndex_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ShadowAccumulationUnorderedAccessIndices, 48, "Shader/Denoiser.hlsli");

	struct AmbientOcclusionAccumulationShaderResourceIndices
	{
		Uint historyIndex_ = 0;
		Uint opennessIndex_ = 0;
		Vector2 ambientOcclusionAccumulationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(AmbientOcclusionAccumulationShaderResourceIndices, 16, "Shader/Denoiser.hlsli");

	struct AmbientOcclusionAccumulationUnorderedAccessIndices
	{
		Uint accumulatedIndex_ = 0;
		Vector3 ambientOcclusionAccumulationUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(AmbientOcclusionAccumulationUnorderedAccessIndices, 16, "Shader/Denoiser.hlsli");

	struct GlobalIlluminationAccumulationShaderResourceIndices
	{
		Uint historyIndex_ = 0;
		Uint radianceIndex_ = 0;
		Uint atrousScratch0Index_ = 0;
		Uint atrousScratch1Index_ = 0;

		Uint reservoirHistoryIndex_ = 0;
		Uint reservoirWriteIndex_ = 0;
		Vector2 globalIlluminationAccumulationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(GlobalIlluminationAccumulationShaderResourceIndices, 32, "Shader/Denoiser.hlsli");

	struct GlobalIlluminationAccumulationUnorderedAccessIndices
	{
		Uint accumulatedIndex_ = 0;
		Uint atrousScratch0Index_ = 0;
		Uint atrousScratch1Index_ = 0;
		Uint reservoirIndex_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(GlobalIlluminationAccumulationUnorderedAccessIndices, 16, "Shader/Denoiser.hlsli");

	struct ReflectionAccumulationShaderResourceIndices
	{
		Uint historyIndex_ = 0;
		Uint accumulatedIndex_ = 0;
		Uint radianceIndex_ = 0;
		Uint atrousScratch0Index_ = 0;

		Uint atrousScratch1Index_ = 0;
		Uint momentsHistoryIndex_ = 0;
		Uint momentsIndex_ = 0;
		Uint historyLengthHistoryIndex_ = 0;

		Uint historyLengthIndex_ = 0;
		Uint depthNormalHistoryIndex_ = 0;
		Uint depthNormalIndex_ = 0;
		Uint reservoirHistoryIndex_ = 0;

		Uint reservoirWriteIndex_ = 0;
		Vector3 reflectionAccumulationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ReflectionAccumulationShaderResourceIndices, 64, "Shader/Denoiser.hlsli");

	struct ReflectionAccumulationUnorderedAccessIndices
	{
		Uint accumulatedIndex_ = 0;
		Uint atrousScratch0Index_ = 0;
		Uint atrousScratch1Index_ = 0;
		Uint momentsIndex_ = 0;

		Uint historyLengthIndex_ = 0;
		Uint depthNormalIndex_ = 0;
		Uint denoisedIndex_ = 0;
		Uint reservoirIndex_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ReflectionAccumulationUnorderedAccessIndices, 32, "Shader/Denoiser.hlsli");

	struct DlssUnorderedAccessIndices
	{
		Uint normalRoughnessIndex_ = 0;
		Uint specularAlbedoIndex_ = 0;
		Uint diffuseAlbedoIndex_ = 0;
		Uint dlssUnorderedAccessPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(DlssUnorderedAccessIndices, 16, "DLSS/Dlss.hlsli");

	struct ExposureIndices
	{
		Uint autoExposureEnabled_ = 0;
		Float exposureCompensation_ = 0.0f;
		Float minLogLuminance_ = 0.0f;
		Float maxLogLuminance_ = 0.0f;

		Float keyValue_ = 0.0f;
		Float adaptSpeedToBright_ = 0.0f;
		Float adaptSpeedToDark_ = 0.0f;
		Uint exposurePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ExposureIndices, 32, "PostProcess/PostProcess.hlsli");

	struct ExposureUnorderedAccessIndices
	{
		Uint histogramIndex_ = 0;
		Uint exposureIndex_ = 0;
		Vector2 exposureUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ExposureUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ToneMappingIndices
	{
		Uint toneMappingEnabled_ = 0;
		Uint toneMappingMode_ = 0;
		Vector2 toneMappingPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ToneMappingIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensFlareIndices
	{
		Uint enabled_ = 0;
		Float threshold_ = 0.0f;
		Float intensity_ = 0.0f;
		Float streakLength_ = 0.0f;

		Float streakAttenuation_ = 0.0f;
		Float chromaticAberration_ = 0.0f;
		Float angleOffset_ = 0.0f;
		Uint ghostCount_ = 4;

		Float ghostDispersal_ = 0.3f;
		Float ghostIntensity_ = 0.3f;
		Float haloWidth_ = 0.45f;
		Uint axisCount_ = 3;

		Float spikeVariation_ = 0.0f;
		Vector3 lensFlarePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensFlareIndices, 64, "PostProcess/PostProcess.hlsli");

	struct LensFlareUnorderedAccessIndices
	{
		Uint index_ = 0;
		Vector3 lensFlareUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensFlareUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensFlareShaderResourceIndices
	{
		Uint index_ = 0;
		Vector3 lensFlareShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensFlareShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensFlareStreakAxisIndices
	{
		Uint pingIndex_ = 0;
		Uint pongIndex_ = 0;
	};

	struct LensFlareStreakUnorderedAccessIndices
	{
		Uint axisBufferIndex_ = 0;
		Uint brightIndex_ = 0;
		Vector2 lensFlareStreakUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensFlareStreakUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensFlareStreakShaderResourceIndices
	{
		Uint axisBufferIndex_ = 0;
		Uint brightIndex_ = 0;
		Vector2 lensFlareStreakShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensFlareStreakShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct BloomIndices
	{
		Uint enabled_ = 0;
		Float threshold_ = 0.0f;
		Float softKnee_ = 0.0f;
		Float intensity_ = 0.0f;

		Float filterRadius_ = 0.0f;
		Vector3 bloomPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(BloomIndices, 32, "PostProcess/PostProcess.hlsli");

	struct BloomUnorderedAccessIndices
	{
		Uint level0Index_ = 0;
		Uint level1Index_ = 0;
		Uint level2Index_ = 0;
		Uint level3Index_ = 0;

		Uint level4Index_ = 0;
		Uint level5Index_ = 0;
		Vector2 bloomUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(BloomUnorderedAccessIndices, 32, "PostProcess/PostProcess.hlsli");

	struct BloomShaderResourceIndices
	{
		Uint level0Index_ = 0;
		Uint level1Index_ = 0;
		Uint level2Index_ = 0;
		Uint level3Index_ = 0;

		Uint level4Index_ = 0;
		Uint level5Index_ = 0;
		Vector2 bloomShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(BloomShaderResourceIndices, 32, "PostProcess/PostProcess.hlsli");

	struct AnamorphicFlareIndices
	{
		Uint enabled_ = 0;
		Float threshold_ = 0.0f;
		Float intensity_ = 0.0f;
		Float streakLength_ = 0.0f;

		Float attenuation_ = 0.0f;
		Vector3 anamorphicFlarePadding0_;

		Float tint_[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	};
	SC_STATIC_ASSERT_SIZE(AnamorphicFlareIndices, 48, "PostProcess/PostProcess.hlsli");

	struct AnamorphicFlareUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Uint pingIndex_ = 0;
		Uint pongIndex_ = 0;
		Uint anamorphicFlareUnorderedAccessPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(AnamorphicFlareUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct AnamorphicFlareShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Uint pingIndex_ = 0;
		Uint pongIndex_ = 0;
		Uint anamorphicFlareShaderResourcePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(AnamorphicFlareShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ColorGradingRangeIndices
	{
		Float temperature_ = 0.0f;
		Float saturation_ = 1.0f;
		Float contrast_ = 1.0f;
		Float gamma_ = 1.0f;

		Float gain_ = 1.0f;
		Float offset_ = 0.0f;
		Vector2 colorGradingRangePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ColorGradingRangeIndices, 32, "PostProcess/PostProcess.hlsli");

	struct ColorGradingIndices
	{
		Uint enabled_ = 0;
		Float shadowsMax_ = 0.0f;
		Float highlightsMin_ = 0.0f;
		Uint colorGradingPadding0_ = 0;

		ColorGradingRangeIndices global_;
		ColorGradingRangeIndices shadows_;
		ColorGradingRangeIndices midtones_;
		ColorGradingRangeIndices highlights_;
	};
	SC_STATIC_ASSERT_SIZE(ColorGradingIndices, 144, "PostProcess/PostProcess.hlsli");

	struct ColorGradingUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 colorGradingUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ColorGradingUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ColorGradingShaderResourceIndices
	{
		Uint sourceIndex_ = 0;
		Uint outputIndex_ = 0;
		Vector2 colorGradingShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ColorGradingShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensDistortionIndices
	{
		Uint enabled_ = 0;
		Float k1_ = 0.0f;
		Float k2_ = 0.0f;
		Float k3_ = 0.0f;

		Float scale_ = 1.0f;
		Vector3 lensDistortionPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensDistortionIndices, 32, "PostProcess/PostProcess.hlsli");

	struct LensDistortionUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 lensDistortionUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensDistortionUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct LensDistortionShaderResourceIndices
	{
		Uint sourceIndex_ = 0;
		Vector3 lensDistortionShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(LensDistortionShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ChromaticAberrationIndices
	{
		Uint enabled_ = 0;
		Float intensity_ = 0.0f;
		Uint sampleCount_ = 8;
		Uint chromaticAberrationPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ChromaticAberrationIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ChromaticAberrationUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 chromaticAberrationUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ChromaticAberrationUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct ChromaticAberrationShaderResourceIndices
	{
		Uint sourceIndex_ = 0;
		Vector3 chromaticAberrationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ChromaticAberrationShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct VignetteIndices
	{
		Uint enabled_ = 0;
		Float intensity_ = 0.0f;
		Float exponent_ = 4.0f;
		Uint vignettePadding0_ = 0;

		Float color_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};
	SC_STATIC_ASSERT_SIZE(VignetteIndices, 32, "PostProcess/PostProcess.hlsli");

	struct VignetteUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 vignetteUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(VignetteUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct VignetteShaderResourceIndices
	{
		Uint sourceIndex_ = 0;
		Vector3 vignetteShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(VignetteShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct DepthOfFieldIndices
	{
		Uint enabled_ = 0;
		Float focusDistance_ = 0.0f;
		Float focusRange_ = 0.0f;
		Float maxBlurRadius_ = 0.0f;
	};
	SC_STATIC_ASSERT_SIZE(DepthOfFieldIndices, 16, "PostProcess/PostProcess.hlsli");

	struct DepthOfFieldUnorderedAccessIndices
	{
		Uint index_ = 0;
		Vector3 depthOfFieldUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(DepthOfFieldUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct DepthOfFieldShaderResourceIndices
	{
		Uint index_ = 0;
		Vector3 depthOfFieldShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(DepthOfFieldShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct BokehIndices
	{
		Uint enabled_ = 0;
		Float highlightThreshold_ = 0.0f;
		Float highlightIntensity_ = 0.0f;
		Uint bladeCount_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(BokehIndices, 16, "PostProcess/PostProcess.hlsli");

	struct SharpnessIndices
	{
		Uint enabled_ = 0;
		Float amount_ = 0.0f;
		Vector2 sharpnessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(SharpnessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct SharpnessUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 sharpnessUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(SharpnessUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct SharpnessShaderResourceIndices
	{
		Uint sourceIndex_ = 0;
		Vector3 sharpnessShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(SharpnessShaderResourceIndices, 16, "PostProcess/PostProcess.hlsli");

	struct FilmGrainIndices
	{
		Uint enabled_ = 0;
		Uint colored_ = 0;
		Float intensity_ = 0.0f;
		Float size_ = 0.0f;

		Float luminanceResponse_ = 0.0f;
		Vector3 filmGrainPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(FilmGrainIndices, 32, "PostProcess/PostProcess.hlsli");

	struct FilmGrainUnorderedAccessIndices
	{
		Uint destinationIndex_ = 0;
		Vector3 filmGrainUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(FilmGrainUnorderedAccessIndices, 16, "PostProcess/PostProcess.hlsli");

	struct PostProcessConstantBuffer
	{
		Uint lensStageEnabled_ = 0;
		Vector3 postProcessPadding0_;

		ExposureIndices exposure_;
		ToneMappingIndices toneMapping_;
		LensFlareIndices lensFlare_;
		BloomIndices bloom_;
		AnamorphicFlareIndices anamorphicFlare_;
		ColorGradingIndices colorGrading_;
		LensDistortionIndices lensDistortion_;
		ChromaticAberrationIndices chromaticAberration_;
		VignetteIndices vignette_;
		DepthOfFieldIndices depthOfField_;
		BokehIndices bokeh_;
		SharpnessIndices sharpness_;
		FilmGrainIndices filmGrain_;
	};
	SC_STATIC_ASSERT_SIZE(PostProcessConstantBuffer, 512, "PostProcess/PostProcess.hlsli");

	struct PostProcessUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Vector3 postProcessUnorderedAccessPadding0_;

		ExposureUnorderedAccessIndices exposure_;
		LensFlareUnorderedAccessIndices lensFlare_;
		LensFlareStreakUnorderedAccessIndices lensFlareStreak_;
		BloomUnorderedAccessIndices bloom_;
		AnamorphicFlareUnorderedAccessIndices anamorphicFlare_;
		ColorGradingUnorderedAccessIndices colorGrading_;
		LensDistortionUnorderedAccessIndices lensDistortion_;
		ChromaticAberrationUnorderedAccessIndices chromaticAberration_;
		VignetteUnorderedAccessIndices vignette_;
		DepthOfFieldUnorderedAccessIndices depthOfField_;
		SharpnessUnorderedAccessIndices sharpness_;
		FilmGrainUnorderedAccessIndices filmGrain_;
	};
	SC_STATIC_ASSERT_SIZE(PostProcessUnorderedAccessIndices, 224, "PostProcess/PostProcess.hlsli");

	struct PostProcessShaderResourceIndices
	{
		Uint sourceColorIndex_ = 0;
		Uint lensStageIndex_ = 0;
		Vector2 postProcessShaderResourcePadding0_;

		LensFlareShaderResourceIndices lensFlare_;
		LensFlareStreakShaderResourceIndices lensFlareStreak_;
		BloomShaderResourceIndices bloom_;
		AnamorphicFlareShaderResourceIndices anamorphicFlare_;
		ColorGradingShaderResourceIndices colorGrading_;
		LensDistortionShaderResourceIndices lensDistortion_;
		ChromaticAberrationShaderResourceIndices chromaticAberration_;
		VignetteShaderResourceIndices vignette_;
		DepthOfFieldShaderResourceIndices depthOfField_;
		SharpnessShaderResourceIndices sharpness_;
	};
	SC_STATIC_ASSERT_SIZE(PostProcessShaderResourceIndices, 192, "PostProcess/PostProcess.hlsli");

	struct HUDShaderResourceIndices
	{
		Uint uiColorAlphaIndex_ = 0;
		Vector3 hudPadding0_;
	};
	SC_STATIC_ASSERT_ALIGNED16(HUDShaderResourceIndices);

	struct TextureShaderResourceIndices
	{
		Uint spriteIndex_ = 0;
		Uint billboardIndex_ = 0;
		Vector2 textureShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(TextureShaderResourceIndices, 16, "Texture/Texture.hlsli");

	struct FontShaderResourceIndices
	{
		Uint spriteIndex_ = 0;
		Uint billboardIndex_ = 0;
		Vector2 fontShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(FontShaderResourceIndices, 16, "Font/Font.hlsli");

	struct MovieShaderResourceIndices
	{
		Uint spriteIndex_ = 0;
		Uint billboardIndex_ = 0;
		Uint fullscreenIndex_ = 0;
		Uint movieShaderResourcePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(MovieShaderResourceIndices, 16, "Movie/Movie.hlsli");

	struct ModelShaderResourceIndices
	{
		Uint instanceIndex_ = 0;
		Uint boneMatrixIndex_ = 0;
		Uint previousBoneMatrixIndex_ = 0;
		Uint morphWeightIndex_ = 0;

		Uint previousMorphWeightIndex_ = 0;
		Uint hiZIndex_ = 0;
		Uint silhouetteIndex_ = 0;
		Uint modelShaderResourcePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ModelShaderResourceIndices, 32, "Model/Model.hlsli");

	struct OitUnorderedAccessIndices
	{
		Uint headPointerIndex_ = 0;
		Uint fragmentBufferIndex_ = 0;
		Uint counterIndex_ = 0;
		Uint oitUnorderedAccessPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(OitUnorderedAccessIndices, 16, "Model/Model.hlsli");

	struct GeometryBufferShaderResourceIndices
	{
		Uint index0_ = 0;
		Uint index1_ = 0;
		Uint index2_ = 0;
		Uint index3_ = 0;

		Uint index4_ = 0;
		Uint depthIndex_ = 0;
		Vector2 geometryBufferShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(GeometryBufferShaderResourceIndices, 32, "Model/Opaque/GeometryBuffer.hlsli");

	struct GeometryBufferUnorderedAccessIndices
	{
		Uint index0_ = 0;
		Uint index1_ = 0;
		Uint index2_ = 0;
		Uint index3_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(GeometryBufferUnorderedAccessIndices, 16, "Model/Opaque/GeometryBuffer.hlsli");

	struct MaterialSortUnorderedAccessIndices
	{
		Uint bucketIndex_ = 0;
		Uint sortedPixelListIndex_ = 0;
		Vector2 materialSortUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(MaterialSortUnorderedAccessIndices, 16, "Shader/Material.hlsli");

	struct SkyShaderResourceIndices
	{
		Uint environmentCubeIndex_ = 0;
		Uint diffuseIrradianceIndex_ = 0;
		Uint specularPrefilteredIndex_ = 0;
		Uint brdfLutIndex_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(SkyShaderResourceIndices, 16, "Sky/Sky.hlsli");

	struct RaytracingShaderResourceIndices
	{
		Uint tlasIndex_ = 0;
		Uint instanceDataIndex_ = 0;
		Vector2 raytracingShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(RaytracingShaderResourceIndices, 16, "Raytracing/Raytracing.hlsli");

	struct ShadowShaderResourceIndices
	{
		Uint rawVisibilityIndex_ = 0;
		Vector3 shadowShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ShadowShaderResourceIndices, 16, "Raytracing/Shadow/Shadow.hlsli");

	struct ShadowUnorderedAccessIndices
	{
		Uint rawVisibilityIndex_ = 0;
		Vector3 shadowUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ShadowUnorderedAccessIndices, 16, "Raytracing/Shadow/Shadow.hlsli");

	struct AmbientOcclusionShaderResourceIndices
	{
		Uint rawIndex_ = 0;
		Vector3 ambientOcclusionShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(AmbientOcclusionShaderResourceIndices, 16, "Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli");

	struct AmbientOcclusionUnorderedAccessIndices
	{
		Uint rawIndex_ = 0;
		Vector3 ambientOcclusionUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(AmbientOcclusionUnorderedAccessIndices, 16, "Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli");

	struct SubsurfaceScatteringShaderResourceIndices
	{
		Uint transmittanceIndex_ = 0;
		Vector3 subsurfaceScatteringShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(SubsurfaceScatteringShaderResourceIndices, 16, "Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli");

	struct SubsurfaceScatteringUnorderedAccessIndices
	{
		Uint transmittanceIndex_ = 0;
		Vector3 subsurfaceScatteringUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(SubsurfaceScatteringUnorderedAccessIndices, 16, "Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli");

	struct ReflectionShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Uint confidenceIndex_ = 0;
		Vector2 reflectionShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ReflectionShaderResourceIndices, 16, "Raytracing/Reflection/ReflectionReSTIR.hlsli");

	struct ReflectionUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Uint confidenceIndex_ = 0;
		Vector2 reflectionUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(ReflectionUnorderedAccessIndices, 16, "Raytracing/Reflection/ReflectionReSTIR.hlsli");

	struct RefractionShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Vector3 refractionShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(RefractionShaderResourceIndices, 16, "Raytracing/Refraction/Refraction.hlsli");

	struct RefractionUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Vector3 refractionUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(RefractionUnorderedAccessIndices, 16, "Raytracing/Refraction/Refraction.hlsli");

	struct GlobalIlluminationShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Uint confidenceIndex_ = 0;
		Vector2 globalIlluminationShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(GlobalIlluminationShaderResourceIndices, 16, "Raytracing/GlobalIllumination/GlobalIlluminationReSTIR.hlsli");

	struct GlobalIlluminationUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Uint confidenceIndex_ = 0;
		Vector2 globalIlluminationUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(GlobalIlluminationUnorderedAccessIndices, 16, "Raytracing/GlobalIllumination/GlobalIlluminationReSTIR.hlsli");

	struct CloudShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Uint shapeNoiseIndex_ = 0;
		Uint detailNoiseIndex_ = 0;
		Uint cloudShaderResourcePadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(CloudShaderResourceIndices, 16, "Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli");

	struct CloudUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Uint shapeNoiseIndex_ = 0;
		Uint detailNoiseIndex_ = 0;
		Uint cloudUnorderedAccessPadding0_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(CloudUnorderedAccessIndices, 16, "Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli");

	struct StarShaderResourceIndices
	{
		Uint outputIndex_ = 0;
		Vector3 starShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(StarShaderResourceIndices, 16, "Raytracing/VolumetricStar/VolumetricStar.hlsli");

	struct StarUnorderedAccessIndices
	{
		Uint outputIndex_ = 0;
		Vector3 starUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(StarUnorderedAccessIndices, 16, "Raytracing/VolumetricStar/VolumetricStar.hlsli");

	struct WeatherParticleShaderResourceIndices
	{
		Uint rainParticleIndex_ = 0;
		Uint snowParticleIndex_ = 0;
		Vector2 weatherParticleShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(WeatherParticleShaderResourceIndices, 16, "Environment/WeatherParticle.hlsli");

	struct WeatherParticleUnorderedAccessIndices
	{
		Uint rainParticleIndex_ = 0;
		Uint snowParticleIndex_ = 0;
		Vector2 weatherParticleUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(WeatherParticleUnorderedAccessIndices, 16, "Environment/WeatherParticle.hlsli");

	struct VolumetricLightShaderResourceIndices
	{
		Uint integrationIndex_ = 0;
		Vector3 volumetricLightShaderResourcePadding0_;
	};
	SC_STATIC_ASSERT_SIZE(VolumetricLightShaderResourceIndices, 16, "Raytracing/VolumetricLight/VolumetricLight.hlsli");

	struct VolumetricLightUnorderedAccessIndices
	{
		Uint densityIndex_ = 0;
		Uint integrationIndex_ = 0;
		Vector2 volumetricLightUnorderedAccessPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(VolumetricLightUnorderedAccessIndices, 16, "Raytracing/VolumetricLight/VolumetricLight.hlsli");

	struct ShaderResourceIndices
	{
		LightShaderResourceIndices light_;
		ClusterAssignShaderResourceIndices clusterAssign_;
		ShadowAccumulationShaderResourceIndices shadowAccumulation_;
		AmbientOcclusionAccumulationShaderResourceIndices ambientOcclusionAccumulation_;
		GlobalIlluminationAccumulationShaderResourceIndices globalIlluminationAccumulation_;
		ReflectionAccumulationShaderResourceIndices reflectionAccumulation_;
		PostProcessShaderResourceIndices postProcess_;
		HUDShaderResourceIndices hud_;
		TextureShaderResourceIndices texture_;
		FontShaderResourceIndices font_;
		MovieShaderResourceIndices movie_;
		ModelShaderResourceIndices model_;
		GeometryBufferShaderResourceIndices geometryBuffer_;
		SkyShaderResourceIndices sky_;
		RaytracingShaderResourceIndices raytracing_;
		ShadowShaderResourceIndices shadow_;
		AmbientOcclusionShaderResourceIndices ambientOcclusion_;
		SubsurfaceScatteringShaderResourceIndices subsurfaceScattering_;
		ReflectionShaderResourceIndices reflection_;
		RefractionShaderResourceIndices refraction_;
		GlobalIlluminationShaderResourceIndices globalIllumination_;
		CloudShaderResourceIndices cloud_;
		StarShaderResourceIndices star_;
		WeatherParticleShaderResourceIndices weatherParticle_;
		VolumetricLightShaderResourceIndices volumetricLight_;
	};
	SC_STATIC_ASSERT_SIZE(ShaderResourceIndices, 752, "Shader/ShaderResources.hlsli");

	struct UnorderedAccessIndices
	{
		ClusterAssignUnorderedAccessIndices clusterAssign_;
		ShadowAccumulationUnorderedAccessIndices shadowAccumulation_;
		AmbientOcclusionAccumulationUnorderedAccessIndices ambientOcclusionAccumulation_;
		GlobalIlluminationAccumulationUnorderedAccessIndices globalIlluminationAccumulation_;
		ReflectionAccumulationUnorderedAccessIndices reflectionAccumulation_;
		DlssUnorderedAccessIndices dlss_;
		PostProcessUnorderedAccessIndices postProcess_;
		OitUnorderedAccessIndices oit_;
		GeometryBufferUnorderedAccessIndices geometryBuffer_;
		MaterialSortUnorderedAccessIndices materialSort_;
		ShadowUnorderedAccessIndices shadow_;
		AmbientOcclusionUnorderedAccessIndices ambientOcclusion_;
		SubsurfaceScatteringUnorderedAccessIndices subsurfaceScattering_;
		ReflectionUnorderedAccessIndices reflection_;
		RefractionUnorderedAccessIndices refraction_;
		GlobalIlluminationUnorderedAccessIndices globalIllumination_;
		CloudUnorderedAccessIndices cloud_;
		StarUnorderedAccessIndices star_;
		WeatherParticleUnorderedAccessIndices weatherParticle_;
		VolumetricLightUnorderedAccessIndices volumetricLight_;
	};
	SC_STATIC_ASSERT_SIZE(UnorderedAccessIndices, 576, "Shader/UnorderedAccesses.hlsli");

	struct ConstantIndices
	{
		Uint sceneIndex_ = 0;
		Uint lightIndex_ = 0;
		Uint clusterAssignIndex_ = 0;
		Uint postProcessIndex_ = 0;

		Uint weatherIndex_ = 0;
		Uint directionalLightIndex_ = 0;
		Uint skyIndex_ = 0;
		Uint oitIndex_ = 0;

		Uint furIndex_ = 0;
		Uint shadowIndex_ = 0;
		Uint ambientOcclusionIndex_ = 0;
		Uint subsurfaceScatteringIndex_ = 0;

		Uint reflectionIndex_ = 0;
		Uint refractionIndex_ = 0;
		Uint globalIlluminationIndex_ = 0;
		Uint cloudIndex_ = 0;

		Uint starIndex_ = 0;
		Uint weatherParticleIndex_ = 0;
		Uint volumetricLightIndex_ = 0;
		Uint colliderIndex_ = 0;
	};
	SC_STATIC_ASSERT_SIZE(ConstantIndices, 80, "Shader/Constants.hlsli");

	class BindlessHeap;

	class ConstantIndicesSystem
	{
	public:
		ConstantIndicesSystem(ID3D12Device* device, BindlessHeap* heap);
		~ConstantIndicesSystem() = default;

		void UploadEditor();

		void UploadGame();

		void UploadCanvas();

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS EditorConstantAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GameConstantAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS CanvasConstantAddress()const;

	public:
		void SetEditorSceneIndex(Uint index);

		void SetGameSceneIndex(Uint index);

		void SetCanvasSceneIndex(Uint index);

		void SetLightIndex(Uint index);

		void SetClusterAssignIndex(Uint index);

		void SetWeatherIndex(Uint index);

		void SetDirectionalLightIndex(Uint index);

		void SetSkyIndex(Uint index);

		void SetOitIndex(Uint index);

		void SetEditorPostProcessIndex(Uint index);

		void SetGamePostProcessIndex(Uint index);

		void SetModelFurIndex(Uint index);

		void SetShadowRayConstantIndex(Uint index);

		void SetAmbientOcclusionRayConstantIndex(Uint index);

		void SetSubsurfaceScatteringRayConstantIndex(Uint index);

		void SetReflectionRayConstantIndex(Uint index);

		void SetRefractionRayConstantIndex(Uint index);

		void SetGlobalIlluminationRayConstantIndex(Uint index);

		void SetCloudRayConstantIndex(Uint index);

		void SetStarRayConstantIndex(Uint index);

		void SetWeatherParticleRayConstantIndex(Uint index);

		void SetVolumetricLightRayConstantIndex(Uint index);

		void SetEditorColliderIndex(Uint index);

		void SetCanvasColliderIndex(Uint index);

	private:
		ConstantIndices editorConstantIndices_{};
		ConstantIndices gameConstantIndices_{};
		ConstantIndices canvasConstantIndices_{};

		ResourcePtr<ConstantBuffer<ConstantIndices>> editorConstantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<ConstantIndices>> gameConstantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<ConstantIndices>> canvasConstantIndicesBuffer_;
	};

	class ShaderResourceIndicesSystem
	{
	public:
		ShaderResourceIndicesSystem(ID3D12Device* device, BindlessHeap* heap);
		~ShaderResourceIndicesSystem() = default;

		void UploadEditor();

		void UploadGame();

		void UploadCanvas();

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS EditorAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GameAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS CanvasAddress()const;

	public:
		void SetLightIndices(const LightShaderResourceIndices& values);

		void SetClusterAssignIndices(const ClusterAssignShaderResourceIndices& values);

		void SetEditorShadowAccumulationIndices(const ShadowAccumulationShaderResourceIndices& values);

		void SetGameShadowAccumulationIndices(const ShadowAccumulationShaderResourceIndices& values);

		void SetEditorAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationShaderResourceIndices& values);

		void SetGameAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationShaderResourceIndices& values);

		void SetEditorGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationShaderResourceIndices& values);

		void SetGameGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationShaderResourceIndices& values);

		void SetEditorReflectionAccumulationIndices(const ReflectionAccumulationShaderResourceIndices& values);

		void SetGameReflectionAccumulationIndices(const ReflectionAccumulationShaderResourceIndices& values);

		void SetEditorPostProcessIndices(const PostProcessShaderResourceIndices& values);

		void SetGamePostProcessIndices(const PostProcessShaderResourceIndices& values);

		void SetUIColorAlphaIndex(Uint index);

		void SetTextureSpriteIndex(Uint index);

		void SetTextureBillboardIndex(Uint index);

		void SetFontSpriteIndex(Uint index);

		void SetFontBillboardIndex(Uint index);

		void SetMovieSpriteIndex(Uint index);

		void SetMovieBillboardIndex(Uint index);

		void SetMovieFullscreenIndex(Uint index);

		void SetModelInstanceIndex(Uint index);

		void SetModelBoneMatrixIndex(Uint index);

		void SetModelPreviousBoneMatrixIndex(Uint index);

		void SetModelMorphWeightIndex(Uint index);

		void SetModelPreviousMorphWeightIndex(Uint index);

		void SetHiZIndex(Uint index);

		void SetSilhouetteIndex(Uint index);

		void SetGBuffer0Index(Uint index);

		void SetGBuffer1Index(Uint index);

		void SetGBuffer2Index(Uint index);

		void SetGBuffer3Index(Uint index);

		void SetGBuffer4Index(Uint index);

		void SetGBufferDepthIndex(Uint index);

		void SetSkyEnvironmentCubeIndex(Uint index);

		void SetSkyDiffuseIrradianceIndex(Uint index);

		void SetSkySpecularPrefilteredIndex(Uint index);

		void SetSkyBrdfLutIndex(Uint index);

		void SetTLASIndex(Uint index);

		void SetReflectionInstanceDataIndex(Uint index);

		void SetShadowRawVisibilityShaderResourceViewIndex(Uint index);

		void SetAmbientOcclusionRawShaderResourceViewIndex(Uint index);

		void SetSubsurfaceScatteringTransmittanceShaderResourceViewIndex(Uint index);

		void SetReflectionOutputShaderResourceViewIndex(Uint index);

		void SetReflectionConfidenceShaderResourceViewIndex(Uint index);

		void SetRefractionOutputShaderResourceViewIndex(Uint index);

		void SetGlobalIlluminationOutputShaderResourceViewIndex(Uint index);

		void SetGlobalIlluminationConfidenceShaderResourceViewIndex(Uint index);

		void SetCloudOutputShaderResourceViewIndex(Uint index);

		void SetCloudShapeNoiseShaderResourceViewIndex(Uint index);

		void SetCloudDetailNoiseShaderResourceViewIndex(Uint index);

		void SetStarOutputShaderResourceViewIndex(Uint index);

		void SetRainParticleShaderResourceViewIndex(Uint index);

		void SetSnowParticleShaderResourceViewIndex(Uint index);

		void SetVolumetricLightIntegrationShaderResourceViewIndex(Uint index);

	private:
		ShaderResourceIndices editorIndices_{};
		ShaderResourceIndices gameIndices_{};
		ShaderResourceIndices canvasIndices_{};

		ResourcePtr<ConstantBuffer<ShaderResourceIndices>> editorBuffer_;
		ResourcePtr<ConstantBuffer<ShaderResourceIndices>> gameBuffer_;
		ResourcePtr<ConstantBuffer<ShaderResourceIndices>> canvasBuffer_;
	};

	class UnorderedAccessIndicesSystem
	{
	public:
		UnorderedAccessIndicesSystem(ID3D12Device* device, BindlessHeap* heap);
		~UnorderedAccessIndicesSystem() = default;

		void UploadEditor();

		void UploadGame();

		void UploadCanvas();

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS EditorAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GameAddress()const;

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS CanvasAddress()const;

	public:
		void SetClusterAssignIndices(const ClusterAssignUnorderedAccessIndices& values);

		void SetEditorShadowAccumulationIndices(const ShadowAccumulationUnorderedAccessIndices& values);

		void SetGameShadowAccumulationIndices(const ShadowAccumulationUnorderedAccessIndices& values);

		void SetEditorAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationUnorderedAccessIndices& values);

		void SetGameAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationUnorderedAccessIndices& values);

		void SetEditorGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationUnorderedAccessIndices& values);

		void SetGameGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationUnorderedAccessIndices& values);

		void SetEditorReflectionAccumulationIndices(const ReflectionAccumulationUnorderedAccessIndices& values);

		void SetGameReflectionAccumulationIndices(const ReflectionAccumulationUnorderedAccessIndices& values);

		void SetEditorDlssNormalRoughnessUnorderedAccessViewIndex(Uint index);

		void SetGameDlssNormalRoughnessUnorderedAccessViewIndex(Uint index);

		void SetEditorDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index);

		void SetGameDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index);

		void SetEditorDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index);

		void SetGameDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index);

		void SetEditorPostProcessIndices(const PostProcessUnorderedAccessIndices& values);

		void SetGamePostProcessIndices(const PostProcessUnorderedAccessIndices& values);

		void SetOITHeadPointerIndex(Uint index);

		void SetOITFragmentBufferIndex(Uint index);

		void SetOITCounterIndex(Uint index);

		void SetGBuffer0UnorderedAccessViewIndex(Uint index);

		void SetGBuffer1UnorderedAccessViewIndex(Uint index);

		void SetGBufferVelocityUnorderedAccessViewIndex(Uint index);

		void SetGBuffer3UnorderedAccessViewIndex(Uint index);

		void SetMaterialSortBucketIndex(Uint index);

		void SetMaterialSortedPixelListIndex(Uint index);

		void SetShadowRawVisibilityUnorderedAccessViewIndex(Uint index);

		void SetAmbientOcclusionRawUnorderedAccessViewIndex(Uint index);

		void SetSubsurfaceScatteringTransmittanceUnorderedAccessViewIndex(Uint index);

		void SetReflectionOutputUnorderedAccessViewIndex(Uint index);

		void SetReflectionConfidenceUnorderedAccessViewIndex(Uint index);

		void SetRefractionOutputUnorderedAccessViewIndex(Uint index);

		void SetGlobalIlluminationOutputUnorderedAccessViewIndex(Uint index);

		void SetGlobalIlluminationConfidenceUnorderedAccessViewIndex(Uint index);

		void SetCloudOutputUnorderedAccessViewIndex(Uint index);

		void SetCloudShapeNoiseUnorderedAccessViewIndex(Uint index);

		void SetCloudDetailNoiseUnorderedAccessViewIndex(Uint index);

		void SetStarOutputUnorderedAccessViewIndex(Uint index);

		void SetRainParticleUnorderedAccessViewIndex(Uint index);

		void SetSnowParticleUnorderedAccessViewIndex(Uint index);

		void SetVolumetricLightDensityUnorderedAccessViewIndex(Uint index);

		void SetVolumetricLightIntegrationUnorderedAccessViewIndex(Uint index);

	private:
		UnorderedAccessIndices editorIndices_{};
		UnorderedAccessIndices gameIndices_{};
		UnorderedAccessIndices canvasIndices_{};

		ResourcePtr<ConstantBuffer<UnorderedAccessIndices>> editorBuffer_;
		ResourcePtr<ConstantBuffer<UnorderedAccessIndices>> gameBuffer_;
		ResourcePtr<ConstantBuffer<UnorderedAccessIndices>> canvasBuffer_;
	};
}
