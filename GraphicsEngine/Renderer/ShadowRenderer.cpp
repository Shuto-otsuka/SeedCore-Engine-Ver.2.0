#include <GraphicsEngine/Renderer/ShadowRenderer.h>
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
		/// [JP] raw/accumulated 共通のテクスチャ作成ヘルパー。RG16_FLOAT の
		///      UAV+SRV を確保し、ClearUnorderedAccessViewFloat 用の非シェーダ
		///      可視 UAV も併せて作る。
		void CreateVisibilityTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height,
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
			resourceDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
			SC_HR_CHECK(hr, "可視性テクスチャの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

			outClearIndex = clearHeap.AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap.CPUHandle(outClearIndex));

			outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
		}

		void CreateAtrousScratchTextures(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height, IndicesSystem& indicesSystem,
			Microsoft::WRL::ComPtr<ID3D12Resource>(&outResource)[2][2], Uint32(&outUnorderedAccessViewIndex)[2][2], Uint32(&outShaderResourceViewIndex)[2][2])
		{
			for (Uint32 view = 0; view < 2; ++view)
			{
				for (Uint32 slot = 0; slot < 2; ++slot)
				{
					Uint32 unusedClearIndex = 0;
					CreateVisibilityTexture(device, bindlessHeap, clearHeap, width, height, outResource[view][slot], outUnorderedAccessViewIndex[view][slot], outShaderResourceViewIndex[view][slot], unusedClearIndex);
				}
			}

			indicesSystem.SetEditorShadowAtrousScratchIndices(outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Editor)][0], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Editor)][0], outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Editor)][1], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Editor)][1]);
			indicesSystem.SetGameShadowAtrousScratchIndices(outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Game)][0], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Game)][0], outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Game)][1], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Game)][1]);
		}
	}

	ShadowRenderer::ShadowRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : shadowShader_(rootSignature, pipelineStateObject), denoiseShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void ShadowRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		width_ = width;
		height_ = height;

		shadowShader_.Create(shaderCache, device);
		denoiseShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<ShadowRayConstantBuffer>>(device, bindlessHeap);

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount + viewCount * 2, false);

		CreateVisibilityTexture(device, bindlessHeap, clearHeap_, width, height, rawVisibilityResource_, rawVisibilityUnorderedAccessViewIndex_, rawVisibilityShaderResourceViewIndex_, clearRawIndex_);
		rawVisibilityState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateVisibilityTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedVisibilityResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedVisibilityState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}

		CreateAtrousScratchTextures(device, bindlessHeap, clearHeap_, width, height, indicesSystem, atrousScratchResource_, atrousScratchUnorderedAccessViewIndex_, atrousScratchShaderResourceViewIndex_);
		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				atrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}

	void ShadowRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(rawVisibilityUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(rawVisibilityShaderResourceViewIndex_);
		rawVisibilityResource_.Reset();

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				bindlessHeap->FreeIndex(accumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(accumulatedShaderResourceViewIndex_[view][slot]);
				accumulatedVisibilityResource_[view][slot].Reset();
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				bindlessHeap->FreeIndex(atrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(atrousScratchShaderResourceViewIndex_[view][slot]);
				atrousScratchResource_[view][slot].Reset();
			}
		}
	}

	void ShadowRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount + viewCount * 2, false);

		CreateVisibilityTexture(device, bindlessHeap, clearHeap_, width, height, rawVisibilityResource_, rawVisibilityUnorderedAccessViewIndex_, rawVisibilityShaderResourceViewIndex_, clearRawIndex_);
		rawVisibilityState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateVisibilityTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedVisibilityResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedVisibilityState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}

		CreateAtrousScratchTextures(device, bindlessHeap, clearHeap_, width, height, *indicesSystem_, atrousScratchResource_, atrousScratchUnorderedAccessViewIndex_, atrousScratchShaderResourceViewIndex_);
		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				atrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}

	void ShadowRenderer::PrepareFrame(const ShadowRayConstantBuffer& settings)
	{
		/// [JP] ピンポンの交換はここ(1回/フレーム)で行う。Dispatch() は
		///      Editor/Game の両ビューで1フレームに2回呼ばれるため、そちらで
		///      交換すると2回目のバリアがシェーダの書き込み先(PrepareFrame で
		///      確定済みのインデックス)とズレて絵が壊れる。
		historySlot_ = 1 - historySlot_;

		ShadowRayConstantBuffer uploadSettings = settings;
		uploadSettings.frameIndex_ = frameIndex_;
		++frameIndex_;

		dlssRayReconstructionActive_ = uploadSettings.denoiseMode_ == static_cast<Uint32>(ShadowDenoiseMode::DlssRR);

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetShadowRayConstantIndex(tuningBuffer_->GetIndex());

		Uint32 writeSlot = 1 - historySlot_;

		indicesSystem_->SetShadowRawVisibilityUnorderedAccessViewIndex(rawVisibilityUnorderedAccessViewIndex_);
		indicesSystem_->SetShadowRawVisibilityShaderResourceViewIndex(rawVisibilityShaderResourceViewIndex_);

		constexpr Uint32 editorView = static_cast<Uint32>(RaytracingView::Editor);
		constexpr Uint32 gameView = static_cast<Uint32>(RaytracingView::Game);

		if (dlssRayReconstructionActive_)
		{
			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、このビューの
			///      「最終」影読み取りは生の単一バッファテクスチャを直接指す
			///      (ピンポン蓄積チェーンには一切触れない)。
			indicesSystem_->SetEditorShadowIndices(rawVisibilityShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], rawVisibilityShaderResourceViewIndex_);
			indicesSystem_->SetGameShadowIndices(rawVisibilityShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], rawVisibilityShaderResourceViewIndex_);
		}
		else
		{
			indicesSystem_->SetEditorShadowIndices(accumulatedShaderResourceViewIndex_[editorView][historySlot_], accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], accumulatedShaderResourceViewIndex_[editorView][writeSlot]);
			indicesSystem_->SetGameShadowIndices(accumulatedShaderResourceViewIndex_[gameView][historySlot_], accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], accumulatedShaderResourceViewIndex_[gameView][writeSlot]);
		}
	}

	void ShadowRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view)
	{
		auto* cmd = cmdList->Get();

		Uint32 viewIndex = static_cast<Uint32>(view);
		Uint32 writeSlot = 1 - historySlot_;

		ID3D12PipelineState* shadowPipelineState = shadowShader_.GetPipelineState();
		ID3D12PipelineState* denoisePipelineState = denoiseShader_.GetPipelineState();

		/// [JP] DLSS-RR経路ではdenoisePipelineStateの有無を「失敗」扱いしない
		///      (デノイズCS自体を使わないため)。
		Bool denoisePipelineRequired = !dlssRayReconstructionActive_;

		/// [JP] PSO 作成に失敗している（DXR インラインレイトレ非対応ハードウェアなど）
		///      場合、SetPipelineState(nullptr) でクラッシュ/ドライバ異常を起こす
		///      前に安全側（常に照射）へフォールバックする。「影が出ない」原因が
		///      「正常判定の無影」か「異常」かはこのログの有無で切り分けられる。
		if ((!shadowPipelineState || (denoisePipelineRequired && !denoisePipelineState)) && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("ShadowRT/ShadowDenoise のコンピュート PSO 作成に失敗しています。DXR インラインレイトレ(Tier 1.1)非対応の可能性があります。影は常に照射(1.0)として扱われます。");
			pipelineStateMissingLogged_ = true;
		}

		if (!tlasValid || !shadowPipelineState || (denoisePipelineRequired && !denoisePipelineState))
		{
			/// [JP] 追跡対象(TLAS)が無い、または PSO が無いフレーム: 照射(1.0)で
			///      クリアする。composite が実際に読む先(DLSS-RR経路なら生
			///      テクスチャ、通常経路ならピンポン write スロット)をそのまま
			///      クリアする。
			if (dlssRayReconstructionActive_)
			{
				if (rawVisibilityState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					rawVisibilityState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				const Float clearValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(rawVisibilityUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearRawIndex_), rawVisibilityResource_.Get(), clearValues, 0, nullptr);

				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedVisibilityState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float clearValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(accumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearAccumulatedIndex_[viewIndex][writeSlot]), accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), clearValues, 0, nullptr);

			cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			return;
		}
		else
		{
			if (rawVisibilityState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(shadowShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);
			cmd->SetPipelineState(shadowPipelineState);

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			rawVisibilityState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (dlssRayReconstructionActive_)
			{
				/// [JP] DLSS-RRが自身で最終合成フレームをデノイズするので、
				///      このRenderer自身の時間積分(デノイズCS)は丸ごと
				///      スキップする — 生テクスチャを composite が読める状態
				///      (PIXEL_SHADER_RESOURCE)へ遷移させるだけでよい。
				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedVisibilityState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][historySlot_].Get(), accumulatedVisibilityState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				accumulatedVisibilityState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (atrousScratchState_[viewIndex][0] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoisePipelineState);
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (atrousScratchState_[viewIndex][1] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][1].Get(), atrousScratchState_[viewIndex][1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][1] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(0));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(atrousScratchResource_[viewIndex][1].Get(), atrousScratchState_[viewIndex][1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][1] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (atrousScratchState_[viewIndex][0] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(1));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (accumulatedVisibilityState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(2));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}
}
