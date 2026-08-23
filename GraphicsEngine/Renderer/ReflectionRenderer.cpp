#include <GraphicsEngine/Renderer/ReflectionRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Buffer/ReservoirBuffer.h>
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
		* SVGF チェーンの各バッファはフォーマットが違うだけなので、全てここを
		* 通す。
		*/
		void CreateReflectionTexture(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 width, Uint32 height, DXGI_FORMAT format,
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
			SC_HR_CHECK(hr, "反射デノイズ用テクスチャの生成に失敗しました");

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

	ReflectionRenderer::ReflectionRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject, PipelineStateObject& pipelineStateObject) : reflectionShader_(rootSignature, raytracingStateObject), denoiseShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	/**
	* [EN]
	* Creates the raw radiance target, the per-view SVGF chain, the instance
	* table, the tuning constant buffer, and the 3-record shader table.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生の放射輝度ターゲット、ビューごとの SVGF チェーン、インスタンス
	* テーブル、チューニング用定数バッファ、3 レコードのシェーダテーブルを
	* 生成する。
	*/
	void ReflectionRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		/// [JP] デバイスは常に ID3D12Device5 として生成されている(D3D12Device 参照)。
		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(device);

		reflectionShader_.Create(shaderCache, device5);
		denoiseShader_.Create(shaderCache, device);

		tuningBuffer_ = MakePtr<ConstantBuffer<ReflectionRayConstantBuffer>>(device, bindlessHeap);
		instanceTable_ = MakePtr<ReadOnlyStructuredBuffer<ReflectionInstanceData>>(device, bindlessHeap, maxInstances);

		HRESULT hr{ S_OK };

		CreateResources(device, bindlessHeap, width, height);

		/// [JP] シェーダテーブル構築。グローバルルートシグネチャのみ(ローカル
		///      ルート引数なし)なので、各レコードは 32 バイトのシェーダ識別子
		///      だけ。識別子はステートオブジェクトから取得する。
		ID3D12StateObject* stateObject = reflectionShader_.GetStateObject();
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

		void* rayGenIdentifier = stateObjectProperties->GetShaderIdentifier(String(ReflectionShader::rayGenExportName).w_str().c_str());
		void* missIdentifier = stateObjectProperties->GetShaderIdentifier(String(ReflectionShader::missExportName).w_str().c_str());
		void* hitGroupIdentifier = stateObjectProperties->GetShaderIdentifier(String(ReflectionShader::hitGroupName).w_str().c_str());
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
		SC_HR_CHECK(hr, "シェーダーテーブルリソースの生成に失敗しました");

		Uint8* mapped = nullptr;
		hr = shaderTableResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		SC_HR_CHECK(hr, "シェーダーテーブルリソースのMapに失敗しました");
		memset(mapped, 0, shaderTableRecordSize * 3);
		memcpy(mapped + shaderTableRecordSize * 0, rayGenIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		memcpy(mapped + shaderTableRecordSize * 1, missIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		memcpy(mapped + shaderTableRecordSize * 2, hitGroupIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		shaderTableResource_->Unmap(0, nullptr);
	}

	/**
	* [EN]
	* Allocates the raw texture and, per view, the whole SVGF chain: the
	* radiance/variance history, the moments, the history length, the packed
	* depth+normal copy, the two A-Trous scratch buffers and the final denoised
	* output. Shared by Create() and Resize() so the two can never drift apart.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* raw テクスチャと、ビューごとの SVGF チェーン一式を確保する: 放射輝度/分散の
	* 履歴、モーメント、履歴長、深度+法線のパック済みコピー、A-Trous スクラッチ
	* 2枚、最終 denoised 出力。Create() と Resize() で共有し、両者がずれないように
	* する。
	*/
	void ReflectionRenderer::CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + 1 + viewCount + viewCount * accumulationSlotCount * 4 + viewCount * accumulationSlotCount, false);

		historyCleared_ = false;

		CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, radianceResource_, radianceUnorderedAccessViewIndex_, radianceShaderResourceViewIndex_, &clearRawIndex_);
		radianceState_ = D3D12_RESOURCE_STATE_COMMON;

		CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16_FLOAT, confidenceResource_, confidenceUnorderedAccessViewIndex_, confidenceShaderResourceViewIndex_, &clearConfidenceIndex_);
		confidenceState_ = D3D12_RESOURCE_STATE_COMMON;

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, accumulatedRadianceResource_[view][slot], accumulatedUnorderedAccessViewIndex_[view][slot], accumulatedShaderResourceViewIndex_[view][slot], &clearAccumulatedIndex_[view][slot]);
				accumulatedRadianceState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				ReservoirBuffer::Create(device, bindlessHeap, clearHeap_, width * height, reservoirElementSizeInBytes_, reservoirResource_[view][slot], reservoirUnorderedAccessViewIndex_[view][slot], reservoirShaderResourceViewIndex_[view][slot], clearReservoirIndex_[view][slot]);
				reservoirState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, momentsResource_[view][slot], momentsUnorderedAccessViewIndex_[view][slot], momentsShaderResourceViewIndex_[view][slot], &clearMomentsIndex_[view][slot]);
				momentsState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16_FLOAT, historyLengthResource_[view][slot], historyLengthUnorderedAccessViewIndex_[view][slot], historyLengthShaderResourceViewIndex_[view][slot], &clearHistoryLengthIndex_[view][slot]);
				historyLengthState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				/// [JP] ここだけ 32bit。ビュー深度を FP16 に丸めると、far=1000 の
				///      シーンでは view_z 150 付近から量子化幅(0.125)が下の
				///      再投影の深度許容量を上回り、面が一致していても格納精度
				///      だけで履歴が棄却されるようになる(A-Trous の深度重みも
				///      同時に全タップ 0 へ潰れる)。SVGF の深度テストは
				///      「勾配を単位とした差」を見る以上、深度側の分解能が
				///      勾配より粗いと成立しない。
				CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, depthNormalResource_[view][slot], depthNormalUnorderedAccessViewIndex_[view][slot], depthNormalShaderResourceViewIndex_[view][slot], &clearDepthNormalIndex_[view][slot]);
				depthNormalState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, atrousScratchResource_[view][slot], atrousScratchUnorderedAccessViewIndex_[view][slot], atrousScratchShaderResourceViewIndex_[view][slot], nullptr);
				atrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			CreateReflectionTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, denoisedResource_[view], denoisedUnorderedAccessViewIndex_[view], denoisedShaderResourceViewIndex_[view], &clearDenoisedIndex_[view]);
			denoisedState_[view] = D3D12_RESOURCE_STATE_COMMON;
		}
	}

	void ReflectionRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(radianceUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(radianceShaderResourceViewIndex_);

		/// [JP] Resize() はこれを呼んだ直後に同じ幅/高さで作り直すが、その時点で
		///      前フレームのGPUコマンドがまだこのリソースを読み書きしている
		///      可能性がある(OITBuffer::Destroyと同じ理由)。即座に.Reset()すると
		///      解放直後のメモリへ新しいリソースが再割り当てされ、GPU側が古い
		///      コマンドで新リソースを踏みに行く事故になる - 遅延破棄する。
		bindlessHeap->DeferRelease(radianceResource_);
		radianceResource_.Reset();

		bindlessHeap->FreeIndex(confidenceUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(confidenceShaderResourceViewIndex_);
		bindlessHeap->DeferRelease(confidenceResource_);
		confidenceResource_.Reset();

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				bindlessHeap->FreeIndex(accumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(accumulatedShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(accumulatedRadianceResource_[view][slot]);
				accumulatedRadianceResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(reservoirUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(reservoirShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(reservoirResource_[view][slot]);
				reservoirResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(momentsUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(momentsShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(momentsResource_[view][slot]);
				momentsResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(historyLengthUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(historyLengthShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(historyLengthResource_[view][slot]);
				historyLengthResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(depthNormalUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(depthNormalShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(depthNormalResource_[view][slot]);
				depthNormalResource_[view][slot].Reset();
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				bindlessHeap->FreeIndex(atrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(atrousScratchShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(atrousScratchResource_[view][slot]);
				atrousScratchResource_[view][slot].Reset();
			}

			bindlessHeap->FreeIndex(denoisedUnorderedAccessViewIndex_[view]);
			bindlessHeap->FreeIndex(denoisedShaderResourceViewIndex_[view]);
			bindlessHeap->DeferRelease(denoisedResource_[view]);
			denoisedResource_[view].Reset();
		}
	}

	void ReflectionRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);

		CreateResources(device, bindlessHeap, width, height);
	}

	void ReflectionRenderer::UpdateInstanceTable(const ReflectionInstanceData* data, Uint32 count)
	{
		instanceTable_->Update(data, count < maxInstances ? count : maxInstances);
	}

	void ReflectionRenderer::PrepareFrame(const ReflectionRayConstantBuffer& settings, Bool useDlssRayReconstruction)
	{
		/// [JP] ピンポンの交換はここ(1回/フレーム)で行う。Dispatch() は
		///      Editor/Game の両ビューで1フレームに2回呼ばれる
		///      (GlobalIlluminationRenderer と同じ理由)。
		historySlot_ = 1 - historySlot_;

		ReflectionRayConstantBuffer uploadSettings = settings;
		uploadSettings.frameIndex_ = frameIndex_;
		++frameIndex_;

		tuningBuffer_->Update(uploadSettings);
		indicesSystem_->SetReflectionRayConstantIndex(tuningBuffer_->GetIndex());
		indicesSystem_->SetReflectionOutputUnorderedAccessViewIndex(radianceUnorderedAccessViewIndex_);
		indicesSystem_->SetReflectionOutputShaderResourceViewIndex(radianceShaderResourceViewIndex_);
		indicesSystem_->SetReflectionConfidenceUnorderedAccessViewIndex(confidenceUnorderedAccessViewIndex_);
		indicesSystem_->SetReflectionConfidenceShaderResourceViewIndex(confidenceShaderResourceViewIndex_);

		/// [JP] フレームリングバッファなので SRV インデックスは毎フレーム変わる
		///      — 必ず毎フレーム登録し直す(frame-ring-rules)。
		indicesSystem_->SetReflectionInstanceDataIndex(instanceTable_->Index());

		Uint32 writeSlot = 1 - historySlot_;

		constexpr Uint32 editorView = static_cast<Uint32>(RaytracingView::Editor);
		constexpr Uint32 gameView = static_cast<Uint32>(RaytracingView::Game);

		/// [JP] フレームをまたぐ状態を持つバッファ(放射輝度+分散、モーメント、
		///      履歴長、深度法線コピー)は全て history スロットを読んでもう片方へ
		///      書く。1組の historySlot_/writeSlot がまとめて駆動する — ずれると、
		///      あるフレームの幾何で整合性を判定しながら別のフレームの放射輝度を
		///      ブレンドすることになる。
		auto buildIndices = [&](Uint32 viewIndex)
		{
			ReflectionAccumulationIndices values{};

			values.historyShaderResourceViewIndex_ = accumulatedShaderResourceViewIndex_[viewIndex][historySlot_];
			values.accumulatedUnorderedAccessViewIndex_ = accumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot];
			values.accumulatedShaderResourceViewIndex_ = accumulatedShaderResourceViewIndex_[viewIndex][writeSlot];

			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、その間だけ
			///      「最終」反射読み取りは生の単一バッファテクスチャを直接指す
			///      (SVGFチェーンには一切触れない)。
			values.radianceShaderResourceViewIndex_ = useDlssRayReconstruction ? radianceShaderResourceViewIndex_ : denoisedShaderResourceViewIndex_[viewIndex];

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

			/// [JP] ReSTIR Reservoir はデノイズ経路(SVGF/DLSS-RR)に関わらず常に
			///      使う — history_/accumulated_ と同じ historySlot_/writeSlot
			///      (GlobalIlluminationRenderer と同じ扱い)。
			values.reservoirHistoryShaderResourceViewIndex_ = reservoirShaderResourceViewIndex_[viewIndex][historySlot_];
			values.reservoirUnorderedAccessViewIndex_ = reservoirUnorderedAccessViewIndex_[viewIndex][writeSlot];
			values.reservoirWriteShaderResourceViewIndex_ = reservoirShaderResourceViewIndex_[viewIndex][writeSlot];

			return values;
		};

		indicesSystem_->SetEditorReflectionAccumulationIndices(buildIndices(editorView));
		indicesSystem_->SetGameReflectionAccumulationIndices(buildIndices(gameView));
	}

	void ReflectionRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction)
	{
		auto* cmd = cmdList->Get();

		Uint32 viewIndex = static_cast<Uint32>(view);
		Uint32 writeSlot = 1 - historySlot_;

		/// [JP] 履歴チェーンの一括ゼロクリア。生成直後の1回だけ、全ビュー・全
		///      スロットをまとめて潰す。ここを通さないと未初期化のビットパターンが
		///      履歴として読み戻され、そのまま自己再投入されて焼き付く。
		if (!historyCleared_)
		{
			historyCleared_ = true;

			ID3D12DescriptorHeap* clearHeaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(clearHeaps), clearHeaps);

			const Float zeroValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			auto clearTexture = [&](Microsoft::WRL::ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES& state, Uint32 unorderedAccessViewIndex, Uint32 clearIndex)
			{
				if (state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(resource.Get(), state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(unorderedAccessViewIndex), clearHeap_.CPUHandle(clearIndex), resource.Get(), zeroValues, 0, nullptr);
			};

			const Uint32 zeroReservoirValues[4] = { 0, 0, 0, 0 };

			for (Uint32 clearView = 0; clearView < viewCount; ++clearView)
			{
				for (Uint32 clearSlot = 0; clearSlot < accumulationSlotCount; ++clearSlot)
				{
					clearTexture(accumulatedRadianceResource_[clearView][clearSlot], accumulatedRadianceState_[clearView][clearSlot], accumulatedUnorderedAccessViewIndex_[clearView][clearSlot], clearAccumulatedIndex_[clearView][clearSlot]);
					clearTexture(momentsResource_[clearView][clearSlot], momentsState_[clearView][clearSlot], momentsUnorderedAccessViewIndex_[clearView][clearSlot], clearMomentsIndex_[clearView][clearSlot]);
					clearTexture(historyLengthResource_[clearView][clearSlot], historyLengthState_[clearView][clearSlot], historyLengthUnorderedAccessViewIndex_[clearView][clearSlot], clearHistoryLengthIndex_[clearView][clearSlot]);
					clearTexture(depthNormalResource_[clearView][clearSlot], depthNormalState_[clearView][clearSlot], depthNormalUnorderedAccessViewIndex_[clearView][clearSlot], clearDepthNormalIndex_[clearView][clearSlot]);

					if (reservoirState_[clearView][clearSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					{
						cmdList->Barrier(reservoirResource_[clearView][clearSlot].Get(), reservoirState_[clearView][clearSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
						reservoirState_[clearView][clearSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					}

					cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(reservoirUnorderedAccessViewIndex_[clearView][clearSlot]), clearHeap_.CPUHandle(clearReservoirIndex_[clearView][clearSlot]), reservoirResource_[clearView][clearSlot].Get(), zeroReservoirValues, 0, nullptr);
				}

				clearTexture(denoisedResource_[clearView], denoisedState_[clearView], denoisedUnorderedAccessViewIndex_[clearView], clearDenoisedIndex_[clearView]);
			}
		}

		ID3D12StateObject* stateObject = reflectionShader_.GetStateObject();
		ID3D12PipelineState* denoisePipelineState = denoiseShader_.GetPipelineState();

		/// [JP] DLSS-RR経路ではdenoisePipelineState等の有無を「失敗」扱いしない
		///      (デノイズCS自体を使わないため)。RTPSO/シェーダテーブルの有無
		///      だけが反射自体の成否を決める。
		Bool denoisePipelineRequired = !useDlssRayReconstruction;
		Bool denoisePipelineMissing = !denoisePipelineState || !denoiseShader_.GetFilterMomentsPipelineState() || !denoiseShader_.GetATrousPipelineState(0) || !denoiseShader_.GetATrousPipelineState(1) || !denoiseShader_.GetATrousPipelineState(2);

		/// [JP] 空間的リユースパスは DLSS-RR 経路でも走る(どちらのデノイザに
		///      入る前段の生信号を綺麗にするため)ので、denoisePipelineRequired
		///      に関わらず常に必須として扱う(GlobalIlluminationRenderer と同じ)。
		Bool spatialReusePipelineMissing = !denoiseShader_.GetSpatialReusePipelineState();

		if ((!stateObject || !shaderTableResource_ || spatialReusePipelineMissing || (denoisePipelineRequired && denoisePipelineMissing)) && !stateObjectMissingLogged_)
		{
			SC_LOG_WARNING("ReflectionRT/ReflectionDenoise の RTPSO/PSO/シェーダテーブル作成に失敗しています。DXR(DispatchRays)非対応の可能性があります。反射は常に無し(0)として扱われます。");
			stateObjectMissingLogged_ = true;
		}

		if (!tlasValid || !stateObject || !shaderTableResource_ || spatialReusePipelineMissing || (denoisePipelineRequired && denoisePipelineMissing))
		{
			/// [JP] 追跡対象(TLAS)が無い、反射が無効、または RTPSO/PSO が無い
			///      フレーム: 反射無し(0)でクリアする。composite が実際に
			///      読む先(DLSS-RR経路なら生テクスチャ、通常経路なら denoised
			///      出力)をそのままクリアする — 逆側をクリアしても composite
			///      からは見えないため。
			/// [JP] このビューの今フレーム write スロットの Reservoir も合わせて
			///      クリアする — 反射が無効な間の古い M_/W_ を残すと、後で
			///      再有効化した時に何フレームも前のゴミ Reservoir をリサンプル
			///      してしまう(GlobalIlluminationRenderer と同じ理由)。
			if (reservoirState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(reservoirResource_[viewIndex][writeSlot].Get(), reservoirState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				reservoirState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Uint32 zeroReservoirValues[4] = { 0, 0, 0, 0 };
			cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(reservoirUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearReservoirIndex_[viewIndex][writeSlot]), reservoirResource_[viewIndex][writeSlot].Get(), zeroReservoirValues, 0, nullptr);

			cmdList->Barrier(reservoirResource_[viewIndex][writeSlot].Get(), reservoirState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			reservoirState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (useDlssRayReconstruction)
			{
				if (radianceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					radianceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(radianceUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearRawIndex_), radianceResource_.Get(), clearValues, 0, nullptr);

				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				radianceState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			if (denoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float clearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(denoisedUnorderedAccessViewIndex_[viewIndex]), clearHeap_.CPUHandle(clearDenoisedIndex_[viewIndex]), denoisedResource_[viewIndex].Get(), clearValues, 0, nullptr);

			cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			/// [JP] 履歴長も 0 にしておく。こうしないと、次に実際にトレースが
			///      走ったフレームで「長い履歴がある」と誤認し、クリア中の無関係な
			///      放射輝度を重く信用してしまう。
			if (historyLengthState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(historyLengthResource_[viewIndex][writeSlot].Get(), historyLengthState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				historyLengthState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(historyLengthUnorderedAccessViewIndex_[viewIndex][writeSlot]), clearHeap_.CPUHandle(clearHistoryLengthIndex_[viewIndex][writeSlot]), historyLengthResource_[viewIndex][writeSlot].Get(), clearValues, 0, nullptr);
			return;
		}
		else
		{
			if (radianceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				radianceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			/// [JP] Reservoir はここで読み書き両方が要る(raygen が同じ
			///      ディスパッチ内で前フレームの history を読み、今フレームの
			///      write スロットへ書くため) - 他のバッファと違い、A-Trous等の
			///      後続パスを待たずに DispatchRays の前に両方遷移させる
			///      (GlobalIlluminationRenderer と同じ)。
			if (reservoirState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(reservoirResource_[viewIndex][historySlot_].Get(), reservoirState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				reservoirState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			}

			if (reservoirState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(reservoirResource_[viewIndex][writeSlot].Get(), reservoirState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				reservoirState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			/// [JP] DispatchRays のルート引数はコンピュートのバインドポイントを使う。
			ID3D12DescriptorHeap* heaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetComputeRootSignature(reflectionShader_.GetRootSignature());
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

			/// [JP] 今フレーム書いた Reservoir を、次フレームが history として
			///      読める状態、かつ下の空間的リユースパスが自分・近傍を読める
			///      状態へ戻す(GlobalIlluminationRenderer と同じ)。
			cmdList->Barrier(reservoirResource_[viewIndex][writeSlot].Get(), reservoirState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			reservoirState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			Uint32 groupCountX = (width_ + 7) / 8;
			Uint32 groupCountY = (height_ + 7) / 8;

			/// [JP] ReSTIR 空間的リユース。raygen が書いた今フレームの Reservoir
			///      (自分+近傍、両方とも上のバリアで読める状態になった直後)を
			///      結合し、その結果を radianceResource_ とその収束度
			///      (confidenceResource_、ReflectionDenoiseCS.hlsl が自身の
			///      時間的ブレンドを reservoir に譲る度合いを決める)へ書き直す
			///      — DLSS-RR/SVGF どちらのデノイザに入る前段でも常に走る(下の
			///      useDlssRayReconstruction 分岐より前、GlobalIlluminationRenderer
			///      と同じ)。
			if (radianceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				radianceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			if (confidenceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(confidenceResource_.Get(), confidenceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				confidenceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetSpatialReusePipelineState());
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			radianceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			cmdList->Barrier(confidenceResource_.Get(), confidenceState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			confidenceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (useDlssRayReconstruction)
			{
				/// [JP] DLSS-RRが自身で最終合成フレームをデノイズするので、
				///      このRenderer自身の時間的蓄積(デノイズCS)は丸ごと
				///      スキップする — 生テクスチャを composite が読める状態
				///      (PIXEL_SHADER_RESOURCE)へ遷移させるだけでよい。
				///      ピンポン蓄積チェーンには一切触れない。
				cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				radianceState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			/// [JP] リプロジェクションが読む履歴側(放射輝度/モーメント/履歴長/
			///      深度法線)をまとめて読み取り状態へ。
			if (accumulatedRadianceState_[viewIndex][historySlot_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			{
				cmdList->Barrier(accumulatedRadianceResource_[viewIndex][historySlot_].Get(), accumulatedRadianceState_[viewIndex][historySlot_], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				accumulatedRadianceState_[viewIndex][historySlot_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
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

			/// [JP] denoiseShader_ は reflectionShader_ と同じ共有
			///      ルートシグネチャ(コンストラクタ引数の rootSignature)を使う
			///      ので、ルート引数の再設定は不要 — PSO だけ差し替える。
			///      groupCountX/Y は上の空間的リユースパスで既に計算済み。

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
			if (accumulatedRadianceState_[viewIndex][writeSlot] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(1));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(accumulatedRadianceResource_[viewIndex][writeSlot].Get(), accumulatedRadianceState_[viewIndex][writeSlot], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			accumulatedRadianceState_[viewIndex][writeSlot] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			/// [JP] パス5(A-Trous step4): history write スロット → denoised 出力。
			if (denoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(2));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(denoisedResource_[viewIndex].Get(), denoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			denoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			/// [JP] 生テクスチャも最後にピクセルシェーダから読める状態へ戻す。
			///      SVGF チェーンが読むのは NON_PIXEL 状態で足りるが、
			///      ViewMode の「反射（生）」表示は DeferredLightingPS
			///      ＝ピクセルシェーダから読むため、その状態のままだと不正な
			///      リソース状態での読み取りになり、表示される値が信用できない。
			cmdList->Barrier(radianceResource_.Get(), radianceState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			radianceState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}
	}
}
