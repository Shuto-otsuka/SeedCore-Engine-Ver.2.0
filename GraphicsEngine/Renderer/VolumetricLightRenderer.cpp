#include <GraphicsEngine/Renderer/VolumetricLightRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	VolumetricLightRenderer::VolumetricLightRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : volumetricLightShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricLightRenderer::CreateVolume(ID3D12Device* device, BindlessHeap* bindlessHeap, Bool createShaderResourceView,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32* outShaderResourceViewIndex, Uint32* outClearIndex)
	{
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		resourceDesc.Width = froxelDimensionX;
		resourceDesc.Height = froxelDimensionY;
		resourceDesc.DepthOrArraySize = static_cast<Uint16>(froxelDimensionZ);
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
		SC_HR_CHECK(hr, "ボリュームリソースの生成に失敗しました");

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		unorderedAccessViewDesc.Texture3D.WSize = froxelDimensionZ;

		outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

		if (outClearIndex)
		{
			*outClearIndex = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(*outClearIndex));
		}

		if (createShaderResourceView && outShaderResourceViewIndex)
		{
			*outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture3D.MipLevels = 1;
			device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(*outShaderResourceViewIndex));
		}
	}

	void VolumetricLightRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		volumetricLightShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<VolumetricLightRayConstantBuffer>>(device, bindlessHeap);

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);

		CreateVolume(device, bindlessHeap, false, densityVolumeResource_, densityVolumeUnorderedAccessViewIndex_, nullptr, nullptr);
		CreateVolume(device, bindlessHeap, false, scatteringVolumeResource_, scatteringVolumeUnorderedAccessViewIndex_, nullptr, nullptr);
		CreateVolume(device, bindlessHeap, true, integrationVolumeResource_, integrationVolumeUnorderedAccessViewIndex_, &integrationVolumeShaderResourceViewIndex_, &clearIntegrationIndex_);
		integrationVolumeState_ = D3D12_RESOURCE_STATE_COMMON;
	}

	void VolumetricLightRenderer::PrepareFrame(const VolumetricLightRayConstantBuffer& settings)
	{
		VolumetricLightRayConstantBuffer uploadSettings = settings;
		uploadSettings.froxelDimensionX_ = froxelDimensionX;
		uploadSettings.froxelDimensionY_ = froxelDimensionY;
		uploadSettings.froxelDimensionZ_ = froxelDimensionZ;

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetVolumetricLightRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetVolumetricLightDensityUnorderedAccessViewIndex(densityVolumeUnorderedAccessViewIndex_);
		indicesSystem_->SetVolumetricLightScatteringUnorderedAccessViewIndex(scatteringVolumeUnorderedAccessViewIndex_);
		indicesSystem_->SetVolumetricLightIntegrationUnorderedAccessViewIndex(integrationVolumeUnorderedAccessViewIndex_);
		indicesSystem_->SetVolumetricLightIntegrationShaderResourceViewIndex(integrationVolumeShaderResourceViewIndex_);
	}

	void VolumetricLightRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool enabled)
	{
		auto* cmd = cmdList->Get();

		/// [JP] density/scattering は UAV でしか読み書きしないので、初回に
		///      一度だけ UNORDERED_ACCESS へ遷移してそのまま維持する。
		if (!workingVolumesTransitioned_)
		{
			cmdList->Barrier(densityVolumeResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmdList->Barrier(scatteringVolumeResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			workingVolumesTransitioned_ = true;
		}

		if (integrationVolumeState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(integrationVolumeResource_.Get(), integrationVolumeState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			integrationVolumeState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12PipelineState* injectionPipeline = volumetricLightShader_.GetInjectionPipelineState();
		ID3D12PipelineState* scatteringPipeline = volumetricLightShader_.GetScatteringPipelineState();
		ID3D12PipelineState* integrationPipeline = volumetricLightShader_.GetIntegrationPipelineState();

		Bool pipelinesReady = injectionPipeline && scatteringPipeline && integrationPipeline;

		if (!pipelinesReady && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("FogInjection/VolumetricLightScattering/FroxelIntegration のコンピュート PSO 作成に失敗しています。フォグ/体積光は常に無し として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!enabled || !pipelinesReady)
		{
			/// [JP] 無効時: 散乱0・透過率1 にクリアして合成を実質 no-op にする。
			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(integrationVolumeUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearIntegrationIndex_), integrationVolumeResource_.Get(), clearValues, 0, nullptr);
		}
		else
		{
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(volumetricLightShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);

			Uint32 groupsX = (froxelDimensionX + 3) / 4;
			Uint32 groupsY = (froxelDimensionY + 3) / 4;
			Uint32 groupsZ = (froxelDimensionZ + 3) / 4;

			D3D12_RESOURCE_BARRIER uavBarrier{};
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

			cmd->SetPipelineState(injectionPipeline);
			cmd->Dispatch(groupsX, groupsY, groupsZ);
			ProfilerStats::AddDrawCall();

			uavBarrier.UAV.pResource = densityVolumeResource_.Get();
			cmd->ResourceBarrier(1, &uavBarrier);

			cmd->SetPipelineState(scatteringPipeline);
			cmd->Dispatch(groupsX, groupsY, groupsZ);
			ProfilerStats::AddDrawCall();

			uavBarrier.UAV.pResource = scatteringVolumeResource_.Get();
			cmd->ResourceBarrier(1, &uavBarrier);

			cmd->SetPipelineState(integrationPipeline);
			cmd->Dispatch((froxelDimensionX + 7) / 8, (froxelDimensionY + 7) / 8, 1);
			ProfilerStats::AddDrawCall();
		}

		cmdList->Barrier(integrationVolumeResource_.Get(), integrationVolumeState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		integrationVolumeState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}
