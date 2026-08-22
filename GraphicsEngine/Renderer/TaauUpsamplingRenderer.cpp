#include <GraphicsEngine/Renderer/TaauUpsamplingRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	TaauUpsamplingRenderer::TaauUpsamplingRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : resolveShader_(rootSignature, pipelineStateObject), backgroundVelocityShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void TaauUpsamplingRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 outputWidth, Uint32 outputHeight)
	{
		resolveShader_.Create(shaderCache, device);
		backgroundVelocityShader_.Create(shaderCache, device);

		CreateViewResources(device, bindlessHeap, outputWidth, outputHeight);
	}

	void TaauUpsamplingRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		View* views[] = { &editorView_, &gameView_ };
		for (View* view : views)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount_; ++slot)
			{
				bindlessHeap->FreeIndex(view->accumulatedUnorderedAccessViewIndex_[slot]);
				bindlessHeap->FreeIndex(view->accumulatedShaderResourceViewIndex_[slot]);
				view->accumulatedResource_[slot].Reset();
			}
			view->constantBuffer_ = nullptr;
		}
	}

	void TaauUpsamplingRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 outputWidth, Uint32 outputHeight)
	{
		Destroy(bindlessHeap);
		CreateViewResources(device, bindlessHeap, outputWidth, outputHeight);
	}

	void TaauUpsamplingRenderer::CreateViewResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 outputWidth, Uint32 outputHeight)
	{
		bindlessHeap_ = bindlessHeap;
		outputWidth_ = outputWidth;
		outputHeight_ = outputHeight;

		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC accumulatedDesc{};
		accumulatedDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		accumulatedDesc.Width = static_cast<Uint64>(outputWidth);
		accumulatedDesc.Height = outputHeight;
		accumulatedDesc.DepthOrArraySize = 1;
		accumulatedDesc.MipLevels = 1;
		accumulatedDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		accumulatedDesc.SampleDesc.Count = 1;
		accumulatedDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		View* views[] = { &editorView_, &gameView_ };
		for (View* view : views)
		{
			view->writeSlot_ = 0;

			for (Uint32 slot = 0; slot < accumulationSlotCount_; ++slot)
			{
				hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &accumulatedDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view->accumulatedResource_[slot]));
				SC_HR_CHECK(hr, "TAAU 蓄積テクスチャの生成に失敗しました");
				view->accumulatedState_[slot] = D3D12_RESOURCE_STATE_COMMON;

				view->accumulatedUnorderedAccessViewIndex_[slot] = bindlessHeap->AllocateIndex();
				device->CreateUnorderedAccessView(view->accumulatedResource_[slot].Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view->accumulatedUnorderedAccessViewIndex_[slot]));

				view->accumulatedShaderResourceViewIndex_[slot] = bindlessHeap->AllocateIndex();
				device->CreateShaderResourceView(view->accumulatedResource_[slot].Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(view->accumulatedShaderResourceViewIndex_[slot]));
			}

			view->constantBuffer_ = MakePtr<ConstantBuffer<TaauResolveConstants>>(device, bindlessHeap);
		}
	}

	void TaauUpsamplingRenderer::PrepareView(RaytracingView view)
	{
		View& target = ViewFor(view);
		target.writeSlot_ = 1 - target.writeSlot_;
	}

	void TaauUpsamplingRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, ID3D12Resource* velocityResource, Uint32 colorShaderResourceViewIndex, Uint32 depthShaderResourceViewIndex, Uint32 velocityShaderResourceViewIndex, Uint32 sourceWidth, Uint32 sourceHeight)
	{
		auto* cmd = cmdList->Get();
		View& target = ViewFor(view);

		ID3D12PipelineState* resolvePipelineState = resolveShader_.GetPipelineState();
		if (!resolvePipelineState && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("TaauResolveCS のコンピュート PSO 作成に失敗しています。TAAU は実行されません。");
			pipelineStateMissingLogged_ = true;
		}

		if (!resolvePipelineState)
		{
			return;
		}

		ID3D12PipelineState* backgroundVelocityPipelineState = backgroundVelocityShader_.GetPipelineState();
		if (backgroundVelocityPipelineState)
		{
			ID3D12DescriptorHeap* backgroundVelocityHeaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(backgroundVelocityHeaps), backgroundVelocityHeaps);
			cmd->SetComputeRootSignature(backgroundVelocityShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);

			cmdList->Barrier(velocityResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			cmd->SetPipelineState(backgroundVelocityPipelineState);
			cmd->Dispatch((sourceWidth + 7) / 8, (sourceHeight + 7) / 8, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(velocityResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		Uint32 writeSlot = target.writeSlot_;
		Uint32 historySlot = 1 - writeSlot;

		if (target.accumulatedState_[writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(target.accumulatedResource_[writeSlot].Get(), target.accumulatedState_[writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			target.accumulatedState_[writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		if (target.accumulatedState_[historySlot] != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		{
			cmdList->Barrier(target.accumulatedResource_[historySlot].Get(), target.accumulatedState_[historySlot], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			target.accumulatedState_[historySlot] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}

		TaauResolveConstants constants{};
		constants.colorShaderResourceViewIndex_ = colorShaderResourceViewIndex;
		constants.depthShaderResourceViewIndex_ = depthShaderResourceViewIndex;
		constants.velocityShaderResourceViewIndex_ = velocityShaderResourceViewIndex;
		constants.historyShaderResourceViewIndex_ = target.accumulatedShaderResourceViewIndex_[historySlot];
		constants.destinationUnorderedAccessViewIndex_ = target.accumulatedUnorderedAccessViewIndex_[writeSlot];
		constants.sourceWidth_ = sourceWidth;
		constants.sourceHeight_ = sourceHeight;
		constants.destinationWidth_ = outputWidth_;
		constants.destinationHeight_ = outputHeight_;
		target.constantBuffer_->Update(constants);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootSignature(resolveShader_.GetRootSignature());
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, target.constantBuffer_->Address());
		cmd->SetPipelineState(resolvePipelineState);
		cmd->Dispatch((outputWidth_ + 7) / 8, (outputHeight_ + 7) / 8, 1);
		ProfilerStats::AddDrawCall();

		cmdList->Barrier(target.accumulatedResource_[writeSlot].Get(), target.accumulatedState_[writeSlot], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		target.accumulatedState_[writeSlot] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}

	ID3D12Resource* TaauUpsamplingRenderer::OutputResource(RaytracingView view)const
	{
		const View& target = view == RaytracingView::Editor ? editorView_ : gameView_;
		return target.accumulatedResource_[target.writeSlot_].Get();
	}

	Uint32 TaauUpsamplingRenderer::OutputShaderResourceViewIndex(RaytracingView view)const
	{
		const View& target = view == RaytracingView::Editor ? editorView_ : gameView_;
		return target.accumulatedShaderResourceViewIndex_[target.writeSlot_];
	}

	TaauUpsamplingRenderer::View& TaauUpsamplingRenderer::ViewFor(RaytracingView view)
	{
		return view == RaytracingView::Editor ? editorView_ : gameView_;
	}
}
