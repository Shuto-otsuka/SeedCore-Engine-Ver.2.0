#include <GraphicsEngine/Renderer/SubsurfaceScatteringRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	SubsurfaceScatteringRenderer::SubsurfaceScatteringRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : subsurfaceScatteringShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void SubsurfaceScatteringRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		width_ = width;
		height_ = height;

		subsurfaceScatteringShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<SubsurfaceScatteringRayConstantBuffer>>(device, bindlessHeap);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&transmittanceResource_));
		SC_HR_CHECK(hr, "透過率リソースの生成に失敗しました");
		transmittanceState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		transmittanceUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(transmittanceResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(transmittanceUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(transmittanceResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		transmittanceShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(transmittanceResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(transmittanceShaderResourceViewIndex_));
	}

	void SubsurfaceScatteringRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(transmittanceUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(transmittanceShaderResourceViewIndex_);
		transmittanceResource_.Reset();
	}

	void SubsurfaceScatteringRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = width;
		height_ = height;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R16_FLOAT;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&transmittanceResource_));
		SC_HR_CHECK(hr, "透過率リソースの生成に失敗しました");
		transmittanceState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		transmittanceUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(transmittanceResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(transmittanceUnorderedAccessViewIndex_));

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
		clearIndex_ = clearHeap_.AllocateIndex();
		device->CreateUnorderedAccessView(transmittanceResource_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearIndex_));

		transmittanceShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(transmittanceResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(transmittanceShaderResourceViewIndex_));
	}

	void SubsurfaceScatteringRenderer::PrepareFrame(const SubsurfaceScatteringRayConstantBuffer& settings)
	{
		tuningBuffer_->Update(settings);
		indicesSystem_->SetSubsurfaceScatteringRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetSubsurfaceScatteringTransmittanceUnorderedAccessViewIndex(transmittanceUnorderedAccessViewIndex_);
		indicesSystem_->SetSubsurfaceScatteringTransmittanceShaderResourceViewIndex(transmittanceShaderResourceViewIndex_);
	}

	void SubsurfaceScatteringRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid)
	{
		auto* cmd = cmdList->Get();

		if (transmittanceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(transmittanceResource_.Get(), transmittanceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			transmittanceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		ID3D12PipelineState* pipelineState = subsurfaceScatteringShader_.GetPipelineState();

		if (!pipelineState && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("SubsurfaceScatteringRT のコンピュート PSO 作成に失敗しています。DXR インラインレイトレ(Tier 1.1)非対応の可能性があります。透光は常に無し(0.0)として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!tlasValid || !pipelineState)
		{
			/// [JP] 追跡対象(TLAS)が無い、SSS が無効、または PSO が無いフレーム:
			///      透光なし(0.0)でクリアする。
			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(transmittanceUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearIndex_), transmittanceResource_.Get(), clearValues, 0, nullptr);
		}
		else
		{
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(subsurfaceScatteringShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);
			cmd->SetPipelineState(pipelineState);

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();
		}

		cmdList->Barrier(transmittanceResource_.Get(), transmittanceState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		transmittanceState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}
}
