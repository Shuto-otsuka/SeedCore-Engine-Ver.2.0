#include <GraphicsEngine/Renderer/SkyRenderer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/Sky/Skymap.h>
#include <GraphicsEngine/Sky/SkymapResource.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void SkyRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject)
	{
		device_ = device;
		bindlessHeap_ = bindlessHeap;

		rootSignatureHandle_ = rootSignature.GetOrCreate(device);
		rootSignature_ = rootSignature.Get(rootSignatureHandle_);

		equirectToCubePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/EquirectToCubeCS.hlsl"));
		diffuseIrradiancePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/DiffuseIrradianceCS.hlsl"));
		specularPrefilterPipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/SpecularPrefilterCS.hlsl"));
		brdfLookupTablePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/BrdfLutCS.hlsl"));
		proceduralSkyToCubePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/ProceduralSkyToCubeCS.hlsl"));

		constantBuffers_.reserve(maxGenerateDispatches_);
		for (Uint dispatchIndex = 0; dispatchIndex < maxGenerateDispatches_; dispatchIndex++)
		{
			constantBuffers_.push_back(MakePtr<ConstantBuffer<SkyGenerateConstant>>(device, bindlessHeap));
		}

		CreateBrdfLookupTable(device, bindlessHeap);
		CreateIblCubes(device, bindlessHeap);
		CreateCaptureResources(device);
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> SkyRenderer::CreateComputePipeline(ID3D12Device* device, ShaderCache& shaderCache, PipelineStateObject& pipelineStateObject, const String& filePath)
	{
		Handle<ComputeShader> shaderHandle = shaderCache.GetOrCreateComputeShader(filePath);

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_->Get();
		key.computeShader_ = shaderCache.GetComputeShader(shaderHandle)->Bytecode();

		auto pipelineHandle = pipelineStateObject.GetOrCreate(device, key);
		return pipelineStateObject.Get(pipelineHandle);
	}

	void SkyRenderer::CreateBrdfLookupTable(ID3D12Device* device, BindlessHeap* bindlessHeap)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = brdfLookupTableSize_;
		resourceDesc.Height = brdfLookupTableSize_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = brdfLookupTableFormat_;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&brdfLookupTableResource_));
		SC_HR_CHECK(hr, "BRDF ルックアップテーブルの生成に失敗しました");

		brdfLookupTableShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = brdfLookupTableFormat_;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(brdfLookupTableResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(brdfLookupTableShaderResourceViewIndex_));

		brdfLookupTableUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = brdfLookupTableFormat_;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		unorderedAccessViewDesc.Texture2D.MipSlice = 0;
		unorderedAccessViewDesc.Texture2D.PlaneSlice = 0;
		device->CreateUnorderedAccessView(brdfLookupTableResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(brdfLookupTableUnorderedAccessViewIndex_));
	}

	void SkyRenderer::CreateIblCubes(ID3D12Device* device, BindlessHeap* bindlessHeap)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		CreateCube(device, bindlessHeap, environmentSize_, environmentMipLevels_, nullptr, readState, environmentResource_, environmentShaderResourceViewIndex_, environmentUnorderedAccessViewIndices_);
		CreateCube(device, bindlessHeap, environmentSize_, 1, &dynamicRenderTargetViewHeap_, readState, dynamicResource_, dynamicShaderResourceViewIndex_, &dynamicUnorderedAccessViewIndex_);
		CreateCube(device, bindlessHeap, irradianceSize_, 1, nullptr, readState, irradianceResource_, irradianceShaderResourceViewIndex_, &irradianceUnorderedAccessViewIndex_);
		CreateCube(device, bindlessHeap, prefilterSize_, prefilterMipLevels_, nullptr, readState, prefilterResource_, prefilterShaderResourceViewIndex_, prefilterUnorderedAccessViewIndices_);
	}

	void SkyRenderer::CreateCube(ID3D12Device* device, BindlessHeap* heap, Uint faceSize, Uint mipLevels, DescriptorHeap* renderTargetViewHeap, D3D12_RESOURCE_STATES initialState,
		Microsoft::WRL::ComPtr<ID3D12Resource>& resource, Uint& shaderResourceViewIndex, Uint* unorderedAccessViewIndices)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = faceSize;
		resourceDesc.Height = faceSize;
		resourceDesc.DepthOrArraySize = 6;
		resourceDesc.MipLevels = static_cast<Uint16>(mipLevels);
		resourceDesc.Format = cubeFormat_;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (renderTargetViewHeap)
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr, IID_PPV_ARGS(&resource));
		SC_HR_CHECK(hr, "スカイマップのキューブリソース生成に失敗しました");

		shaderResourceViewIndex = heap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = cubeFormat_;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.TextureCube.MostDetailedMip = 0;
		shaderResourceViewDesc.TextureCube.MipLevels = mipLevels;
		device->CreateShaderResourceView(resource.Get(), &shaderResourceViewDesc, heap->CPUHandle(shaderResourceViewIndex));

		for (Uint mip = 0; mip < mipLevels; mip++)
		{
			unorderedAccessViewIndices[mip] = heap->AllocateIndex();
			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = cubeFormat_;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			unorderedAccessViewDesc.Texture2DArray.MipSlice = mip;
			unorderedAccessViewDesc.Texture2DArray.FirstArraySlice = 0;
			unorderedAccessViewDesc.Texture2DArray.ArraySize = 6;
			unorderedAccessViewDesc.Texture2DArray.PlaneSlice = 0;
			device->CreateUnorderedAccessView(resource.Get(), nullptr, &unorderedAccessViewDesc, heap->CPUHandle(unorderedAccessViewIndices[mip]));
		}

		if (renderTargetViewHeap)
		{
			renderTargetViewHeap->Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, false);
			for (Uint face = 0; face < 6; face++)
			{
				Uint index = renderTargetViewHeap->AllocateIndex();
				D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
				renderTargetViewDesc.Format = cubeFormat_;
				renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				renderTargetViewDesc.Texture2DArray.MipSlice = 0;
				renderTargetViewDesc.Texture2DArray.FirstArraySlice = face;
				renderTargetViewDesc.Texture2DArray.ArraySize = 1;
				renderTargetViewDesc.Texture2DArray.PlaneSlice = 0;
				device->CreateRenderTargetView(resource.Get(), &renderTargetViewDesc, renderTargetViewHeap->CPUHandle(index));
			}
		}
	}

	void SkyRenderer::CreateCaptureResources(ID3D12Device* device)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = environmentSize_;
		resourceDesc.Height = environmentSize_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = captureDepthFormat_;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = captureDepthFormat_;
		clearValue.DepthStencil.Depth = 0.0f;
		clearValue.DepthStencil.Stencil = 0;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&captureDepthResource_));
		SC_HR_CHECK(hr, "スカイマップキャプチャ深度バッファの生成に失敗しました");

		captureDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
		Uint depthIndex = captureDepthStencilViewHeap_.AllocateIndex();
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
		depthStencilViewDesc.Format = captureDepthFormat_;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;
		device->CreateDepthStencilView(captureDepthResource_.Get(), &depthStencilViewDesc, captureDepthStencilViewHeap_.CPUHandle(depthIndex));

		captureSceneConstantBuffer_ = MakePtr<ConstantBuffer<SceneConstantBuffer>>(device, bindlessHeap_);
		captureConstantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap_);
	}

	void SkyRenderer::Gather(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world)
	{
		hasSkymap_ = false;
		sourceEquirectShaderResourceViewIndex_ = invalidIndex_;
		intensity_ = 1.0f;
		captureType_ = SkyLight::CaptureType::Lietime;

		SkymapResource* skymapResource = resourceCache.GetSkymapResource();
		if (!skymapResource)
		{
			return;
		}

		Bool found = false;
		Query<Read<Active>, Read<SkyLight>> query(world);
		query.ForEach([&](const Active& active, const SkyLight& skyLight)
			{
				if (!active.active_ || found)
				{
					return;
				}

				if (!skyLight.useSkymap_)
				{
					return;
				}

				Handle<Skymap> handle = skymapResource->GetHandle(skyLight.skymapID_);
				if (handle.empty())
				{
					return;
				}

				Skymap* skymap = skymapResource->Resolve(loaderSystem, handle);
				if (!skymap || !skymap->Valid())
				{
					return;
				}

				hasSkymap_ = true;
				sourceEquirectShaderResourceViewIndex_ = skymap->EquirectShaderResourceViewIndex();
				intensity_ = skyLight.intensity_;
				captureType_ = skyLight.captureType_;
				captureDuration_ = skyLight.captureDuration_;
				found = true;
			});

		/// [EN] Note: the generated cubes are kept while the sky is toggled off
		///      (nothing overwrites them), so toggling the same skymap back on
		///      resumes instantly without re-seeding the dynamic cube. A change
		///      to a different skymap is detected by its differing source index
		///      in Generate and triggers a regenerate there.
		/// [JP] 空を OFF にしている間も生成済みキューブは保持される（誰も上書き
		///      しない）ので、同じスカイマップに戻せば dynamic を再シードせず即
		///      再開する。別スカイマップへの変更は Generate 側で source index の
		///      差異として検出され、そこで再生成される。
	}

	void SkyRenderer::SetProceduralSky(Bool enabled, Uint32 settingsHash, Uint lightIndex, Float totalTime)
	{
		proceduralSkyEnabled_ = enabled;
		proceduralSkyHash_ = settingsHash;
		proceduralSkyLightIndex_ = lightIndex;
		proceduralSkyTime_ = totalTime;
	}

	void SkyRenderer::SetIndices(IndicesSystem& indicesSystem)
	{
		indicesSystem.SetSkyBrdfLutIndex(brdfLookupTableShaderResourceViewIndex_);

		Bool ready = hasSkymap_ && generatedSourceShaderResourceViewIndex_ != invalidIndex_;
		if (ready)
		{
			indicesSystem.SetSkyEnvironmentCubeIndex(environmentShaderResourceViewIndex_);
			indicesSystem.SetSkyDiffuseIrradianceIndex(irradianceShaderResourceViewIndex_);
			indicesSystem.SetSkySpecularPrefilteredIndex(prefilterShaderResourceViewIndex_);
			indicesSystem.SetSkyIntensity(intensity_);
		}
		else if (!hasSkymap_ && proceduralSkyEnabled_ && proceduralSkyGenerated_)
		{
			/// [EN] Procedural-sky IBL: register irradiance/prefilter only.
			///      The environment cube index deliberately stays 0 so the
			///      background remains the ANALYTIC sky (crisp sun disc, see
			///      DeferredCompositePS.hlsl) while lit surfaces receive the
			///      convolved sky as IBL.
			/// [JP] プロシージャル空の IBL: irradiance/prefilter のみ登録する。
			///      environment キューブのインデックスは意図的に 0 のままに
			///      して、背景は【解析的な】空(シャープな太陽ディスク、
			///      DeferredCompositePS.hlsl 参照)のまま、ライティング面だけが
			///      畳み込み済みの空を IBL として受け取るようにする。
			indicesSystem.SetSkyEnvironmentCubeIndex(0);
			indicesSystem.SetSkyDiffuseIrradianceIndex(irradianceShaderResourceViewIndex_);
			indicesSystem.SetSkySpecularPrefilteredIndex(prefilterShaderResourceViewIndex_);
			indicesSystem.SetSkyIntensity(1.0f);
		}
		else
		{
			indicesSystem.SetSkyEnvironmentCubeIndex(0);
			indicesSystem.SetSkyDiffuseIrradianceIndex(0);
			indicesSystem.SetSkySpecularPrefilteredIndex(0);
			indicesSystem.SetSkyIntensity(1.0f);
		}
	}

	void SkyRenderer::Generate(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress)
	{
		dispatchCursor_ = 0;

		if (!brdfLookupTableGenerated_)
		{
			SkyGenerateConstant data{};
			data.destIndex_ = brdfLookupTableUnorderedAccessViewIndex_;
			data.faceSize_ = brdfLookupTableSize_;
			data.sampleCount_ = brdfSampleCount_;
			Dispatch(cmdList, heap, brdfLookupTablePipeline_.Get(), data, (brdfLookupTableSize_ + 7) / 8, (brdfLookupTableSize_ + 7) / 8, 1);

			UnorderedAccessBarrier(cmdList, brdfLookupTableResource_.Get());
			Transition(cmdList, brdfLookupTableResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			brdfLookupTableGenerated_ = true;
		}

		if (!hasSkymap_ || sourceEquirectShaderResourceViewIndex_ == invalidIndex_)
		{
			/// [JP] スカイマップ無し: プロシージャル空の IBL 経路。設定ハッシュが
			///      変わった時、または定期的(太陽の回転はハッシュに含まれない
			///      ため)に environment キューブへ空を描いて再畳み込みする。
			if (!hasSkymap_ && proceduralSkyEnabled_ && proceduralSkyLightIndex_ != 0)
			{
				++proceduralSkyRefreshCounter_;

				Bool hashChanged = !proceduralSkyGenerated_ || generatedProceduralSkyHash_ != proceduralSkyHash_;
				Bool periodicRefresh = proceduralSkyRefreshCounter_ >= proceduralSkyRefreshInterval_;

				if (hashChanged || periodicRefresh)
				{
					GenerateProceduralEnvironment(cmdList, heap, structuredAddress);
					generatedProceduralSkyHash_ = proceduralSkyHash_;
					proceduralSkyGenerated_ = true;
					proceduralSkyRefreshCounter_ = 0;

					/// [JP] environment キューブをプロシージャル空で上書きした
					///      ので、スカイマップ側の生成済み状態を無効化する
					///      (スカイマップへ戻した時に必ず再生成させる)。
					generatedSourceShaderResourceViewIndex_ = invalidIndex_;
				}
			}
			return;
		}
		if (sourceEquirectShaderResourceViewIndex_ == generatedSourceShaderResourceViewIndex_)
		{
			return;
		}

		GenerateStaticEnvironment(cmdList, heap);
		generatedSourceShaderResourceViewIndex_ = sourceEquirectShaderResourceViewIndex_;

		/// [JP] スカイマップで environment/畳み込みを上書きしたので、
		///      プロシージャル空側の生成済み状態を無効化する。
		proceduralSkyGenerated_ = false;
	}

	void SkyRenderer::GenerateProceduralEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		/// [JP] 解析的な空+太陽を environment の6面へ描く。source_index_ は
		///      このパスでは未使用なので LightConstantData の bindless
		///      インデックスを載せる(ProceduralSkyToCubeCS.hlsl 参照)。
		Transition(cmdList, environmentResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		{
			SkyGenerateConstant data{};
			data.sourceIndex_ = proceduralSkyLightIndex_;
			data.destIndex_ = environmentUnorderedAccessViewIndices_[0];
			data.faceSize_ = environmentSize_;

			/// [JP] roughness_ はこのパスでは prefilter 用として未使用なので、
			///      雲の風スクロール時刻を転用して渡す(ProceduralSkyToCubeCS 参照)。
			data.roughness_ = proceduralSkyTime_;
			Dispatch(cmdList, heap, proceduralSkyToCubePipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 6, structuredAddress);
		}
		UnorderedAccessBarrier(cmdList, environmentResource_.Get());
		Transition(cmdList, environmentResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);

		ConvolveFromSource(cmdList, heap, environmentShaderResourceViewIndex_);
	}

	void SkyRenderer::GenerateStaticEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		/// [EN] Project the HDR equirect onto all six environment faces (sky).
		/// [JP] HDR equirect を environment の 6 面（空）へ投影する。
		Transition(cmdList, environmentResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		{
			SkyGenerateConstant data{};
			data.sourceIndex_ = sourceEquirectShaderResourceViewIndex_;
			data.destIndex_ = environmentUnorderedAccessViewIndices_[0];
			data.faceSize_ = environmentSize_;
			Dispatch(cmdList, heap, equirectToCubePipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 6);
		}
		UnorderedAccessBarrier(cmdList, environmentResource_.Get());
		Transition(cmdList, environmentResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);

		/// [EN] Seed the dynamic cube with the same sky so the first Realtime
		///      captures don't convolve uninitialised faces.
		/// [JP] dynamic キューブも同じ空で初期化し、最初の Realtime キャプチャが
		///      未初期化面を畳み込まないようにする。
		if (captureType_ == SkyLight::CaptureType::Realtime)
		{
			Transition(cmdList, dynamicResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			{
				SkyGenerateConstant data{};
				data.sourceIndex_ = sourceEquirectShaderResourceViewIndex_;
				data.destIndex_ = dynamicUnorderedAccessViewIndex_;
				data.faceSize_ = environmentSize_;
				Dispatch(cmdList, heap, equirectToCubePipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 6);
			}
			UnorderedAccessBarrier(cmdList, dynamicResource_.Get());
			Transition(cmdList, dynamicResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);
		}

		ConvolveFromSource(cmdList, heap, environmentShaderResourceViewIndex_);
	}

	void SkyRenderer::ConvolveFromSource(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, Uint sourceShaderResourceViewIndex)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		Transition(cmdList, irradianceResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Transition(cmdList, prefilterResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			SkyGenerateConstant data{};
			data.sourceIndex_ = sourceShaderResourceViewIndex;
			data.destIndex_ = irradianceUnorderedAccessViewIndex_;
			data.faceSize_ = irradianceSize_;
			Dispatch(cmdList, heap, diffuseIrradiancePipeline_.Get(), data, (irradianceSize_ + 7) / 8, (irradianceSize_ + 7) / 8, 6);
		}

		for (Uint mip = 0; mip < prefilterMipLevels_; mip++)
		{
			Uint mipSize = prefilterSize_ >> mip;
			if (mipSize == 0)
			{
				mipSize = 1;
			}

			SkyGenerateConstant data{};
			data.sourceIndex_ = sourceShaderResourceViewIndex;
			data.destIndex_ = prefilterUnorderedAccessViewIndices_[mip];
			data.faceSize_ = mipSize;
			data.sampleCount_ = prefilterSampleCount_;
			data.mipLevel_ = mip;
			data.roughness_ = (prefilterMipLevels_ <= 1) ? 0.0f : static_cast<Float>(mip) / static_cast<Float>(prefilterMipLevels_ - 1);
			Dispatch(cmdList, heap, specularPrefilterPipeline_.Get(), data, (mipSize + 7) / 8, (mipSize + 7) / 8, 6);
		}

		UnorderedAccessBarrier(cmdList, irradianceResource_.Get());
		UnorderedAccessBarrier(cmdList, prefilterResource_.Get());
		Transition(cmdList, irradianceResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);
		Transition(cmdList, prefilterResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);
	}

	Bool SkyRenderer::RealtimeCaptureReady(Float deltaTime)
	{
		if (!hasSkymap_ || captureType_ != SkyLight::CaptureType::Realtime || generatedSourceShaderResourceViewIndex_ != sourceEquirectShaderResourceViewIndex_ || sourceEquirectShaderResourceViewIndex_ == invalidIndex_)
		{
			captureTimer_ = 0.0f;
			return false;
		}

		captureTimer_ += deltaTime;
		if (captureTimer_ >= captureDuration_)
		{
			captureTimer_ = 0.0f;
			return true;
		}
		return false;
	}

	Uint SkyRenderer::CurrentCaptureFace()const
	{
		return currentCaptureFace_;
	}

	void SkyRenderer::AdvanceCaptureFace()
	{
		currentCaptureFace_ = (currentCaptureFace_ + 1) % 6;
	}

	D3D12_GPU_VIRTUAL_ADDRESS SkyRenderer::CaptureConstantAddress()const
	{
		return captureConstantIndicesBuffer_->Address();
	}

	void SkyRenderer::UpdateFaceCamera(Uint face, Uint lightIndex)
	{
		static const Vector3 forwards[6] =
		{
			Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f),
			Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
			Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f),
		};
		static const Vector3 ups[6] =
		{
			Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f),
			Vector3(0.0f, 0.0f, -1.0f), Vector3(0.0f, 0.0f, 1.0f),
			Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f),
		};

		const Vector3 center = Vector3(0.0f, 0.0f, 0.0f);

		Matrix view = Matrix::CreateLookAt(center, center + forwards[face], ups[face]);

		Matrix projection = Matrix::CreatePerspectiveFieldOfView(1.57079632679f, 1.0f, captureNearPlane_, captureFarPlane_);
		projection._33 = captureNearPlane_ / (captureNearPlane_ - captureFarPlane_);
		projection._43 = (captureFarPlane_ * captureNearPlane_) / (captureFarPlane_ - captureNearPlane_);

		Matrix viewProjection = view * projection;

		SceneConstantBuffer scene{};
		scene.view_ = view;
		scene.inverseView_ = view.Invert();
		scene.projection_ = projection;
		scene.inverseProjection_ = projection.Invert();
		scene.nonJitterProjection_ = projection;
		scene.currentViewProjection_ = viewProjection;
		scene.previousViewProjection_ = viewProjection;
		scene.inverseViewProjection_ = viewProjection.Invert();
		scene.nonJitterViewProjection_ = viewProjection;
		scene.cameraPosition_ = Vector4(center.x, center.y, center.z, 1.0f);
		scene.cameraFocus_ = Vector4(forwards[face].x, forwards[face].y, forwards[face].z, 0.0f);
		scene.fieldOfView_ = 1.57079632679f;
		scene.nearPlane_ = captureNearPlane_;
		scene.farPlane_ = captureFarPlane_;
		scene.screenSize_ = Vector2(static_cast<Float>(environmentSize_), static_cast<Float>(environmentSize_));
		scene.inverseScreenSize_ = Vector2(1.0f / environmentSize_, 1.0f / environmentSize_);
		scene.displaySize_ = scene.screenSize_;
		captureSceneConstantBuffer_->Update(scene);

		ConstantIndices indices{};
		indices.sceneIndex_ = captureSceneConstantBuffer_->GetIndex();
		indices.lightIndex_ = lightIndex;
		indices.clusterConstantIndex_ = 0;
		captureConstantIndicesBuffer_->Update(indices);
	}

	void SkyRenderer::BeginFaceCapture(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, Uint face, Uint lightIndex)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		UpdateFaceCamera(face, lightIndex);

		/// [EN] Restore the sky onto this dynamic face (currently the HDR skymap;
		///      later a volumetric-cloud pass), then move the cube to a render
		///      target for the forward scene draw.
		/// [JP] この dynamic 面へ空を復元し（現状は HDR スカイマップ、将来は
		///      ボリューメトリッククラウド）、フォワードのシーン描画のためキューブを
		///      レンダーターゲットへ移す。
		Transition(cmdList, dynamicResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		SkyGenerateConstant data{};
		data.sourceIndex_ = sourceEquirectShaderResourceViewIndex_;
		data.destIndex_ = dynamicUnorderedAccessViewIndex_;
		data.faceSize_ = environmentSize_;
		data.faceOffset_ = face;
		Dispatch(cmdList, heap, equirectToCubePipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 1);

		UnorderedAccessBarrier(cmdList, dynamicResource_.Get());
		Transition(cmdList, dynamicResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

		auto* cmd = cmdList->Get();

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = dynamicRenderTargetViewHeap_.CPUHandle(face);
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = captureDepthStencilViewHeap_.CPUHandle(0);
		cmd->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<Float>(environmentSize_), static_cast<Float>(environmentSize_), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, static_cast<Int>(environmentSize_), static_cast<Int>(environmentSize_) };
		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissor);

		cmd->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	}

	void SkyRenderer::EndFaceCapture(D3D12CommandList* cmdList, Uint face)
	{
		(void)face;
		Transition(cmdList, dynamicResource_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	void SkyRenderer::ConvolveRealtime(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap)
	{
		ConvolveFromSource(cmdList, heap, dynamicShaderResourceViewIndex_);
	}

	void SkyRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, ID3D12PipelineState* pipeline, const SkyGenerateConstant& data, Uint groupsX, Uint groupsY, Uint groupsZ, D3D12_GPU_VIRTUAL_ADDRESS structuredAddress)
	{
		ConstantBuffer<SkyGenerateConstant>* constantBuffer = constantBuffers_[dispatchCursor_ % maxGenerateDispatches_].get();
		dispatchCursor_++;
		constantBuffer->Update(data);

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(rootSignature_->Get());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantBuffer->Address());
		if (structuredAddress != 0)
		{
			cmd->SetComputeRootConstantBufferView(3, structuredAddress);
		}
		cmd->SetPipelineState(pipeline);
		cmd->Dispatch(groupsX, groupsY, groupsZ);
	}

	void SkyRenderer::Transition(D3D12CommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, Uint subresource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = subresource;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		cmdList->Get()->ResourceBarrier(1, &barrier);
	}

	void SkyRenderer::UnorderedAccessBarrier(D3D12CommandList* cmdList, ID3D12Resource* resource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource;
		cmdList->Get()->ResourceBarrier(1, &barrier);
	}
}
