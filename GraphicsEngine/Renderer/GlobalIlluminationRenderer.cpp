#include <GraphicsEngine/Renderer/GlobalIlluminationRenderer.h>
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
		/// [JP] raw/accumulated 共通のテクスチャ作成ヘルパー。RGBA16F の
		///      UAV+SRV を確保し、ClearUnorderedAccessViewFloat 用の非シェーダ
		///      可視 UAV も併せて作る(AmbientOcclusionRenderer の R16 版と同型)。
		void CreateRadianceTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height,
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
			resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
			SC_HR_CHECK(hr, "GI 放射輝度テクスチャの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

			outClearIndex = clearHeap.AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap.CPUHandle(outClearIndex));

			outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
		}

		/// [JP] A-Trous スクラッチの生成+bindless登録。Create()/Resize() の
		///      両方から呼ぶ(スクラッチは毎フレーム全パスが上書きするので
		///      history/accumulated と違いフレームリング二重化は不要 —
		///      GlobalIlluminationRenderer.h のコメント参照)。clear 用
		///      ディスクリプタは CreateRadianceTexture が仕様上必ず1つ消費
		///      するが(実際にはクリアしない)、clearHeap のサイズ計算に
		///      含めておくこと。
		void CreateAtrousScratchTextures(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height, IndicesSystem& indicesSystem,
			Microsoft::WRL::ComPtr<ID3D12Resource>(&outResource)[2][2], Uint32(&outUnorderedAccessViewIndex)[2][2], Uint32(&outShaderResourceViewIndex)[2][2])
		{
			for (Uint32 view = 0; view < 2; ++view)
			{
				for (Uint32 slot = 0; slot < 2; ++slot)
				{
					Uint32 unusedClearIndex = 0;
					CreateRadianceTexture(device, bindlessHeap, clearHeap, width, height, outResource[view][slot], outUnorderedAccessViewIndex[view][slot], outShaderResourceViewIndex[view][slot], unusedClearIndex);
				}
			}

			indicesSystem.SetEditorGlobalIlluminationAtrousScratchIndices(outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Editor)][0], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Editor)][0], outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Editor)][1], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Editor)][1]);
			indicesSystem.SetGameGlobalIlluminationAtrousScratchIndices(outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Game)][0], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Game)][0], outShaderResourceViewIndex[static_cast<Uint32>(RaytracingView::Game)][1], outUnorderedAccessViewIndex[static_cast<Uint32>(RaytracingView::Game)][1]);
		}
	}

	GlobalIlluminationRenderer::GlobalIlluminationRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject, PipelineStateObject& pipelineStateObject) : globalIlluminationShader_(rootSignature, raytracingStateObject), denoiseShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	/**
	* [EN]
	* Creates the raw radiance target, the ping-ponged per-view accumulated
	* (denoised) targets, the tuning constant buffer, and the 3-record shader
	* table.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生の放射輝度ターゲット、ビューごとのピンポン蓄積(デノイズ済み)
	* ターゲット、チューニング用定数バッファ、3 レコードのシェーダテーブルを
	* 生成する。
	*/
	void GlobalIlluminationRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		width_ = width;
		height_ = height;

		/// [JP] デバイスは常に ID3D12Device5 として生成されている(D3D12Device 参照)。
		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(device);

		globalIlluminationShader_.Create(shaderCache, device5);
		denoiseShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<GlobalIlluminationRayConstantBuffer>>(device, bindlessHeap);

		HRESULT hr{ S_OK };

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount + viewCount * 2, false);

		CreateRadianceTexture(device, bindlessHeap, clearHeap_, width, height, radianceResource_, radianceUnorderedAccessViewIndex_, radianceShaderResourceViewIndex_, clearRawIndex_);
		radianceState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateRadianceTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedRadianceResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedRadianceState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
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

		/// [JP] シェーダテーブル構築。グローバルルートシグネチャのみ(ローカル
		///      ルート引数なし)なので、各レコードは 32 バイトのシェーダ識別子
		///      だけ。識別子はステートオブジェクトから取得する。
		ID3D12StateObject* stateObject = globalIlluminationShader_.GetStateObject();
		if (!stateObject)
		{
			return;
		}

		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		hr = stateObject->QueryInterface(IID_PPV_ARGS(&stateObjectProperties));
		if (FAILED(hr))
		{
			return;
		}

		void* rayGenIdentifier = stateObjectProperties->GetShaderIdentifier(String(GlobalIlluminationShader::rayGenExportName).w_str().c_str());
		void* missIdentifier = stateObjectProperties->GetShaderIdentifier(String(GlobalIlluminationShader::missExportName).w_str().c_str());
		void* hitGroupIdentifier = stateObjectProperties->GetShaderIdentifier(String(GlobalIlluminationShader::hitGroupName).w_str().c_str());
		if (!rayGenIdentifier || !missIdentifier || !hitGroupIdentifier)
		{
			return;
		}

		D3D12_HEAP_PROPERTIES uploadHeapProperties{};
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC tableDesc{};
		tableDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		tableDesc.Width = shaderTableRecordSize * 3;
		tableDesc.Height = 1;
		tableDesc.DepthOrArraySize = 1;
		tableDesc.MipLevels = 1;
		tableDesc.Format = DXGI_FORMAT_UNKNOWN;
		tableDesc.SampleDesc.Count = 1;
		tableDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &tableDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&shaderTableResource_));
		SC_HR_CHECK(hr, "GI シェーダーテーブルリソースの生成に失敗しました");

		Uint8* mapped = nullptr;
		hr = shaderTableResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		SC_HR_CHECK(hr, "GI シェーダーテーブルリソースのMapに失敗しました");
		memset(mapped, 0, shaderTableRecordSize * 3);
		memcpy(mapped + shaderTableRecordSize * 0, rayGenIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		memcpy(mapped + shaderTableRecordSize * 1, missIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		memcpy(mapped + shaderTableRecordSize * 2, hitGroupIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		shaderTableResource_->Unmap(0, nullptr);
	}

	void GlobalIlluminationRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(radianceUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(radianceShaderResourceViewIndex_);
		radianceResource_.Reset();

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				bindlessHeap->FreeIndex(accumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(accumulatedShaderResourceViewIndex_[view][slot]);
				accumulatedRadianceResource_[view][slot].Reset();
			}
		}

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				bindlessHeap->FreeIndex(atrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(atrousScratchShaderResourceViewIndex_[view][slot]);
				atrousScratchResource_[view][slot].Reset();
			}
		}
	}

	void GlobalIlluminationRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * accumulationSlotCount + viewCount * 2, false);

		CreateRadianceTexture(device, bindlessHeap, clearHeap_, width, height, radianceResource_, radianceUnorderedAccessViewIndex_, radianceShaderResourceViewIndex_, clearRawIndex_);
		radianceState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateRadianceTexture(device, bindlessHeap, clearHeap_, width, height, accumulatedRadianceResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], clearAccumulatedIndex_[view][slot]);
				accumulatedRadianceState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
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

	void GlobalIlluminationRenderer::PrepareFrame(const GlobalIlluminationRayConstantBuffer& settings, Bool useDlssRayReconstruction)
	{
		/// [JP] ピンポンの交換はここ(1回/フレーム)で行う。Dispatch() は
		///      Editor/Game の両ビューで1フレームに2回呼ばれる
		///      (AmbientOcclusionRenderer と同じ理由)。
		historySlot_ = 1 - historySlot_;

		GlobalIlluminationRayConstantBuffer uploadSettings = settings;
		uploadSettings.frameIndex_ = frameIndex_;
		++frameIndex_;

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetGlobalIlluminationRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetGlobalIlluminationOutputUnorderedAccessViewIndex(radianceUnorderedAccessViewIndex_);
		indicesSystem_->SetGlobalIlluminationOutputShaderResourceViewIndex(radianceShaderResourceViewIndex_);

		Uint32 writeSlot = 1 - historySlot_;

		constexpr Uint32 editorView = static_cast<Uint32>(RaytracingView::Editor);
		constexpr Uint32 gameView = static_cast<Uint32>(RaytracingView::Game);

		if (useDlssRayReconstruction)
		{
			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、このビューの
			///      「最終」GI読み取りは生の単一バッファテクスチャを直接指す
			///      (ピンポン蓄積チェーンには一切触れない)。
			indicesSystem_->SetEditorGlobalIlluminationAccumulationIndices(radianceShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], radianceShaderResourceViewIndex_);
			indicesSystem_->SetGameGlobalIlluminationAccumulationIndices(radianceShaderResourceViewIndex_, accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], radianceShaderResourceViewIndex_);
		}
		else
		{
			indicesSystem_->SetEditorGlobalIlluminationAccumulationIndices(accumulatedShaderResourceViewIndex_[editorView][historySlot_], accumulatedUnorderedAccessViewIndex_[editorView][writeSlot], accumulatedShaderResourceViewIndex_[editorView][writeSlot]);
			indicesSystem_->SetGameGlobalIlluminationAccumulationIndices(accumulatedShaderResourceViewIndex_[gameView][historySlot_], accumulatedUnorderedAccessViewIndex_[gameView][writeSlot], accumulatedShaderResourceViewIndex_[gameView][writeSlot]);
		}
	}

	void GlobalIlluminationRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction)
	{
		auto* cmd = cmdList->Get();

		Uint32 viewIndex = static_cast<Uint32>(view);
		Uint32 writeSlot = 1 - historySlot_;

		ID3D12StateObject* stateObject = globalIlluminationShader_.GetStateObject();
		ID3D12PipelineState* denoisePipelineState = denoiseShader_.GetPipelineState();

		/// [JP] DLSS-RR経路ではdenoisePipelineState/atrousPipelineStatesの有無を
		///      「失敗」扱いしない(デノイズ/A-Trous CS自体を使わないため)。
		///      RTPSO/シェーダテーブルの有無だけがGI自体の成否を決める。
		Bool denoisePipelineRequired = !useDlssRayReconstruction;
		Bool denoisePipelineMissing = !denoisePipelineState || !denoiseShader_.GetATrousPipelineState(0) || !denoiseShader_.GetATrousPipelineState(1) || !denoiseShader_.GetATrousPipelineState(2);

		if ((!stateObject || !shaderTableResource_ || (denoisePipelineRequired && denoisePipelineMissing)) && !stateObjectMissingLogged_)
		{
			SC_LOG_WARNING("GlobalIlluminationRT/GlobalIlluminationDenoise の RTPSO/PSO/シェーダテーブル作成に失敗しています。DXR(DispatchRays)非対応の可能性があります。間接光は常に無し(0)として扱われます。");
			stateObjectMissingLogged_ = true;
		}

		if (!tlasValid || !stateObject || !shaderTableResource_ || (denoisePipelineRequired && denoisePipelineMissing))
		{
			/// [JP] 追跡対象(TLAS)が無い、GI が無効、または RTPSO/PSO が無い
			///      フレーム: 間接光無し(0)でクリアする。composite が実際に
			///      読む先(DLSS-RR経路なら生テクスチャ、通常経路ならピンポン
			///      write スロット)をそのままクリアする — 逆側をクリアしても
			///      composite からは見えないため。
			if (useDlssRayReconstruction)
			{
				if (radianceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					radianceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(radianceUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearRawIndex_), radianceResource_.Get(), clearValues, 0, nullptr);

				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				radianceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedRadianceState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(accumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearAccumulatedIndex_[viewIndex][writeSlot]), accumulatedRadianceResource_[viewIndex][writeSlot].Get(), clearValues, 0, nullptr);

			cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			return;
		}
		else
		{
			if (radianceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				radianceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			/// [JP] DispatchRays のルート引数はコンピュートのバインドポイントを使う。
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(globalIlluminationShader_.GetRootSignature());
			cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
			cmd->SetComputeRootConstantBufferView(2, constantIndex);
			cmd->SetComputeRootConstantBufferView(3, structuredIndex);
			cmd->SetPipelineState1(stateObject);

			D3D12_GPU_VIRTUAL_ADDRESS tableAddress = shaderTableResource_->GetGPUVirtualAddress();

			D3D12_DISPATCH_RAYS_DESC dispatchDesc{};
			dispatchDesc.RayGenerationShaderRecord.StartAddress = tableAddress + shaderTableRecordSize * 0;
			dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderTableRecordSize;
			dispatchDesc.MissShaderTable.StartAddress = tableAddress + shaderTableRecordSize * 1;
			dispatchDesc.MissShaderTable.SizeInBytes = shaderTableRecordSize;
			dispatchDesc.MissShaderTable.StrideInBytes = shaderTableRecordSize;
			dispatchDesc.HitGroupTable.StartAddress = tableAddress + shaderTableRecordSize * 2;
			dispatchDesc.HitGroupTable.SizeInBytes = shaderTableRecordSize;
			dispatchDesc.HitGroupTable.StrideInBytes = shaderTableRecordSize;
			dispatchDesc.Width = width_;
			dispatchDesc.Height = height_;
			dispatchDesc.Depth = 1;

			cmd->DispatchRays(&dispatchDesc);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			radianceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (useDlssRayReconstruction)
			{
				/// [JP] DLSS-RRが自身で最終合成フレームをデノイズするので、
				///      このRenderer自身の時間的蓄積(デノイズCS)は丸ごと
				///      スキップする — 生テクスチャを composite が読める状態
				///      (PIXEL_SHADER_RESOURCE)へ遷移させるだけでよい。
				///      ピンポン蓄積チェーンには一切触れない。
				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				radianceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				return;
			}

			if (accumulatedRadianceState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(accumulatedRadianceResource_[viewIndex][historySlot_].Get(), accumulatedRadianceState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				accumulatedRadianceState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			/// [JP] denoiseShader_ は globalIlluminationShader_ と同じ共有
			///      ルートシグネチャ(コンストラクタ引数の rootSignature)を使う
			///      ので、ルート引数の再設定は不要 — PSO だけ差し替える
			///      (AmbientOcclusionRenderer と同じ)。
			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;

			/// [JP] 時間ブレンド(main())は、もう accumulated write スロットへ
			///      直接書かない — A-Trous スクラッチ0へ書き、この後の3パスが
			///      さらに空間フィルタしてから最終的に accumulated write
			///      スロットへ書く(GlobalIlluminationDenoiseCS.hlsl 参照)。
			if (atrousScratchState_[viewIndex][0] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoisePipelineState);
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			/// [JP] A-Trous パス1: スクラッチ0(読み)→スクラッチ1(書き)、step=1。
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

			/// [JP] A-Trous パス2: スクラッチ1(読み)→スクラッチ0(書き)、step=2。
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

			/// [JP] A-Trous パス3(最終): スクラッチ0(読み)→今フレームの
			///      accumulated write スロット(書き)、step=4。この結果が今
			///      フレーム composite が読む値であり、次フレームの時間ブレンド
			///      が history として読む値にもなる。
			cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (accumulatedRadianceState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(2));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}
}
