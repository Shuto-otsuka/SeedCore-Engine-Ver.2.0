#include <GraphicsEngine/Renderer/SkyRenderer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/Sky/Skymap.h>
#include <GraphicsEngine/Sky/SkymapResource.h>
#include <GraphicsEngine/Light/SkyLight.h>
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

		equirectCubemapPipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/Cubemap/EquirectCubemapCS.hlsl"));
		diffuseIrradiancePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/IBL/DiffuseIrradianceCS.hlsl"));
		specularPrefilterPipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/IBL/SpecularPrefilterCS.hlsl"));
		brdfLookupTablePipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/IBL/BrdfLutCS.hlsl"));
		proceduralCubemapPipeline_ = CreateComputePipeline(device, shaderCache, pipelineStateObject, String("../GraphicsEngine/Sky/Cubemap/ProceduralCubemapCS.hlsl"));

		constantBuffers_.reserve(maxGenerateDispatches_);
		for (Uint dispatchIndex = 0; dispatchIndex < maxGenerateDispatches_; dispatchIndex++)
		{
			constantBuffers_.push_back(MakePtr<ConstantBuffer<SkyDispatchBuffer>>(device, bindlessHeap));
		}

		skyConstantBuffer_ = MakePtr<ConstantBuffer<SkyConstantBuffer>>(device, bindlessHeap);

		CreateBrdfLookupTable(device, bindlessHeap);
		CreateIblCubes(device, bindlessHeap);
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
#ifdef _DEBUG
		brdfLookupTableResource_->SetName(L"Sky_BrdfLookupTable");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(brdfLookupTableResource_.Get());
#endif

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
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		CreateCube(device, bindlessHeap, environmentSize_, environmentMipLevels_, nullptr, readState, environmentResource_, environmentShaderResourceViewIndex_, environmentUnorderedAccessViewIndices_);
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
#ifdef _DEBUG
		resource->SetName(L"Sky_Cubemap");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(resource.Get());
#endif

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

	void SkyRenderer::Gather(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world)
	{
		hasSkymap_ = false;
		sourceShaderResourceViewIndex_ = SC_INVALID;
		intensity_ = 1.0f;

		SkymapResource* skymapResource = resourceCache.GetResource<SkymapResource>(AssetType::Skymap);
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
				sourceShaderResourceViewIndex_ = skymap->ShaderResourceViewIndex();
				intensity_ = skyLight.intensity_;
				found = true;
			});

		/// [EN] Note: the generated cubes are kept while the sky is toggled off
		///      (nothing overwrites them), so toggling the same skymap back on
		///      resumes instantly. A change to a different skymap is detected
		///      by its differing source index in Generate and triggers a
		///      regenerate there.
		/// [JP] 空を OFF にしている間も生成済みキューブは保持される（誰も上書き
		///      しない）ので、同じスカイマップに戻せば即再開する。別スカイマップ
		///      への変更は Generate 側で source index の差異として検出され、
		///      そこで再生成される。
	}

	void SkyRenderer::SetProceduralSky(Bool enabled, Uint32 settingsHash, Uint lightIndex, Float totalTime)
	{
		proceduralSkyEnabled_ = enabled;
		proceduralSkyHash_ = settingsHash;
		proceduralSkyLightIndex_ = lightIndex;
		proceduralSkyTime_ = totalTime;
	}

	void SkyRenderer::SetIndices(ConstantIndicesSystem& constantIndicesSystem, ShaderResourceIndicesSystem& shaderResourceIndicesSystem, Float directionalIntensity)
	{
		shaderResourceIndicesSystem.SetSkyBrdfLutIndex(brdfLookupTableShaderResourceViewIndex_);

		Float uploadIntensity;
		Bool ready = hasSkymap_ && generatedSourceShaderResourceViewIndex_ != SC_INVALID;
		if (ready)
		{
			shaderResourceIndicesSystem.SetSkyEnvironmentCubeIndex(environmentShaderResourceViewIndex_);
			shaderResourceIndicesSystem.SetSkyDiffuseIrradianceIndex(irradianceShaderResourceViewIndex_);
			shaderResourceIndicesSystem.SetSkySpecularPrefilteredIndex(prefilterShaderResourceViewIndex_);
			uploadIntensity = intensity_ * Max(directionalIntensity, 0.0f);
		}
		else if (!hasSkymap_ && proceduralSkyEnabled_ && proceduralSkyGenerated_)
		{
			/// [EN] Procedural-sky IBL: register irradiance/prefilter only.
			///      The environment cube index deliberately stays 0 so the
			///      background remains the ANALYTIC sky (crisp sun disc, see
			///      DeferredLightingPS.hlsl) while lit surfaces receive the
			///      convolved sky as IBL.
			/// [JP] プロシージャル空の IBL: irradiance/prefilter のみ登録する。
			///      environment キューブのインデックスは意図的に 0 のままに
			///      して、背景は【解析的な】空(シャープな太陽ディスク、
			///      DeferredLightingPS.hlsl 参照)のまま、ライティング面だけが
			///      畳み込み済みの空を IBL として受け取るようにする。
			shaderResourceIndicesSystem.SetSkyEnvironmentCubeIndex(0);
			shaderResourceIndicesSystem.SetSkyDiffuseIrradianceIndex(irradianceShaderResourceViewIndex_);
			shaderResourceIndicesSystem.SetSkySpecularPrefilteredIndex(prefilterShaderResourceViewIndex_);
			uploadIntensity = 1.0f;
		}
		else
		{
			shaderResourceIndicesSystem.SetSkyEnvironmentCubeIndex(0);
			shaderResourceIndicesSystem.SetSkyDiffuseIrradianceIndex(0);
			shaderResourceIndicesSystem.SetSkySpecularPrefilteredIndex(0);
			uploadIntensity = 1.0f;
		}

		SkyConstantBuffer data{};
		data.intensity_ = uploadIntensity;
		skyConstantBuffer_->Update(data);
		constantIndicesSystem.SetSkyIndex(skyConstantBuffer_->GetIndex());
	}

	void SkyRenderer::Generate(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		dispatchCursor_ = 0;

		if (!brdfLookupTableGenerated_)
		{
			SkyDispatchBuffer data{};
			data.destIndex_ = brdfLookupTableUnorderedAccessViewIndex_;
			data.faceSize_ = brdfLookupTableSize_;
			data.sampleCount_ = brdfSampleCount_;
			Dispatch(cmdList, heap, brdfLookupTablePipeline_.Get(), data, (brdfLookupTableSize_ + 7) / 8, (brdfLookupTableSize_ + 7) / 8, 1, addresses);

			UnorderedAccessBarrier(cmdList, brdfLookupTableResource_.Get());
			Transition(cmdList, brdfLookupTableResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			brdfLookupTableGenerated_ = true;
		}

		if (!hasSkymap_ || sourceShaderResourceViewIndex_ == SC_INVALID)
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
					GenerateProceduralEnvironment(cmdList, heap, addresses);
					generatedProceduralSkyHash_ = proceduralSkyHash_;
					proceduralSkyGenerated_ = true;
					proceduralSkyRefreshCounter_ = 0;

					/// [JP] environment キューブをプロシージャル空で上書きした
					///      ので、スカイマップ側の生成済み状態を無効化する
					///      (スカイマップへ戻した時に必ず再生成させる)。
					generatedSourceShaderResourceViewIndex_ = SC_INVALID;
				}
			}
			return;
		}
		if (sourceShaderResourceViewIndex_ == generatedSourceShaderResourceViewIndex_)
		{
			return;
		}

		GenerateStaticEnvironment(cmdList, heap, addresses);
		generatedSourceShaderResourceViewIndex_ = sourceShaderResourceViewIndex_;

		/// [JP] スカイマップで environment/畳み込みを上書きしたので、
		///      プロシージャル空側の生成済み状態を無効化する。
		proceduralSkyGenerated_ = false;
	}

	void SkyRenderer::GenerateProceduralEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		/// [JP] 解析的な空+太陽を environment の6面へ描く。太陽は
		///      constant_indices 経由の GetDirectionalLightConstantBuffer()
		///      から引くため(ProceduralCubemapCS.hlsl 参照)、source_index_
		///      はこのパスでは未使用。
		Transition(cmdList, environmentResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		{
			SkyDispatchBuffer data{};
			data.destIndex_ = environmentUnorderedAccessViewIndices_[0];
			data.faceSize_ = environmentSize_;

			/// [JP] roughness_ はこのパスでは prefilter 用として未使用なので、
			///      雲の風スクロール時刻を転用して渡す(ProceduralCubemapCS 参照)。
			data.roughness_ = proceduralSkyTime_;
			Dispatch(cmdList, heap, proceduralCubemapPipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 6, addresses);
		}
		UnorderedAccessBarrier(cmdList, environmentResource_.Get());
		Transition(cmdList, environmentResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);

		ConvolveFromSource(cmdList, heap, environmentShaderResourceViewIndex_, addresses);
	}

	void SkyRenderer::GenerateStaticEnvironment(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		/// [EN] Project the HDR equirect onto all six environment faces (sky).
		/// [JP] HDR equirect を environment の 6 面（空）へ投影する。
		Transition(cmdList, environmentResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		{
			SkyDispatchBuffer data{};
			data.sourceIndex_ = sourceShaderResourceViewIndex_;
			data.destIndex_ = environmentUnorderedAccessViewIndices_[0];
			data.faceSize_ = environmentSize_;
			Dispatch(cmdList, heap, equirectCubemapPipeline_.Get(), data, (environmentSize_ + 7) / 8, (environmentSize_ + 7) / 8, 6, addresses);
		}
		UnorderedAccessBarrier(cmdList, environmentResource_.Get());
		Transition(cmdList, environmentResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);

		ConvolveFromSource(cmdList, heap, environmentShaderResourceViewIndex_, addresses);
	}

	void SkyRenderer::ConvolveFromSource(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, Uint sourceShaderResourceViewIndex, const RootAddresses& addresses)
	{
		const D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		Transition(cmdList, irradianceResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Transition(cmdList, prefilterResource_.Get(), readState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			SkyDispatchBuffer data{};
			data.sourceIndex_ = sourceShaderResourceViewIndex;
			data.destIndex_ = irradianceUnorderedAccessViewIndex_;
			data.faceSize_ = irradianceSize_;
			Dispatch(cmdList, heap, diffuseIrradiancePipeline_.Get(), data, (irradianceSize_ + 7) / 8, (irradianceSize_ + 7) / 8, 6, addresses);
		}

		for (Uint mip = 0; mip < prefilterMipLevels_; mip++)
		{
			Uint mipSize = prefilterSize_ >> mip;
			if (mipSize == 0)
			{
				mipSize = 1;
			}

			SkyDispatchBuffer data{};
			data.sourceIndex_ = sourceShaderResourceViewIndex;
			data.destIndex_ = prefilterUnorderedAccessViewIndices_[mip];
			data.faceSize_ = mipSize;
			data.sampleCount_ = prefilterSampleCount_;
			data.mipLevel_ = mip;
			data.roughness_ = (prefilterMipLevels_ <= 1) ? 0.0f : static_cast<Float>(mip) / static_cast<Float>(prefilterMipLevels_ - 1);
			Dispatch(cmdList, heap, specularPrefilterPipeline_.Get(), data, (mipSize + 7) / 8, (mipSize + 7) / 8, 6, addresses);
		}

		UnorderedAccessBarrier(cmdList, irradianceResource_.Get());
		UnorderedAccessBarrier(cmdList, prefilterResource_.Get());
		Transition(cmdList, irradianceResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);
		Transition(cmdList, prefilterResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readState);
	}

	void SkyRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, ID3D12PipelineState* pipeline, const SkyDispatchBuffer& data, Uint groupsX, Uint groupsY, Uint groupsZ, const RootAddresses& addresses)
	{
		ConstantBuffer<SkyDispatchBuffer>* constantBuffer = constantBuffers_[dispatchCursor_ % maxGenerateDispatches_].get();
		dispatchCursor_++;
		constantBuffer->Update(data);

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(rootSignature_->Get());
		RootSignature::BindCompute(cmd, addresses);
		Uint dispatchBufferIndex = constantBuffer->GetIndex();
		cmd->SetComputeRoot32BitConstants(3, 1, &dispatchBufferIndex, 0);
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
