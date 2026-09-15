#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	ConstantIndicesSystem::ConstantIndicesSystem(ID3D12Device* device, BindlessHeap* heap)
	{
		editorConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
		gameConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
		canvasConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, heap);
	}

	void ConstantIndicesSystem::UploadEditor()
	{
		editorConstantIndicesBuffer_->Update(editorConstantIndices_);
	}

	void ConstantIndicesSystem::UploadGame()
	{
		gameConstantIndicesBuffer_->Update(gameConstantIndices_);
	}

	void ConstantIndicesSystem::UploadCanvas()
	{
		canvasConstantIndicesBuffer_->Update(canvasConstantIndices_);
	}

	D3D12_GPU_VIRTUAL_ADDRESS ConstantIndicesSystem::EditorConstantAddress()const
	{
		return editorConstantIndicesBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS ConstantIndicesSystem::GameConstantAddress()const
	{
		return gameConstantIndicesBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS ConstantIndicesSystem::CanvasConstantAddress()const
	{
		return canvasConstantIndicesBuffer_->Address();
	}

	void ConstantIndicesSystem::SetEditorSceneIndex(Uint index)
	{
		editorConstantIndices_.sceneIndex_ = index;
	}

	void ConstantIndicesSystem::SetGameSceneIndex(Uint index)
	{
		gameConstantIndices_.sceneIndex_ = index;
	}

	void ConstantIndicesSystem::SetCanvasSceneIndex(Uint index)
	{
		canvasConstantIndices_.sceneIndex_ = index;
	}

	void ConstantIndicesSystem::SetLightIndex(Uint index)
	{
		editorConstantIndices_.lightIndex_ = index;
		gameConstantIndices_.lightIndex_ = index;
		canvasConstantIndices_.lightIndex_ = index;
	}

	void ConstantIndicesSystem::SetClusterAssignIndex(Uint index)
	{
		editorConstantIndices_.clusterAssignIndex_ = index;
		gameConstantIndices_.clusterAssignIndex_ = index;
		canvasConstantIndices_.clusterAssignIndex_ = index;
	}

	void ConstantIndicesSystem::SetWeatherIndex(Uint index)
	{
		editorConstantIndices_.weatherIndex_ = index;
		gameConstantIndices_.weatherIndex_ = index;
		canvasConstantIndices_.weatherIndex_ = index;
	}

	void ConstantIndicesSystem::SetDirectionalLightIndex(Uint index)
	{
		editorConstantIndices_.directionalLightIndex_ = index;
		gameConstantIndices_.directionalLightIndex_ = index;
		canvasConstantIndices_.directionalLightIndex_ = index;
	}

	void ConstantIndicesSystem::SetSkyIndex(Uint index)
	{
		editorConstantIndices_.skyIndex_ = index;
		gameConstantIndices_.skyIndex_ = index;
		canvasConstantIndices_.skyIndex_ = index;
	}

	void ConstantIndicesSystem::SetOitIndex(Uint index)
	{
		editorConstantIndices_.oitIndex_ = index;
		gameConstantIndices_.oitIndex_ = index;
		canvasConstantIndices_.oitIndex_ = index;
	}

	void ConstantIndicesSystem::SetEditorPostProcessIndex(Uint index)
	{
		editorConstantIndices_.postProcessIndex_ = index;
	}

	void ConstantIndicesSystem::SetGamePostProcessIndex(Uint index)
	{
		gameConstantIndices_.postProcessIndex_ = index;
	}

	void ConstantIndicesSystem::SetModelFurIndex(Uint index)
	{
		editorConstantIndices_.furIndex_ = index;
		gameConstantIndices_.furIndex_ = index;
		canvasConstantIndices_.furIndex_ = index;
	}

	void ConstantIndicesSystem::SetShadowRayConstantIndex(Uint index)
	{
		editorConstantIndices_.shadowIndex_ = index;
		gameConstantIndices_.shadowIndex_ = index;
		canvasConstantIndices_.shadowIndex_ = index;
	}

	void ConstantIndicesSystem::SetAmbientOcclusionRayConstantIndex(Uint index)
	{
		editorConstantIndices_.ambientOcclusionIndex_ = index;
		gameConstantIndices_.ambientOcclusionIndex_ = index;
		canvasConstantIndices_.ambientOcclusionIndex_ = index;
	}

	void ConstantIndicesSystem::SetSubsurfaceScatteringRayConstantIndex(Uint index)
	{
		editorConstantIndices_.subsurfaceScatteringIndex_ = index;
		gameConstantIndices_.subsurfaceScatteringIndex_ = index;
		canvasConstantIndices_.subsurfaceScatteringIndex_ = index;
	}

	void ConstantIndicesSystem::SetReflectionRayConstantIndex(Uint index)
	{
		editorConstantIndices_.reflectionIndex_ = index;
		gameConstantIndices_.reflectionIndex_ = index;
		canvasConstantIndices_.reflectionIndex_ = index;
	}

	void ConstantIndicesSystem::SetRefractionRayConstantIndex(Uint index)
	{
		editorConstantIndices_.refractionIndex_ = index;
		gameConstantIndices_.refractionIndex_ = index;
		canvasConstantIndices_.refractionIndex_ = index;
	}

	void ConstantIndicesSystem::SetGlobalIlluminationRayConstantIndex(Uint index)
	{
		editorConstantIndices_.globalIlluminationIndex_ = index;
		gameConstantIndices_.globalIlluminationIndex_ = index;
		canvasConstantIndices_.globalIlluminationIndex_ = index;
	}

	void ConstantIndicesSystem::SetCloudRayConstantIndex(Uint index)
	{
		editorConstantIndices_.cloudIndex_ = index;
		gameConstantIndices_.cloudIndex_ = index;
		canvasConstantIndices_.cloudIndex_ = index;
	}

	void ConstantIndicesSystem::SetStarRayConstantIndex(Uint index)
	{
		editorConstantIndices_.starIndex_ = index;
		gameConstantIndices_.starIndex_ = index;
		canvasConstantIndices_.starIndex_ = index;
	}

	void ConstantIndicesSystem::SetWeatherParticleRayConstantIndex(Uint index)
	{
		editorConstantIndices_.weatherParticleIndex_ = index;
		gameConstantIndices_.weatherParticleIndex_ = index;
		canvasConstantIndices_.weatherParticleIndex_ = index;
	}

	void ConstantIndicesSystem::SetVolumetricLightRayConstantIndex(Uint index)
	{
		editorConstantIndices_.volumetricLightIndex_ = index;
		gameConstantIndices_.volumetricLightIndex_ = index;
		canvasConstantIndices_.volumetricLightIndex_ = index;
	}

	void ConstantIndicesSystem::SetColliderIndex(Uint index)
	{
		editorConstantIndices_.colliderIndex_ = index;
		gameConstantIndices_.colliderIndex_ = index;
		canvasConstantIndices_.colliderIndex_ = index;
	}

	ShaderResourceIndicesSystem::ShaderResourceIndicesSystem(ID3D12Device* device, BindlessHeap* heap)
	{
		editorBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, heap);
		gameBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, heap);
		canvasBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, heap);
	}

	void ShaderResourceIndicesSystem::UploadEditor()
	{
		editorBuffer_->Update(editorIndices_);
	}

	void ShaderResourceIndicesSystem::UploadGame()
	{
		gameBuffer_->Update(gameIndices_);
	}

	void ShaderResourceIndicesSystem::UploadCanvas()
	{
		canvasBuffer_->Update(canvasIndices_);
	}

	D3D12_GPU_VIRTUAL_ADDRESS ShaderResourceIndicesSystem::EditorAddress()const
	{
		return editorBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS ShaderResourceIndicesSystem::GameAddress()const
	{
		return gameBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS ShaderResourceIndicesSystem::CanvasAddress()const
	{
		return canvasBuffer_->Address();
	}

	void ShaderResourceIndicesSystem::SetLightIndices(const LightShaderResourceIndices& values)
	{
		editorIndices_.light_ = values;
		gameIndices_.light_ = values;
		canvasIndices_.light_ = values;
	}

	void ShaderResourceIndicesSystem::SetClusterAssignIndices(const ClusterAssignShaderResourceIndices& values)
	{
		editorIndices_.clusterAssign_ = values;
		gameIndices_.clusterAssign_ = values;
		canvasIndices_.clusterAssign_ = values;
	}

	void ShaderResourceIndicesSystem::SetEditorShadowAccumulationIndices(const ShadowAccumulationShaderResourceIndices& values)
	{
		editorIndices_.shadowAccumulation_ = values;
		canvasIndices_.shadowAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetGameShadowAccumulationIndices(const ShadowAccumulationShaderResourceIndices& values)
	{
		gameIndices_.shadowAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetEditorAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationShaderResourceIndices& values)
	{
		editorIndices_.ambientOcclusionAccumulation_ = values;
		canvasIndices_.ambientOcclusionAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetGameAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationShaderResourceIndices& values)
	{
		gameIndices_.ambientOcclusionAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetEditorGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationShaderResourceIndices& values)
	{
		editorIndices_.globalIlluminationAccumulation_ = values;
		canvasIndices_.globalIlluminationAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetGameGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationShaderResourceIndices& values)
	{
		gameIndices_.globalIlluminationAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetEditorReflectionAccumulationIndices(const ReflectionAccumulationShaderResourceIndices& values)
	{
		editorIndices_.reflectionAccumulation_ = values;
		canvasIndices_.reflectionAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetGameReflectionAccumulationIndices(const ReflectionAccumulationShaderResourceIndices& values)
	{
		gameIndices_.reflectionAccumulation_ = values;
	}

	void ShaderResourceIndicesSystem::SetEditorPostProcessIndices(const PostProcessShaderResourceIndices& values)
	{
		editorIndices_.postProcess_ = values;
	}

	void ShaderResourceIndicesSystem::SetGamePostProcessIndices(const PostProcessShaderResourceIndices& values)
	{
		gameIndices_.postProcess_ = values;
	}

	void ShaderResourceIndicesSystem::SetUIColorAlphaIndex(Uint index)
	{
		editorIndices_.hud_.uiColorAlphaIndex_ = index;
		gameIndices_.hud_.uiColorAlphaIndex_ = index;
		canvasIndices_.hud_.uiColorAlphaIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetImageSpriteIndex(Uint index)
	{
		editorIndices_.image_.spriteIndex_ = index;
		gameIndices_.image_.spriteIndex_ = index;
		canvasIndices_.image_.spriteIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetImageBillboardIndex(Uint index)
	{
		editorIndices_.image_.billboardIndex_ = index;
		gameIndices_.image_.billboardIndex_ = index;
		canvasIndices_.image_.billboardIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetFontSpriteIndex(Uint index)
	{
		editorIndices_.font_.spriteIndex_ = index;
		gameIndices_.font_.spriteIndex_ = index;
		canvasIndices_.font_.spriteIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetFontBillboardIndex(Uint index)
	{
		editorIndices_.font_.billboardIndex_ = index;
		gameIndices_.font_.billboardIndex_ = index;
		canvasIndices_.font_.billboardIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetMovieSpriteIndex(Uint index)
	{
		editorIndices_.movie_.spriteIndex_ = index;
		gameIndices_.movie_.spriteIndex_ = index;
		canvasIndices_.movie_.spriteIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetMovieBillboardIndex(Uint index)
	{
		editorIndices_.movie_.billboardIndex_ = index;
		gameIndices_.movie_.billboardIndex_ = index;
		canvasIndices_.movie_.billboardIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetMovieFullscreenIndex(Uint index)
	{
		editorIndices_.movie_.fullscreenIndex_ = index;
		gameIndices_.movie_.fullscreenIndex_ = index;
		canvasIndices_.movie_.fullscreenIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetModelInstanceIndex(Uint index)
	{
		editorIndices_.model_.instanceIndex_ = index;
		gameIndices_.model_.instanceIndex_ = index;
		canvasIndices_.model_.instanceIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetModelBoneMatrixIndex(Uint index)
	{
		editorIndices_.model_.boneMatrixIndex_ = index;
		gameIndices_.model_.boneMatrixIndex_ = index;
		canvasIndices_.model_.boneMatrixIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetModelPreviousBoneMatrixIndex(Uint index)
	{
		editorIndices_.model_.previousBoneMatrixIndex_ = index;
		gameIndices_.model_.previousBoneMatrixIndex_ = index;
		canvasIndices_.model_.previousBoneMatrixIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetModelMorphWeightIndex(Uint index)
	{
		editorIndices_.model_.morphWeightIndex_ = index;
		gameIndices_.model_.morphWeightIndex_ = index;
		canvasIndices_.model_.morphWeightIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetModelPreviousMorphWeightIndex(Uint index)
	{
		editorIndices_.model_.previousMorphWeightIndex_ = index;
		gameIndices_.model_.previousMorphWeightIndex_ = index;
		canvasIndices_.model_.previousMorphWeightIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetHiZIndex(Uint index)
	{
		editorIndices_.model_.hiZIndex_ = index;
		gameIndices_.model_.hiZIndex_ = index;
		canvasIndices_.model_.hiZIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSelectionMaskIndex(Uint index)
	{
		editorIndices_.model_.selectionMaskIndex_ = index;
		gameIndices_.model_.selectionMaskIndex_ = index;
		canvasIndices_.model_.selectionMaskIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBuffer0Index(Uint index)
	{
		editorIndices_.geometryBuffer_.index0_ = index;
		gameIndices_.geometryBuffer_.index0_ = index;
		canvasIndices_.geometryBuffer_.index0_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBuffer1Index(Uint index)
	{
		editorIndices_.geometryBuffer_.index1_ = index;
		gameIndices_.geometryBuffer_.index1_ = index;
		canvasIndices_.geometryBuffer_.index1_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBuffer2Index(Uint index)
	{
		editorIndices_.geometryBuffer_.index2_ = index;
		gameIndices_.geometryBuffer_.index2_ = index;
		canvasIndices_.geometryBuffer_.index2_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBuffer3Index(Uint index)
	{
		editorIndices_.geometryBuffer_.index3_ = index;
		gameIndices_.geometryBuffer_.index3_ = index;
		canvasIndices_.geometryBuffer_.index3_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBuffer4Index(Uint index)
	{
		editorIndices_.geometryBuffer_.index4_ = index;
		gameIndices_.geometryBuffer_.index4_ = index;
		canvasIndices_.geometryBuffer_.index4_ = index;
	}

	void ShaderResourceIndicesSystem::SetGBufferDepthIndex(Uint index)
	{
		editorIndices_.geometryBuffer_.depthIndex_ = index;
		gameIndices_.geometryBuffer_.depthIndex_ = index;
		canvasIndices_.geometryBuffer_.depthIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSkyEnvironmentCubeIndex(Uint index)
	{
		editorIndices_.sky_.environmentCubeIndex_ = index;
		gameIndices_.sky_.environmentCubeIndex_ = index;
		canvasIndices_.sky_.environmentCubeIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSkyDiffuseIrradianceIndex(Uint index)
	{
		editorIndices_.sky_.diffuseIrradianceIndex_ = index;
		gameIndices_.sky_.diffuseIrradianceIndex_ = index;
		canvasIndices_.sky_.diffuseIrradianceIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSkySpecularPrefilteredIndex(Uint index)
	{
		editorIndices_.sky_.specularPrefilteredIndex_ = index;
		gameIndices_.sky_.specularPrefilteredIndex_ = index;
		canvasIndices_.sky_.specularPrefilteredIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSkyBrdfLutIndex(Uint index)
	{
		editorIndices_.sky_.brdfLutIndex_ = index;
		gameIndices_.sky_.brdfLutIndex_ = index;
		canvasIndices_.sky_.brdfLutIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetTLASIndex(Uint index)
	{
		editorIndices_.raytracing_.tlasIndex_ = index;
		gameIndices_.raytracing_.tlasIndex_ = index;
		canvasIndices_.raytracing_.tlasIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetReflectionInstanceDataIndex(Uint index)
	{
		editorIndices_.raytracing_.instanceDataIndex_ = index;
		gameIndices_.raytracing_.instanceDataIndex_ = index;
		canvasIndices_.raytracing_.instanceDataIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetShadowRawVisibilityShaderResourceViewIndex(Uint index)
	{
		editorIndices_.shadow_.rawVisibilityIndex_ = index;
		gameIndices_.shadow_.rawVisibilityIndex_ = index;
		canvasIndices_.shadow_.rawVisibilityIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetAmbientOcclusionRawShaderResourceViewIndex(Uint index)
	{
		editorIndices_.ambientOcclusion_.rawIndex_ = index;
		gameIndices_.ambientOcclusion_.rawIndex_ = index;
		canvasIndices_.ambientOcclusion_.rawIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSubsurfaceScatteringTransmittanceShaderResourceViewIndex(Uint index)
	{
		editorIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
		gameIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
		canvasIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetReflectionOutputShaderResourceViewIndex(Uint index)
	{
		editorIndices_.reflection_.outputIndex_ = index;
		gameIndices_.reflection_.outputIndex_ = index;
		canvasIndices_.reflection_.outputIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetReflectionConfidenceShaderResourceViewIndex(Uint index)
	{
		editorIndices_.reflection_.confidenceIndex_ = index;
		gameIndices_.reflection_.confidenceIndex_ = index;
		canvasIndices_.reflection_.confidenceIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetRefractionOutputShaderResourceViewIndex(Uint index)
	{
		editorIndices_.refraction_.outputIndex_ = index;
		gameIndices_.refraction_.outputIndex_ = index;
		canvasIndices_.refraction_.outputIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetGlobalIlluminationOutputShaderResourceViewIndex(Uint index)
	{
		editorIndices_.globalIllumination_.outputIndex_ = index;
		gameIndices_.globalIllumination_.outputIndex_ = index;
		canvasIndices_.globalIllumination_.outputIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetGlobalIlluminationConfidenceShaderResourceViewIndex(Uint index)
	{
		editorIndices_.globalIllumination_.confidenceIndex_ = index;
		gameIndices_.globalIllumination_.confidenceIndex_ = index;
		canvasIndices_.globalIllumination_.confidenceIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetCloudOutputShaderResourceViewIndex(Uint index)
	{
		editorIndices_.cloud_.outputIndex_ = index;
		gameIndices_.cloud_.outputIndex_ = index;
		canvasIndices_.cloud_.outputIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetCloudShapeNoiseShaderResourceViewIndex(Uint index)
	{
		editorIndices_.cloud_.shapeNoiseIndex_ = index;
		gameIndices_.cloud_.shapeNoiseIndex_ = index;
		canvasIndices_.cloud_.shapeNoiseIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetCloudDetailNoiseShaderResourceViewIndex(Uint index)
	{
		editorIndices_.cloud_.detailNoiseIndex_ = index;
		gameIndices_.cloud_.detailNoiseIndex_ = index;
		canvasIndices_.cloud_.detailNoiseIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetStarOutputShaderResourceViewIndex(Uint index)
	{
		editorIndices_.star_.outputIndex_ = index;
		gameIndices_.star_.outputIndex_ = index;
		canvasIndices_.star_.outputIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetRainParticleShaderResourceViewIndex(Uint index)
	{
		editorIndices_.weatherParticle_.rainParticleIndex_ = index;
		gameIndices_.weatherParticle_.rainParticleIndex_ = index;
		canvasIndices_.weatherParticle_.rainParticleIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetSnowParticleShaderResourceViewIndex(Uint index)
	{
		editorIndices_.weatherParticle_.snowParticleIndex_ = index;
		gameIndices_.weatherParticle_.snowParticleIndex_ = index;
		canvasIndices_.weatherParticle_.snowParticleIndex_ = index;
	}

	void ShaderResourceIndicesSystem::SetVolumetricLightIntegrationShaderResourceViewIndex(Uint index)
	{
		editorIndices_.volumetricLight_.integrationIndex_ = index;
		gameIndices_.volumetricLight_.integrationIndex_ = index;
		canvasIndices_.volumetricLight_.integrationIndex_ = index;
	}

	UnorderedAccessIndicesSystem::UnorderedAccessIndicesSystem(ID3D12Device* device, BindlessHeap* heap)
	{
		editorBuffer_ = MakePtr<ConstantBuffer<UnorderedAccessIndices>>(device, heap);
		gameBuffer_ = MakePtr<ConstantBuffer<UnorderedAccessIndices>>(device, heap);
		canvasBuffer_ = MakePtr<ConstantBuffer<UnorderedAccessIndices>>(device, heap);
	}

	void UnorderedAccessIndicesSystem::UploadEditor()
	{
		editorBuffer_->Update(editorIndices_);
	}

	void UnorderedAccessIndicesSystem::UploadGame()
	{
		gameBuffer_->Update(gameIndices_);
	}

	void UnorderedAccessIndicesSystem::UploadCanvas()
	{
		canvasBuffer_->Update(canvasIndices_);
	}

	D3D12_GPU_VIRTUAL_ADDRESS UnorderedAccessIndicesSystem::EditorAddress()const
	{
		return editorBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS UnorderedAccessIndicesSystem::GameAddress()const
	{
		return gameBuffer_->Address();
	}

	D3D12_GPU_VIRTUAL_ADDRESS UnorderedAccessIndicesSystem::CanvasAddress()const
	{
		return canvasBuffer_->Address();
	}

	void UnorderedAccessIndicesSystem::SetClusterAssignIndices(const ClusterAssignUnorderedAccessIndices& values)
	{
		editorIndices_.clusterAssign_ = values;
		gameIndices_.clusterAssign_ = values;
		canvasIndices_.clusterAssign_ = values;
	}

	void UnorderedAccessIndicesSystem::SetEditorShadowAccumulationIndices(const ShadowAccumulationUnorderedAccessIndices& values)
	{
		editorIndices_.shadowAccumulation_ = values;
		canvasIndices_.shadowAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetGameShadowAccumulationIndices(const ShadowAccumulationUnorderedAccessIndices& values)
	{
		gameIndices_.shadowAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetEditorAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationUnorderedAccessIndices& values)
	{
		editorIndices_.ambientOcclusionAccumulation_ = values;
		canvasIndices_.ambientOcclusionAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetGameAmbientOcclusionAccumulationIndices(const AmbientOcclusionAccumulationUnorderedAccessIndices& values)
	{
		gameIndices_.ambientOcclusionAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetEditorGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationUnorderedAccessIndices& values)
	{
		editorIndices_.globalIlluminationAccumulation_ = values;
		canvasIndices_.globalIlluminationAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetGameGlobalIlluminationAccumulationIndices(const GlobalIlluminationAccumulationUnorderedAccessIndices& values)
	{
		gameIndices_.globalIlluminationAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetEditorReflectionAccumulationIndices(const ReflectionAccumulationUnorderedAccessIndices& values)
	{
		editorIndices_.reflectionAccumulation_ = values;
		canvasIndices_.reflectionAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetGameReflectionAccumulationIndices(const ReflectionAccumulationUnorderedAccessIndices& values)
	{
		gameIndices_.reflectionAccumulation_ = values;
	}

	void UnorderedAccessIndicesSystem::SetEditorDlssNormalRoughnessUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.dlss_.normalRoughnessIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGameDlssNormalRoughnessUnorderedAccessViewIndex(Uint index)
	{
		gameIndices_.dlss_.normalRoughnessIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetEditorDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.dlss_.specularAlbedoIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGameDlssSpecularAlbedoUnorderedAccessViewIndex(Uint index)
	{
		gameIndices_.dlss_.specularAlbedoIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetEditorDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.dlss_.diffuseAlbedoIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGameDlssDiffuseAlbedoUnorderedAccessViewIndex(Uint index)
	{
		gameIndices_.dlss_.diffuseAlbedoIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetEditorPostProcessIndices(const PostProcessUnorderedAccessIndices& values)
	{
		editorIndices_.postProcess_ = values;
	}

	void UnorderedAccessIndicesSystem::SetGamePostProcessIndices(const PostProcessUnorderedAccessIndices& values)
	{
		gameIndices_.postProcess_ = values;
	}

	void UnorderedAccessIndicesSystem::SetOITHeadPointerIndex(Uint index)
	{
		editorIndices_.oit_.headPointerIndex_ = index;
		gameIndices_.oit_.headPointerIndex_ = index;
		canvasIndices_.oit_.headPointerIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetOITFragmentBufferIndex(Uint index)
	{
		editorIndices_.oit_.fragmentBufferIndex_ = index;
		gameIndices_.oit_.fragmentBufferIndex_ = index;
		canvasIndices_.oit_.fragmentBufferIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetOITCounterIndex(Uint index)
	{
		editorIndices_.oit_.counterIndex_ = index;
		gameIndices_.oit_.counterIndex_ = index;
		canvasIndices_.oit_.counterIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGBuffer0UnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.geometryBuffer_.index0_ = index;
		gameIndices_.geometryBuffer_.index0_ = index;
		canvasIndices_.geometryBuffer_.index0_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGBuffer1UnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.geometryBuffer_.index1_ = index;
		gameIndices_.geometryBuffer_.index1_ = index;
		canvasIndices_.geometryBuffer_.index1_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGBufferVelocityUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.geometryBuffer_.index2_ = index;
		gameIndices_.geometryBuffer_.index2_ = index;
		canvasIndices_.geometryBuffer_.index2_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGBuffer3UnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.geometryBuffer_.index3_ = index;
		gameIndices_.geometryBuffer_.index3_ = index;
		canvasIndices_.geometryBuffer_.index3_ = index;
	}

	void UnorderedAccessIndicesSystem::SetMaterialSortBucketIndex(Uint index)
	{
		editorIndices_.materialSort_.bucketIndex_ = index;
		gameIndices_.materialSort_.bucketIndex_ = index;
		canvasIndices_.materialSort_.bucketIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetMaterialSortedPixelListIndex(Uint index)
	{
		editorIndices_.materialSort_.sortedPixelListIndex_ = index;
		gameIndices_.materialSort_.sortedPixelListIndex_ = index;
		canvasIndices_.materialSort_.sortedPixelListIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetShadowRawVisibilityUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.shadow_.rawVisibilityIndex_ = index;
		gameIndices_.shadow_.rawVisibilityIndex_ = index;
		canvasIndices_.shadow_.rawVisibilityIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetAmbientOcclusionRawUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.ambientOcclusion_.rawIndex_ = index;
		gameIndices_.ambientOcclusion_.rawIndex_ = index;
		canvasIndices_.ambientOcclusion_.rawIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetSubsurfaceScatteringTransmittanceUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
		gameIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
		canvasIndices_.subsurfaceScattering_.transmittanceIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetReflectionOutputUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.reflection_.outputIndex_ = index;
		gameIndices_.reflection_.outputIndex_ = index;
		canvasIndices_.reflection_.outputIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetReflectionConfidenceUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.reflection_.confidenceIndex_ = index;
		gameIndices_.reflection_.confidenceIndex_ = index;
		canvasIndices_.reflection_.confidenceIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetRefractionOutputUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.refraction_.outputIndex_ = index;
		gameIndices_.refraction_.outputIndex_ = index;
		canvasIndices_.refraction_.outputIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGlobalIlluminationOutputUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.globalIllumination_.outputIndex_ = index;
		gameIndices_.globalIllumination_.outputIndex_ = index;
		canvasIndices_.globalIllumination_.outputIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetGlobalIlluminationConfidenceUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.globalIllumination_.confidenceIndex_ = index;
		gameIndices_.globalIllumination_.confidenceIndex_ = index;
		canvasIndices_.globalIllumination_.confidenceIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetCloudOutputUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.cloud_.outputIndex_ = index;
		gameIndices_.cloud_.outputIndex_ = index;
		canvasIndices_.cloud_.outputIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetCloudShapeNoiseUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.cloud_.shapeNoiseIndex_ = index;
		gameIndices_.cloud_.shapeNoiseIndex_ = index;
		canvasIndices_.cloud_.shapeNoiseIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetCloudDetailNoiseUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.cloud_.detailNoiseIndex_ = index;
		gameIndices_.cloud_.detailNoiseIndex_ = index;
		canvasIndices_.cloud_.detailNoiseIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetStarOutputUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.star_.outputIndex_ = index;
		gameIndices_.star_.outputIndex_ = index;
		canvasIndices_.star_.outputIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetRainParticleUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.weatherParticle_.rainParticleIndex_ = index;
		gameIndices_.weatherParticle_.rainParticleIndex_ = index;
		canvasIndices_.weatherParticle_.rainParticleIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetSnowParticleUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.weatherParticle_.snowParticleIndex_ = index;
		gameIndices_.weatherParticle_.snowParticleIndex_ = index;
		canvasIndices_.weatherParticle_.snowParticleIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetVolumetricLightDensityUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.volumetricLight_.densityIndex_ = index;
		gameIndices_.volumetricLight_.densityIndex_ = index;
		canvasIndices_.volumetricLight_.densityIndex_ = index;
	}

	void UnorderedAccessIndicesSystem::SetVolumetricLightIntegrationUnorderedAccessViewIndex(Uint index)
	{
		editorIndices_.volumetricLight_.integrationIndex_ = index;
		gameIndices_.volumetricLight_.integrationIndex_ = index;
		canvasIndices_.volumetricLight_.integrationIndex_ = index;
	}
}
