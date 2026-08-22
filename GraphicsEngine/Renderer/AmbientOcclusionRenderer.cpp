#include <GraphicsEngine/Renderer/AmbientOcclusionRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	namespace
	{
		/// [JP] raw/accumulated 共通のテクスチャ作成ヘルパー。R16_FLOAT の
		///      UAV+SRV を確保し、ClearUnorderedAccessViewFloat 用の非シェーダ
		///      可視 UAV も併せて作る(ShadowRenderer の RG16 版と同型)。
		void CreateOpennessTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex, Uint32& outClearIndex)
		{
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

			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
			SC_HR_CHECK(hr, "オープンネステクスチャの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

			outClearIndex = clearHeap.AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap.CPUHandle(outClearIndex));

			outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R16_FLOAT;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
		}
	}

	AmbientOcclusionRenderer::AmbientOcclusionRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : ambientOcclusionShader_(rootSignature, pipelineStateObject), denoiseShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void AmbientOcclusionRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		width_ = width;
		height_ = height;

		ambientOcclusionShader_.Create(shaderCache, device);
		denoiseShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<AmbientOcclusionRayConstantBuffer>>(device, bindlessHeap);

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount, false);

		CreateOpennessTexture(device, bindlessHeap, clearHeap_, width, height, rawOpennessResource_, rawOpennessUnorderedAccessViewIndex_, rawOpennessShaderResourceViewIndex_, clearRawIndex_);
		rawOpennessState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateOpennessTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedOpennessResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedOpennessState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}

	void AmbientOcclusionRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(rawOpennessUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(rawOpennessShaderResourceViewIndex_);
		rawOpennessResource_.Reset();

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				bindlessHeap->FreeIndex(accumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(accumulatedShaderResourceViewIndex_[view][slot]);
				accumulatedOpennessResource_[view][slot].Reset();
			}
		}
	}

	void AmbientOcclusionRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount, false);

		CreateOpennessTexture(device, bindlessHeap, clearHeap_, width, height, rawOpennessResource_, rawOpennessUnorderedAccessViewIndex_, rawOpennessShaderResourceViewIndex_, clearRawIndex_);
		rawOpennessState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateOpennessTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedOpennessResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedOpennessState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}

	void AmbientOcclusionRenderer::PrepareFrame(const AmbientOcclusionRayConstantBuffer& settings, Bool useDlssRayReconstruction)
	{
		/// [JP] ピンポンの交換はここ(1回/フレーム)で行う。Dispatch() は
		///      Editor/Game の両ビューで1フレームに2回呼ばれる(ShadowRenderer と
		///      同じ理由)。
		historySlot_ = 1 - historySlot_;

		AmbientOcclusionRayConstantBuffer uploadSettings = settings;
		uploadSettings.frameIndex_ = frameIndex_;
		++frameIndex_;

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetAmbientOcclusionRayConstantIndex(tuningBuffer_->GetIndex());

		Uint32 writeSlot = 1 - historySlot_;

		indicesSystem_->SetAmbientOcclusionRawUnorderedAccessViewIndex(rawOpennessUnorderedAccessViewIndex_);
		indicesSystem_->SetAmbientOcclusionRawShaderResourceViewIndex(rawOpennessShaderResourceViewIndex_);

		constexpr Uint32 editorView = static_cast<Uint32>(RaytracingView::Editor);
		constexpr Uint32 gameView = static_cast<Uint32>(RaytracingView::Game);

		if (useDlssRayReconstruction)
		{
			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、このビューの
			///      「最終」AO読み取りは生の単一バッファテクスチャを直接指す
			///      (ピンポン蓄積チェーンには一切触れない)。
			indicesSystem_->SetEditorAmbientOcclusionIndices(rawOpennessShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], rawOpennessShaderResourceViewIndex_);
			indicesSystem_->SetGameAmbientOcclusionIndices(rawOpennessShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], rawOpennessShaderResourceViewIndex_);
		}
		else
		{
			indicesSystem_->SetEditorAmbientOcclusionIndices(accumulatedShaderResourceViewIndex_[editorView][historySlot_], accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], accumulatedShaderResourceViewIndex_[editorView][writeSlot]);
			indicesSystem_->SetGameAmbientOcclusionIndices(accumulatedShaderResourceViewIndex_[gameView][historySlot_], accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], accumulatedShaderResourceViewIndex_[gameView][writeSlot]);
		}
	}

	void AmbientOcclusionRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction)
	{
		auto* cmd = cmdList->Get();

		Uint32 viewIndex = static_cast<Uint32>(view);
		Uint32 writeSlot = 1 - historySlot_;

		ID3D12PipelineState* ambientOcclusionPipelineState = ambientOcclusionShader_.GetPipelineState();
		ID3D12PipelineState* denoisePipelineState = denoiseShader_.GetPipelineState();

		/// [JP] DLSS-RR経路ではdenoisePipelineStateの有無を「失敗」扱いしない
		///      (デノイズCS自体を使わないため)。
		Bool denoisePipelineRequired = !useDlssRayReconstruction;

		if ((!ambientOcclusionPipelineState || (denoisePipelineRequired && !denoisePipelineState)) && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("AmbientOcclusionRT/AmbientOcclusionDenoise のコンピュート PSO 作成に失敗しています。DXR インラインレイトレ(Tier 1.1)非対応の可能性があります。AO は常に開放(1.0)として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!tlasValid || !ambientOcclusionPipelineState || (denoisePipelineRequired && !denoisePipelineState))
		{
			/// [JP] 追跡対象(TLAS)が無い、AO が無効、または PSO が無いフレーム:
			///      開放(1.0)でクリアする。composite が実際に読む先(DLSS-RR
			///      経路なら生テクスチャ、通常経路ならピンポン write スロット)
			///      をそのままクリアする。
			if (useDlssRayReconstruction)
			{
				if (rawOpennessState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(rawOpennessResource_.Get(), rawOpennessState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					rawOpennessState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				const Float clearValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(rawOpennessUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearRawIndex_), rawOpennessResource_.Get(), clearValues, 0, nullptr);

				cmdList->Barrier(rawOpennessResource_.Get(), rawOpennessState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				rawOpennessState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedOpennessState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedOpennessResource_[viewIndex][writeSlot].Get(), accumulatedOpennessState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedOpennessState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float clearValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(accumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearAccumulatedIndex_[viewIndex][writeSlot]), accumulatedOpennessResource_[viewIndex][writeSlot].Get(), clearValues, 0, nullptr);

			cmdList->Barrier(accumulatedOpennessResource_[viewIndex][writeSlot].Get(), accumulatedOpennessState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			accumulatedOpennessState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			return;
		}
		else
		{
			if (rawOpennessState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(rawOpennessResource_.Get(), rawOpennessState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				rawOpennessState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(ambientOcclusionShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);
			cmd->SetPipelineState(ambientOcclusionPipelineState);

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(rawOpennessResource_.Get(), rawOpennessState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			rawOpennessState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (useDlssRayReconstruction)
			{
				/// [JP] DLSS-RRが自身で最終合成フレームをデノイズするので、
				///      このRenderer自身の時間積分(デノイズCS)は丸ごと
				///      スキップする — 生テクスチャを composite が読める状態
				///      (PIXEL_SHADER_RESOURCE)へ遷移させるだけでよい。
				cmdList->Barrier(rawOpennessResource_.Get(), rawOpennessState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				rawOpennessState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedOpennessState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(accumulatedOpennessResource_[viewIndex][historySlot_].Get(), accumulatedOpennessState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				accumulatedOpennessState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (accumulatedOpennessState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedOpennessResource_[viewIndex][writeSlot].Get(), accumulatedOpennessState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedOpennessState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoisePipelineState);
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(accumulatedOpennessResource_[viewIndex][writeSlot].Get(), accumulatedOpennessState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			accumulatedOpennessState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}
	}
}
