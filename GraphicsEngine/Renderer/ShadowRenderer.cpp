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
		/**
		* [EN]
		* Creates one screen-sized UAV+SRV texture of the given format, plus an
		* optional non-shader-visible UAV for ClearUnorderedAccessViewFloat.
		* Every buffer in the SVGF chain differs only in format, so they all go
		* through here.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定フォーマットで画面サイズの UAV+SRV テクスチャを1枚作る。併せて
		* ClearUnorderedAccessViewFloat 用の非シェーダ可視 UAV も(必要なら)作る。
		* SVGF チェーンの各バッファはフォーマットが違うだけなので、全てここを通す。
		*/
		void CreateShadowTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height, DXGI_FORMAT format,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex, Uint32* outClearIndex)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = width;
			resourceDesc.Height = height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = format;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outResource));
			SC_HR_CHECK(hr, "シャドウデノイズ用テクスチャの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = format;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			outUnorderedAccessViewIndex = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(outUnorderedAccessViewIndex));

			if (outClearIndex)
			{
				*outClearIndex = clearHeap.AllocateIndex();
				device->CreateUnorderedAccessView(outResource.Get(), nullptr, &unorderedAccessViewDesc, clearHeap.CPUHandle(*outClearIndex));
			}

			outShaderResourceViewIndex = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = format;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(outResource.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(outShaderResourceViewIndex));
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

		shadowShader_.Create(shaderCache, device);
		denoiseShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<ShadowRayConstantBuffer>>(device, bindlessHeap);

		CreateResources(device, bindlessHeap, width, height);
	}

	/**
	* [EN]
	* Allocates the raw texture and, per view, the whole SVGF chain: the
	* illumination/variance history, the moments, the history length, the packed
	* depth+normal copy, the two A-Trous scratch buffers and the final denoised
	* output. Shared by Create() and Resize() so the two can never drift apart.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* raw テクスチャと、ビューごとの SVGF チェーン一式を確保する: 輝度/分散の
	* 履歴、モーメント、履歴長、深度+法線のパック済みコピー、A-Trous スクラッチ
	* 2枚、最終 denoised 出力。Create() と Resize() で共有し、両者がずれないように
	* する。
	*/
	void ShadowRenderer::CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount + viewCount * accumulationSlotCount, false);

		CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, rawVisibilityResource_, rawVisibilityUnorderedAccessViewIndex_, rawVisibilityShaderResourceViewIndex_, &clearRawIndex_);
		rawVisibilityState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, accumulatedVisibilityResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], nullptr);
				accumulatedVisibilityState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, momentsResource_[view][slot], momentsUnorderedAccessViewIndex_[view][slot], momentsShaderResourceViewIndex_[view][slot], nullptr);
				momentsState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16_FLOAT, historyLengthResource_[view][slot], historyLengthUnorderedAccessViewIndex_[view][slot], historyLengthShaderResourceViewIndex_[view][slot], &clearHistoryLengthIndex_[view][slot]);
				historyLengthState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				/// [JP] ここだけ 32bit。ビュー深度を FP16 に丸めると、far=1000 の
				///      シーンでは view_z 150 付近から量子化幅(0.125)が下の
				///      再投影の深度許容量を上回り、面が一致していても格納精度
				///      だけで履歴が棄却されるようになる(A-Trous の深度重みも
				///      同時に全タップ 0 へ潰れる)。SVGF の深度テストは
				///      「勾配を単位とした差」を見る以上、深度側の分解能が
				///      勾配より粗いと成立しない。
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, depthNormalResource_[view][slot], depthNormalUnorderedAccessViewIndex_[view][slot], depthNormalShaderResourceViewIndex_[view][slot], nullptr);
				depthNormalState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, atrousScratchResource_[view][slot], atrousScratchUnorderedAccessViewIndex_[view][slot], atrousScratchShaderResourceViewIndex_[view][slot], nullptr);
				atrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, denoisedResource_[view], denoisedUnorderedAccessViewIndex_[view], denoisedShaderResourceViewIndex_[view], &clearDenoisedIndex_[view]);
			denoisedState_[view] = D3D12_RESOURCE_STATE_COMMON;
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

				bindlessHeap->FreeIndex(momentsUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(momentsShaderResourceViewIndex_[view][slot]);
				momentsResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(historyLengthUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(historyLengthShaderResourceViewIndex_[view][slot]);
				historyLengthResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(depthNormalUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(depthNormalShaderResourceViewIndex_[view][slot]);
				depthNormalResource_[view][slot].Reset();
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				bindlessHeap->FreeIndex(atrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(atrousScratchShaderResourceViewIndex_[view][slot]);
				atrousScratchResource_[view][slot].Reset();
			}

			bindlessHeap->FreeIndex(denoisedUnorderedAccessViewIndex_[view]);
			bindlessHeap->FreeIndex(denoisedShaderResourceViewIndex_[view]);
			denoisedResource_[view].Reset();
		}
	}

	void ShadowRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		CreateResources(device, bindlessHeap, width, height);
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

		indicesSystem_->SetShadowRawVisibilityUnorderedAccessViewIndex(rawVisibilityUnorderedAccessViewIndex_);
		indicesSystem_->SetShadowRawVisibilityShaderResourceViewIndex(rawVisibilityShaderResourceViewIndex_);

		constexpr Uint32 editorView = static_cast<Uint32>(RaytracingView::Editor);
		constexpr Uint32 gameView = static_cast<Uint32>(RaytracingView::Game);

		Uint32 writeSlot = 1 - historySlot_;

		/// [JP] フレームをまたぐ状態を持つバッファは全て history スロットを読んで
		///      もう片方へ書く。1組の historySlot_/writeSlot が輝度・モーメント・
		///      履歴長・深度法線コピーをまとめて駆動する — これらがずれると、
		///      あるフレームの幾何で整合性を判定しながら別のフレームの輝度を
		///      ブレンドすることになる。
		auto buildIndices = [&](Uint32 viewIndex)
		{
			ShadowAccumulationIndices values{};

			values.historyShaderResourceViewIndex_ = accumulatedShaderResourceViewIndex_[viewIndex][historySlot_];
			values.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot];
			values.accumulatedShaderResourceViewIndex_ = accumulatedShaderResourceViewIndex_[viewIndex][writeSlot];

			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、その間だけ
			///      「最終」影読み取りは生の単一バッファテクスチャを直接指す
			///      (SVGFチェーンには一切触れない)。
			values.visibilityShaderResourceViewIndex_ = dlssRayReconstructionActive_ ? rawVisibilityShaderResourceViewIndex_ : denoisedShaderResourceViewIndex_[viewIndex];

			values.atrousScratch0ShaderResourceViewIndex_ = atrousScratchShaderResourceViewIndex_[viewIndex][0];
			values.atrousScratch0UnorderedAccessViewIndex_ = atrousScratchUnorderedAccessViewIndex_[viewIndex][0];
			values.atrousScratch1ShaderResourceViewIndex_ = atrousScratchShaderResourceViewIndex_[viewIndex][1];
			values.atrousScratch1UnorderedAccessViewIndex_ = atrousScratchUnorderedAccessViewIndex_[viewIndex][1];

			values.momentsHistoryShaderResourceViewIndex_ = momentsShaderResourceViewIndex_[viewIndex][historySlot_];
			values.momentsShaderResourceViewIndex_ = momentsShaderResourceViewIndex_[viewIndex][writeSlot];
			values.momentsUnorderedAccessViewIndex_ = momentsUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.historyLengthHistoryShaderResourceViewIndex_ = historyLengthShaderResourceViewIndex_[viewIndex][historySlot_];
			values.historyLengthShaderResourceViewIndex_ = historyLengthShaderResourceViewIndex_[viewIndex][writeSlot];
			values.historyLengthUnorderedAccessViewIndex_ = historyLengthUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.depthNormalHistoryShaderResourceViewIndex_ = depthNormalShaderResourceViewIndex_[viewIndex][historySlot_];
			values.depthNormalShaderResourceViewIndex_ = depthNormalShaderResourceViewIndex_[viewIndex][writeSlot];
			values.depthNormalUnorderedAccessViewIndex_ = depthNormalUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.denoisedUnorderedAccessViewIndex_ = denoisedUnorderedAccessViewIndex_[viewIndex];

			return values;
		};

		indicesSystem_->SetEditorShadowIndices(buildIndices(editorView));
		indicesSystem_->SetGameShadowIndices(buildIndices(gameView));
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
			///      テクスチャ、通常経路なら denoised 出力)をそのままクリアする。
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

			if (denoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float litValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(denoisedUnorderedAccessViewIndex_[viewIndex]), clearHeap_.CPUHandle(clearDenoisedIndex_[viewIndex]), denoisedResource_[viewIndex].Get(), litValues, 0, nullptr);

			cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			/// [JP] 履歴長も 0 にしておく。こうしないと、次に実際にトレースが走った
			///      フレームで「長い履歴がある」と誤認し、クリア中の無関係な輝度を
			///      重く信用してしまう。
			if (historyLengthState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(historyLengthResource_[viewIndex][writeSlot].Get(), historyLengthState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				historyLengthState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float zeroValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(historyLengthUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearHistoryLengthIndex_[viewIndex][writeSlot]), historyLengthResource_[viewIndex][writeSlot].Get(), zeroValues, 0, nullptr);
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
				///      このRenderer自身のSVGFチェーンは丸ごとスキップする —
				///      生テクスチャを composite が読める状態
				///      (PIXEL_SHADER_RESOURCE)へ遷移させるだけでよい。
				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				return;
			}

			/// [JP] リプロジェクションが読む履歴側(輝度/モーメント/履歴長/
			///      深度法線)をまとめて読み取り状態へ。
			if (accumulatedVisibilityState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][historySlot_].Get(), accumulatedVisibilityState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				accumulatedVisibilityState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (momentsState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(momentsResource_[viewIndex][historySlot_].Get(), momentsState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				momentsState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (historyLengthState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(historyLengthResource_[viewIndex][historySlot_].Get(), historyLengthState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				historyLengthState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (depthNormalState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(depthNormalResource_[viewIndex][historySlot_].Get(), depthNormalState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				depthNormalState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			/// [JP] パス1(リプロジェクション): raw + 履歴 → scratch0 と、
			///      今フレームのモーメント/履歴長/深度法線。
			if (atrousScratchState_[viewIndex][0] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			if (momentsState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(momentsResource_[viewIndex][writeSlot].Get(), momentsState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				momentsState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			if (historyLengthState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(historyLengthResource_[viewIndex][writeSlot].Get(), historyLengthState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				historyLengthState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			if (depthNormalState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(depthNormalResource_[viewIndex][writeSlot].Get(), depthNormalState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				depthNormalState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoisePipelineState);
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			/// [JP] 以降のパスはモーメント/履歴長/深度法線を読むだけなので、
			///      ここで一度だけ読み取り状態へ落として最後まで据え置く。
			cmdList->Barrier(momentsResource_[viewIndex][writeSlot].Get(), momentsState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			momentsState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			cmdList->Barrier(historyLengthResource_[viewIndex][writeSlot].Get(), historyLengthState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			historyLengthState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			cmdList->Barrier(depthNormalResource_[viewIndex][writeSlot].Get(), depthNormalState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			depthNormalState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			/// [JP] パス2(FilterMoments): scratch0 → scratch1。
			if (atrousScratchState_[viewIndex][1] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][1].Get(), atrousScratchState_[viewIndex][1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][1] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetFilterMomentsPipelineState());
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(atrousScratchResource_[viewIndex][1].Get(), atrousScratchState_[viewIndex][1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][1] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			/// [JP] パス3(A-Trous step1): scratch1 → scratch0。
			if (atrousScratchState_[viewIndex][0] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(0));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(atrousScratchResource_[viewIndex][0].Get(), atrousScratchState_[viewIndex][0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			atrousScratchState_[viewIndex][0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			/// [JP] パス4(A-Trous step2 = フィードバックタップ): scratch0 →
			///      history write スロット。これが次フレームの履歴になる。
			if (accumulatedVisibilityState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(1));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(accumulatedVisibilityResource_[viewIndex][writeSlot].Get(), accumulatedVisibilityState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			accumulatedVisibilityState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			/// [JP] パス5(A-Trous step4): history write スロット → denoised 出力。
			if (denoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(2));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			/// [JP] 生テクスチャも最後にピクセルシェーダから読める状態へ戻す。
			///      デノイズチェーンが読むのは NON_PIXEL 状態で足りるが、
			///      ViewMode の「シャドウ（生）」表示は DeferredLightingPS
			///      ＝ピクセルシェーダから読むため、その状態のままだと不正な
			///      リソース状態での読み取りになり、表示される値が信用できない。
			cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			rawVisibilityState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}
}
