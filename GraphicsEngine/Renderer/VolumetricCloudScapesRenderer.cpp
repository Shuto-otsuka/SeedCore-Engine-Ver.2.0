#include <GraphicsEngine/Renderer/VolumetricCloudScapesRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	VolumetricCloudScapesRenderer::VolumetricCloudScapesRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : cloudShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricCloudScapesRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		/// [JP] 縮小解像度で確保する。切り上げなので、画面が奇数サイズでも
		///      2x2 ブロックが画面外へはみ出す分を含めて必ず覆える。
		width_ = (width + resolutionDivisor_ - 1) / resolutionDivisor_;
		height_ = (height + resolutionDivisor_ - 1) / resolutionDivisor_;

		cloudShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<VolumetricCloudScapesRayConstantBuffer>>(device, bindlessHeap);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&cloudResource_));
		SC_HR_CHECK(hr, "クラウドリソースの生成に失敗しました");
		cloudState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		cloudUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(cloudResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(cloudUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(cloudResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		cloudShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(cloudResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(cloudShaderResourceViewIndex_));

		CreateNoiseTexture(device, bindlessHeap, shapeNoiseSize, shapeNoiseResource_, shapeNoiseUnorderedAccessViewIndex_, shapeNoiseShaderResourceViewIndex_);
		CreateNoiseTexture(device, bindlessHeap, detailNoiseSize, detailNoiseResource_, detailNoiseUnorderedAccessViewIndex_, detailNoiseShaderResourceViewIndex_);
	}

	void VolumetricCloudScapesRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(cloudUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(cloudShaderResourceViewIndex_);
		cloudResource_.Reset();
	}

	void VolumetricCloudScapesRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = (width + resolutionDivisor_ - 1) / resolutionDivisor_;
		height_ = (height + resolutionDivisor_ - 1) / resolutionDivisor_;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width_;
		resourceDesc.Height = height_;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&cloudResource_));
		SC_HR_CHECK(hr, "クラウドリソースの生成に失敗しました");
		cloudState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		cloudUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(cloudResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(cloudUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(cloudResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		cloudShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(cloudResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(cloudShaderResourceViewIndex_));
	}

	void VolumetricCloudScapesRenderer::CreateNoiseTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 size,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex)
	{
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		resourceDesc.Width = size;
		resourceDesc.Height = size;
		resourceDesc.DepthOrArraySize = static_cast<Uint16>(size);
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
		SC_HR_CHECK(hr, "ノイズテクスチャの生成に失敗しました");

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		unorderedAccessViewDesc.Texture3D.WSize = size;

		outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

		outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture3D.MipLevels = 1;
		device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
	}

	void VolumetricCloudScapesRenderer::PrepareFrame(const VolumetricCloudScapesRayConstantBuffer& settings, Bool enabled)
	{
		VolumetricCloudScapesRayConstantBuffer uploadSettings = settings;
		uploadSettings.frameIndex_ = frameIndex_;
		uploadSettings.proceduralSkyEnabled_ = enabled ? 1 : 0;
		++frameIndex_;

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetCloudRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetCloudOutputUnorderedAccessViewIndex(cloudUnorderedAccessViewIndex_);
		indicesSystem_->SetCloudOutputShaderResourceViewIndex(cloudShaderResourceViewIndex_);
		indicesSystem_->SetCloudShapeNoiseUnorderedAccessViewIndex(shapeNoiseUnorderedAccessViewIndex_);
		indicesSystem_->SetCloudShapeNoiseShaderResourceViewIndex(shapeNoiseShaderResourceViewIndex_);
		indicesSystem_->SetCloudDetailNoiseUnorderedAccessViewIndex(detailNoiseUnorderedAccessViewIndex_);
		indicesSystem_->SetCloudDetailNoiseShaderResourceViewIndex(detailNoiseShaderResourceViewIndex_);
	}

	void VolumetricCloudScapesRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled)
	{
		auto* cmd = cmdList->Get();

		if (cloudState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(cloudResource_.Get(), cloudState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cloudState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12PipelineState* pipelineState = cloudShader_.GetPipelineState();

		if (!pipelineState && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("VolumetricCloudScapesRT のコンピュート PSO 作成に失敗しています。雲は常に無し(0)として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!enabled || !pipelineState)
		{
			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(cloudUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearIndex_), cloudResource_.Get(), clearValues, 0, nullptr);
		}
		else
		{
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(cloudShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);

			/// [JP] 初回のみノイズ Texture3D をベイクする(コマンドリストが要る
			///      ため Create() ではなくここで行う)。以後は焼き込み済みを
			///      wrap サンプルするだけ。
			if (!noiseBaked_)
			{
				ID3D12PipelineState* shapeBakePipeline = cloudShader_.GetShapeBakePipelineState();
				ID3D12PipelineState* detailBakePipeline = cloudShader_.GetDetailBakePipelineState();

				if (shapeBakePipeline && detailBakePipeline)
				{
					cmdList->Barrier(shapeNoiseResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					cmdList->Barrier(detailNoiseResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					cmd->SetPipelineState(shapeBakePipeline);
					cmd->Dispatch(shapeNoiseSize / 4, shapeNoiseSize / 4, shapeNoiseSize / 4);

					cmd->SetPipelineState(detailBakePipeline);
					cmd->Dispatch(detailNoiseSize / 4, detailNoiseSize / 4, detailNoiseSize / 4);

					cmdList->Barrier(shapeNoiseResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					cmdList->Barrier(detailNoiseResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

					noiseBaked_ = true;
				}
			}

			cmd->SetPipelineState(pipelineState);

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();
		}

		cmdList->Barrier(cloudResource_.Get(), cloudState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		cloudState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}
