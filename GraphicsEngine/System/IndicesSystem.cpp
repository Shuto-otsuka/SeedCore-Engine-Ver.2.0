#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	IndicesSystem::IndicesSystem(ID3D12Device* device, BindlessHeap* heap)
	{
		editorConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
		gameConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
		canvasConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
		structuredIndicesBuffer_ = MakePtr<ConstantBuffer<StructuredIndices>>(device, heap);
	}

	void IndicesSystem::UploadEditor()
	{
		editorConstantIndicesBuffer_->Update(editorConstantIndices_);
		structuredIndicesBuffer_->Update(structuredIndices_);
	}

	void IndicesSystem::UploadGame()
	{
		gameConstantIndicesBuffer_->Update(gameConstantIndices_);
		structuredIndicesBuffer_->Update(structuredIndices_);
	}

	void IndicesSystem::UploadCanvas()
	{
		canvasConstantIndicesBuffer_->Update(canvasConstantIndices_);
		structuredIndicesBuffer_->Update(structuredIndices_);
	}

	D3D12_GPU_VIRTUAL_ADDRESS IndicesSystem::EditorConstantAddress()const
	{
		return editorConstantIndicesBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS IndicesSystem::GameConstantAddress()const
	{
		return gameConstantIndicesBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS IndicesSystem::CanvasConstantAddress()const
	{
		return canvasConstantIndicesBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS IndicesSystem::StructuredAddress()const
	{
		return structuredIndicesBuffer_->Address();
	}

	void IndicesSystem::SetEditorSceneIndex(Uint index)
	{
		editorConstantIndices_.sceneIndex_ = index;
	}

	void IndicesSystem::SetGameSceneIndex(Uint index)
	{
		gameConstantIndices_.sceneIndex_ = index;
	}

	void IndicesSystem::SetCanvasSceneIndex(Uint index)
	{
		canvasConstantIndices_.sceneIndex_ = index;
	}

	void IndicesSystem::SetLightIndex(Uint index)
	{
		editorConstantIndices_.lightIndex_ = index;
		gameConstantIndices_.lightIndex_ = index;
		canvasConstantIndices_.lightIndex_ = index;
	}

	void IndicesSystem::SetClusterConstantIndex(Uint index)
	{
		editorConstantIndices_.clusterConstantIndex_ = index;
		gameConstantIndices_.clusterConstantIndex_ = index;
		canvasConstantIndices_.clusterConstantIndex_ = index;
	}

	void IndicesSystem::SetEditorViewMode(Uint mode)
	{
		/// [JP] View Mode はエディタービュー限定。game/canvas は 0(Lit) のまま。
		editorConstantIndices_.viewMode_ = mode;
	}

	void IndicesSystem::SetImageSpriteIndex(Uint index)
	{
		structuredIndices_.sprite_.imageIndex_ = index;
	}

	void IndicesSystem::SetImageBillboardIndex(Uint index)
	{
		structuredIndices_.sprite_.imageBillboardIndex_ = index;
	}

	void IndicesSystem::SetFontSpriteIndex(Uint index)
	{
		structuredIndices_.sprite_.fontIndex_ = index;
	}

	void IndicesSystem::SetFontBillboardIndex(Uint index)
	{
		structuredIndices_.sprite_.fontBillboardIndex_ = index;
	}

	void IndicesSystem::SetModelInstanceIndex(Uint index)
	{
		structuredIndices_.model_.instanceIndex_ = index;
	}

	void IndicesSystem::SetModelBoneMatrixIndex(Uint index)
	{
		structuredIndices_.model_.boneMatrixIndex_ = index;
	}

	void IndicesSystem::SetModelMorphWeightIndex(Uint index)
	{
		structuredIndices_.model_.morphWeightIndex_ = index;
	}

	void IndicesSystem::SetOITHeadPointerIndex(Uint index)
	{
		structuredIndices_.oit_.headPointerIndex_ = index;
	}

	void IndicesSystem::SetOITFragmentBufferIndex(Uint index)
	{
		structuredIndices_.oit_.fragmentBufferIndex_ = index;
	}

	void IndicesSystem::SetOITCounterIndex(Uint index)
	{
		structuredIndices_.oit_.counterIndex_ = index;
	}

	void IndicesSystem::SetOITFragmentCapacity(Uint capacity)
	{
		structuredIndices_.oit_.fragmentCapacity_ = capacity;
	}

	void IndicesSystem::SetHiZIndex(Uint index)
	{
		structuredIndices_.model_.hiZIndex_ = index;
	}

	void IndicesSystem::SetGBuffer0Index(Uint index)
	{
		structuredIndices_.gbuffer_.index0_ = index;
	}

	void IndicesSystem::SetGBuffer1Index(Uint index)
	{
		structuredIndices_.gbuffer_.index1_ = index;
	}

	void IndicesSystem::SetGBuffer2Index(Uint index)
	{
		structuredIndices_.gbuffer_.index2_ = index;
	}

	void IndicesSystem::SetGBuffer3Index(Uint index)
	{
		structuredIndices_.gbuffer_.index3_ = index;
	}

	void IndicesSystem::SetGBuffer4Index(Uint index)
	{
		structuredIndices_.gbuffer_.index4_ = index;
	}

	void IndicesSystem::SetGBufferDepthIndex(Uint index)
	{
		structuredIndices_.gbuffer_.depthIndex_ = index;
	}

	void IndicesSystem::SetGBufferVelocityUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.gbuffer_.velocityUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGBuffer0UnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.gbuffer_.index0UnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGBuffer1UnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.gbuffer_.index1UnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetMaterialSortBucketIndex(Uint index)
	{
		structuredIndices_.materialSort_.bucketIndex_ = index;
	}

	void IndicesSystem::SetMaterialSortedPixelListIndex(Uint index)
	{
		structuredIndices_.materialSort_.sortedPixelListIndex_ = index;
	}

	void IndicesSystem::SetGBuffer3UnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.gbuffer_.index3UnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetSkyEnvironmentCubeIndex(Uint index)
	{
		structuredIndices_.sky_.environmentCubeIndex_ = index;
	}

	void IndicesSystem::SetSkyDiffuseIrradianceIndex(Uint index)
	{
		structuredIndices_.sky_.diffuseIrradianceIndex_ = index;
	}

	void IndicesSystem::SetSkySpecularPrefilteredIndex(Uint index)
	{
		structuredIndices_.sky_.specularPrefilteredIndex_ = index;
	}

	void IndicesSystem::SetSkyBrdfLutIndex(Uint index)
	{
		structuredIndices_.sky_.brdfLutIndex_ = index;
	}

	void IndicesSystem::SetSkyIntensity(Float intensity)
	{
		structuredIndices_.sky_.intensity_ = intensity;
	}

	void IndicesSystem::SetSelectionMaskIndex(Uint index)
	{
		structuredIndices_.model_.selectionMaskIndex_ = index;
	}

	void IndicesSystem::SetTLASIndex(Uint index)
	{
		structuredIndices_.raytracing_.tlasIndex_ = index;
	}

	void IndicesSystem::SetShadowRawVisibilityUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.shadow_.rawVisibilityUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetShadowRawVisibilityShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.shadow_.rawVisibilityShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetShadowRayConstantIndex(Uint index)
	{
		structuredIndices_.shadow_.rayConstantIndex_ = index;
	}

	/**
	* [EN]
	* Registers the editor view's shadow SVGF chain. See the declaration in
	* IndicesSystem.h for why this takes the whole struct.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エディタビューの影 SVGF チェーンを登録する。構造体をまるごと受け取る理由は
	* IndicesSystem.h の宣言側を参照。
	*/
	void IndicesSystem::SetEditorShadowIndices(const ShadowAccumulationIndices& values)
	{
		editorConstantIndices_.shadow_ = values;

		/// [JP] Canvas は影を読まないが、未定義値を残さないためエディタと同値を入れる。
		canvasConstantIndices_.shadow_ = values;
	}

	/**
	* [EN]
	* Registers the game view's shadow SVGF chain.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームビューの影 SVGF チェーンを登録する。
	*/
	void IndicesSystem::SetGameShadowIndices(const ShadowAccumulationIndices& values)
	{
		gameConstantIndices_.shadow_ = values;
	}

	void IndicesSystem::SetAmbientOcclusionRawUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.ambientOcclusion_.rawUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetAmbientOcclusionRawShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.ambientOcclusion_.rawShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetAmbientOcclusionRayConstantIndex(Uint index)
	{
		structuredIndices_.ambientOcclusion_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetEditorAmbientOcclusionIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint opennessShaderResourceViewIndex)
	{
		editorConstantIndices_.ambientOcclusion_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		editorConstantIndices_.ambientOcclusion_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		editorConstantIndices_.ambientOcclusion_.opennessShaderResourceViewIndex_ = opennessShaderResourceViewIndex;

		/// [JP] Canvas はAOを読まないが、未定義値を残さないためエディタと同値を入れる。
		canvasConstantIndices_.ambientOcclusion_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		canvasConstantIndices_.ambientOcclusion_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		canvasConstantIndices_.ambientOcclusion_.opennessShaderResourceViewIndex_ = opennessShaderResourceViewIndex;
	}

	void IndicesSystem::SetGameAmbientOcclusionIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint opennessShaderResourceViewIndex)
	{
		gameConstantIndices_.ambientOcclusion_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		gameConstantIndices_.ambientOcclusion_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		gameConstantIndices_.ambientOcclusion_.opennessShaderResourceViewIndex_ = opennessShaderResourceViewIndex;
	}

	void IndicesSystem::SetEditorGlobalIlluminationAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex)
	{
		editorConstantIndices_.globalIllumination_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		editorConstantIndices_.globalIllumination_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		editorConstantIndices_.globalIllumination_.radianceShaderResourceViewIndex_ = radianceShaderResourceViewIndex;

		/// [JP] Canvas はGIを読まないが、未定義値を残さないためエディタと同値を入れる。
		canvasConstantIndices_.globalIllumination_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		canvasConstantIndices_.globalIllumination_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		canvasConstantIndices_.globalIllumination_.radianceShaderResourceViewIndex_ = radianceShaderResourceViewIndex;
	}

	void IndicesSystem::SetGameGlobalIlluminationAccumulationIndices(Uint historyShaderResourceViewIndex, Uint accumulatedUnorderedAccessViewIndex, Uint radianceShaderResourceViewIndex)
	{
		gameConstantIndices_.globalIllumination_.historyShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		gameConstantIndices_.globalIllumination_.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex;
		gameConstantIndices_.globalIllumination_.radianceShaderResourceViewIndex_ = radianceShaderResourceViewIndex;
	}

	void IndicesSystem::SetEditorGlobalIlluminationReservoirIndices(Uint historyShaderResourceViewIndex, Uint unorderedAccessViewIndex, Uint writeShaderResourceViewIndex)
	{
		editorConstantIndices_.globalIllumination_.reservoirHistoryShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		editorConstantIndices_.globalIllumination_.reservoirUnorderedAccessViewIndex_ = unorderedAccessViewIndex;
		editorConstantIndices_.globalIllumination_.reservoirWriteShaderResourceViewIndex_ = writeShaderResourceViewIndex;

		/// [JP] Canvas はGIを読まないが、未定義値を残さないためエディタと同値を入れる。
		canvasConstantIndices_.globalIllumination_.reservoirHistoryShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		canvasConstantIndices_.globalIllumination_.reservoirUnorderedAccessViewIndex_ = unorderedAccessViewIndex;
		canvasConstantIndices_.globalIllumination_.reservoirWriteShaderResourceViewIndex_ = writeShaderResourceViewIndex;
	}

	void IndicesSystem::SetGameGlobalIlluminationReservoirIndices(Uint historyShaderResourceViewIndex, Uint unorderedAccessViewIndex, Uint writeShaderResourceViewIndex)
	{
		gameConstantIndices_.globalIllumination_.reservoirHistoryShaderResourceViewIndex_ = historyShaderResourceViewIndex;
		gameConstantIndices_.globalIllumination_.reservoirUnorderedAccessViewIndex_ = unorderedAccessViewIndex;
		gameConstantIndices_.globalIllumination_.reservoirWriteShaderResourceViewIndex_ = writeShaderResourceViewIndex;
	}

	void IndicesSystem::SetEditorGlobalIlluminationAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex)
	{
		editorConstantIndices_.globalIllumination_.atrousScratch0ShaderResourceViewIndex_ = scratch0ShaderResourceViewIndex;
		editorConstantIndices_.globalIllumination_.atrousScratch0UnorderedAccessViewIndex_ = scratch0UnorderedAccessViewIndex;
		editorConstantIndices_.globalIllumination_.atrousScratch1ShaderResourceViewIndex_ = scratch1ShaderResourceViewIndex;
		editorConstantIndices_.globalIllumination_.atrousScratch1UnorderedAccessViewIndex_ = scratch1UnorderedAccessViewIndex;
	}

	void IndicesSystem::SetGameGlobalIlluminationAtrousScratchIndices(Uint scratch0ShaderResourceViewIndex, Uint scratch0UnorderedAccessViewIndex, Uint scratch1ShaderResourceViewIndex, Uint scratch1UnorderedAccessViewIndex)
	{
		gameConstantIndices_.globalIllumination_.atrousScratch0ShaderResourceViewIndex_ = scratch0ShaderResourceViewIndex;
		gameConstantIndices_.globalIllumination_.atrousScratch0UnorderedAccessViewIndex_ = scratch0UnorderedAccessViewIndex;
		gameConstantIndices_.globalIllumination_.atrousScratch1ShaderResourceViewIndex_ = scratch1ShaderResourceViewIndex;
		gameConstantIndices_.globalIllumination_.atrousScratch1UnorderedAccessViewIndex_ = scratch1UnorderedAccessViewIndex;
	}

	/**
	* [EN]
	* Registers the editor view's reflection SVGF chain. See the declaration in
	* IndicesSystem.h for why this takes the whole struct.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エディタビューの反射 SVGF チェーンを登録する。構造体をまるごと受け取る
	* 理由は IndicesSystem.h の宣言側を参照。
	*/
	void IndicesSystem::SetEditorReflectionAccumulationIndices(const ReflectionAccumulationIndices& values)
	{
		editorConstantIndices_.reflection_ = values;

		/// [JP] Canvas は反射を読まないが、未定義値を残さないためエディタと同値を入れる。
		canvasConstantIndices_.reflection_ = values;
	}

	/**
	* [EN]
	* Registers the game view's reflection SVGF chain.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲームビューの反射 SVGF チェーンを登録する。
	*/
	void IndicesSystem::SetGameReflectionAccumulationIndices(const ReflectionAccumulationIndices& values)
	{
		gameConstantIndices_.reflection_ = values;
	}

	void IndicesSystem::SetEditorPostProcessIndices(const PostProcessIndices& values)
	{
		editorConstantIndices_.postProcess_ = values;
	}

	void IndicesSystem::SetGamePostProcessIndices(const PostProcessIndices& values)
	{
		gameConstantIndices_.postProcess_ = values;
	}

	void IndicesSystem::SetSubsurfaceScatteringTransmittanceUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.subsurfaceScattering_.transmittanceUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetSubsurfaceScatteringTransmittanceShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.subsurfaceScattering_.transmittanceShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetSubsurfaceScatteringRayConstantIndex(Uint index)
	{
		structuredIndices_.subsurfaceScattering_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetReflectionOutputUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.reflection_.outputUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetReflectionOutputShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.reflection_.outputShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetReflectionConfidenceUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.reflection_.confidenceUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetReflectionConfidenceShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.reflection_.confidenceShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetReflectionRayConstantIndex(Uint index)
	{
		structuredIndices_.reflection_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetReflectionInstanceDataIndex(Uint index)
	{
		structuredIndices_.raytracing_.instanceDataIndex_ = index;
	}

	void IndicesSystem::SetRefractionOutputUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.refraction_.outputUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetRefractionOutputShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.refraction_.outputShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetRefractionRayConstantIndex(Uint index)
	{
		structuredIndices_.refraction_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetGlobalIlluminationOutputUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.globalIllumination_.outputUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGlobalIlluminationOutputShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.globalIllumination_.outputShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetGlobalIlluminationConfidenceUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.globalIllumination_.confidenceUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGlobalIlluminationConfidenceShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.globalIllumination_.confidenceShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetGlobalIlluminationRayConstantIndex(Uint index)
	{
		structuredIndices_.globalIllumination_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetCloudOutputUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.cloud_.outputUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetCloudOutputShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.cloud_.outputShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetCloudRayConstantIndex(Uint index)
	{
		structuredIndices_.cloud_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetCloudShapeNoiseUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.cloud_.shapeNoiseUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetCloudShapeNoiseShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.cloud_.shapeNoiseShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetCloudDetailNoiseUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.cloud_.detailNoiseUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetCloudDetailNoiseShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.cloud_.detailNoiseShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetStarOutputUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.star_.outputUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetStarOutputShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.star_.outputShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetStarRayConstantIndex(Uint index)
	{
		structuredIndices_.star_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetRainParticleUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.weatherParticle_.rainParticleUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetRainParticleShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.weatherParticle_.rainParticleShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetSnowParticleUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.weatherParticle_.snowParticleUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetSnowParticleShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.weatherParticle_.snowParticleShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetWeatherParticleRayConstantIndex(Uint index)
	{
		structuredIndices_.weatherParticle_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetVolumetricLightDensityUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.volumetricLight_.densityUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetVolumetricLightScatteringUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.volumetricLight_.scatteringUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetVolumetricLightIntegrationUnorderedAccessViewIndex(Uint index)
	{
		structuredIndices_.volumetricLight_.integrationUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetVolumetricLightIntegrationShaderResourceViewIndex(Uint index)
	{
		structuredIndices_.volumetricLight_.integrationShaderResourceViewIndex_ = index;
	}

	void IndicesSystem::SetVolumetricLightRayConstantIndex(Uint index)
	{
		structuredIndices_.volumetricLight_.rayConstantIndex_ = index;
	}

	void IndicesSystem::SetMovieSpriteIndex(Uint index)
	{
		structuredIndices_.movie_.spriteIndex_ = index;
	}

	void IndicesSystem::SetMovieBillboardIndex(Uint index)
	{
		structuredIndices_.movie_.billboardIndex_ = index;
	}

	void IndicesSystem::SetMovieFullscreenIndex(Uint index)
	{
		structuredIndices_.movie_.fullscreenIndex_ = index;
	}

	void IndicesSystem::SetEditorDlssNormalRoughnessUnorderedAccessViewIndex(Uint index)
	{
		editorConstantIndices_.dlss_.normalRoughnessUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGameDlssNormalRoughnessUnorderedAccessViewIndex(Uint index)
	{
		gameConstantIndices_.dlss_.normalRoughnessUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetEditorDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index)
	{
		editorConstantIndices_.dlss_.specularAlbedoUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGameDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index)
	{
		gameConstantIndices_.dlss_.specularAlbedoUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetEditorDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index)
	{
		editorConstantIndices_.dlss_.diffuseAlbedoUnorderedAccessViewIndex_ = index;
	}

	void IndicesSystem::SetGameDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index)
	{
		gameConstantIndices_.dlss_.diffuseAlbedoUnorderedAccessViewIndex_ = index;
	}
}
