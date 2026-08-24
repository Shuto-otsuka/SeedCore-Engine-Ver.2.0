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
#ifdef _DEBUG
			outResource->SetName(L"Shadow_Denoise");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(outResource.Get());
#endif

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
	* Allocates the raw texture and, per view, the whole SVGF chain for both
	* signals (directional/punctual): each chain's illumination/variance
	* history, moments, A-Trous scratch pair and final denoised output, plus the
	* ONE shared history length and packed depth+normal copy (purely geometric,
	* identical for both chains). Shared by Create() and Resize() so the two can
	* never drift apart.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* raw テクスチャと、ビューごとの両信号(directional/punctual)分の SVGF
	* チェーン一式を確保する: チェーンごとの輝度/分散の履歴、モーメント、
	* A-Trous スクラッチ2枚、最終 denoised 出力、それに両チェーンで共有する
	* (純粋に幾何なので同一になる)履歴長と深度+法線のパック済みコピーを1組。
	* Create() と Resize() で共有し、両者がずれないようにする。
	*/
	void ShadowRenderer::CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 + viewCount * 2 + viewCount * accumulationSlotCount * 6, false);

		historyCleared_ = false;

		/// [JP] r = ディレクショナル可視性、gba = パンクチュアル放射輝度。
		CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, rawVisibilityResource_, rawVisibilityUnorderedAccessViewIndex_, rawVisibilityShaderResourceViewIndex_, &clearRawIndex_);
		rawVisibilityState_ = D3D12_RESOURCE_STATE_COMMON;

		/// [JP] 同じ rawVisibilityResource_ に対する、チャンネルをずらした
		///      2つ目のSRV(DLSS-RR 素通り経路専用 - ShadowRenderer.h の
		///      rawPunctualShaderResourceViewIndex_ のコメント参照)。
		///      Shader4ComponentMapping で dest.r/g/b <- src.g/b/a にずらすので、
		///      このビュー越しに読む `.rgb` が raw の gba(パンクチュアル
		///      放射輝度)になる。
		{
			rawPunctualShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();

			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 2, 3, 3);
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(rawVisibilityResource_.Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(rawPunctualShaderResourceViewIndex_));
		}

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, directionalAccumulatedResource_[view][slot], directionalAccumulatedUnorderedAccessViewIndex_[view][slot], directionalAccumulatedShaderResourceViewIndex_[view][slot], &clearDirectionalAccumulatedIndex_[view][slot]);
				directionalAccumulatedState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, punctualAccumulatedResource_[view][slot], punctualAccumulatedUnorderedAccessViewIndex_[view][slot], punctualAccumulatedShaderResourceViewIndex_[view][slot], &clearPunctualAccumulatedIndex_[view][slot]);
				punctualAccumulatedState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, directionalMomentsResource_[view][slot], directionalMomentsUnorderedAccessViewIndex_[view][slot], directionalMomentsShaderResourceViewIndex_[view][slot], &clearDirectionalMomentsIndex_[view][slot]);
				directionalMomentsState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, punctualMomentsResource_[view][slot], punctualMomentsUnorderedAccessViewIndex_[view][slot], punctualMomentsShaderResourceViewIndex_[view][slot], &clearPunctualMomentsIndex_[view][slot]);
				punctualMomentsState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16_FLOAT, historyLengthResource_[view][slot], historyLengthUnorderedAccessViewIndex_[view][slot], historyLengthShaderResourceViewIndex_[view][slot], &clearHistoryLengthIndex_[view][slot]);
				historyLengthState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				/// [JP] ここだけ 32bit。ビュー深度を FP16 に丸めると、far=1000 の
				///      シーンでは view_z 150 付近から量子化幅(0.125)が下の
				///      再投影の深度許容量を上回り、面が一致していても格納精度
				///      だけで履歴が棄却されるようになる(A-Trous の深度重みも
				///      同時に全タップ 0 へ潰れる)。SVGF の深度テストは
				///      「勾配を単位とした差」を見る以上、深度側の分解能が
				///      勾配より粗いと成立しない。
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, depthNormalResource_[view][slot], depthNormalUnorderedAccessViewIndex_[view][slot], depthNormalShaderResourceViewIndex_[view][slot], &clearDepthNormalIndex_[view][slot]);
				depthNormalState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			for (Uint32 slot = 0; slot < 2; ++slot)
			{
				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16_FLOAT, directionalAtrousScratchResource_[view][slot], directionalAtrousScratchUnorderedAccessViewIndex_[view][slot], directionalAtrousScratchShaderResourceViewIndex_[view][slot], nullptr);
				directionalAtrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;

				CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, punctualAtrousScratchResource_[view][slot], punctualAtrousScratchUnorderedAccessViewIndex_[view][slot], punctualAtrousScratchShaderResourceViewIndex_[view][slot], nullptr);
				punctualAtrousScratchState_[view][slot] = D3D12_RESOURCE_STATE_COMMON;
			}

			CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16_FLOAT, directionalDenoisedResource_[view], directionalDenoisedUnorderedAccessViewIndex_[view], directionalDenoisedShaderResourceViewIndex_[view], &clearDirectionalDenoisedIndex_[view]);
			directionalDenoisedState_[view] = D3D12_RESOURCE_STATE_COMMON;

			CreateShadowTexture(device, bindlessHeap, clearHeap_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, punctualDenoisedResource_[view], punctualDenoisedUnorderedAccessViewIndex_[view], punctualDenoisedShaderResourceViewIndex_[view], &clearPunctualDenoisedIndex_[view]);
			punctualDenoisedState_[view] = D3D12_RESOURCE_STATE_COMMON;
		}
	}

	void ShadowRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(rawVisibilityUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(rawVisibilityShaderResourceViewIndex_);
		bindlessHeap->FreeIndex(rawPunctualShaderResourceViewIndex_);

		bindlessHeap->DeferRelease(rawVisibilityResource_);
		rawVisibilityResource_.Reset();

		for (Uint32 view = 0; view < viewCount; ++view)
		{
			for (Uint32 slot = 0; slot < accumulationSlotCount; ++slot)
			{
				bindlessHeap->FreeIndex(directionalAccumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(directionalAccumulatedShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(directionalAccumulatedResource_[view][slot]);
				directionalAccumulatedResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(punctualAccumulatedUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(punctualAccumulatedShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(punctualAccumulatedResource_[view][slot]);
				punctualAccumulatedResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(directionalMomentsUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(directionalMomentsShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(directionalMomentsResource_[view][slot]);
				directionalMomentsResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(punctualMomentsUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(punctualMomentsShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(punctualMomentsResource_[view][slot]);
				punctualMomentsResource_[view][slot].Reset();

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
				bindlessHeap->FreeIndex(directionalAtrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(directionalAtrousScratchShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(directionalAtrousScratchResource_[view][slot]);
				directionalAtrousScratchResource_[view][slot].Reset();

				bindlessHeap->FreeIndex(punctualAtrousScratchUnorderedAccessViewIndex_[view][slot]);
				bindlessHeap->FreeIndex(punctualAtrousScratchShaderResourceViewIndex_[view][slot]);
				bindlessHeap->DeferRelease(punctualAtrousScratchResource_[view][slot]);
				punctualAtrousScratchResource_[view][slot].Reset();
			}

			bindlessHeap->FreeIndex(directionalDenoisedUnorderedAccessViewIndex_[view]);
			bindlessHeap->FreeIndex(directionalDenoisedShaderResourceViewIndex_[view]);
			bindlessHeap->DeferRelease(directionalDenoisedResource_[view]);
			directionalDenoisedResource_[view].Reset();

			bindlessHeap->FreeIndex(punctualDenoisedUnorderedAccessViewIndex_[view]);
			bindlessHeap->FreeIndex(punctualDenoisedShaderResourceViewIndex_[view]);
			bindlessHeap->DeferRelease(punctualDenoisedResource_[view]);
			punctualDenoisedResource_[view].Reset();
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
		///      もう片方へ書く。1組の historySlot_/writeSlot が両信号チェーンの
		///      輝度・モーメントと、共有の履歴長・深度法線コピーをまとめて
		///      駆動する — これらがずれると、あるフレームの幾何で整合性を
		///      判定しながら別のフレームの輝度をブレンドすることになる。
		auto buildIndices = [&](Uint32 viewIndex)
		{
			ShadowAccumulationIndices values{};

			values.directionalHistoryShaderResourceViewIndex_ = directionalAccumulatedShaderResourceViewIndex_[viewIndex][historySlot_];
			values.directionalAccumulatedUnorderedAccessViewIndex_ = directionalAccumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot];
			values.directionalAccumulatedShaderResourceViewIndex_ = directionalAccumulatedShaderResourceViewIndex_[viewIndex][writeSlot];

			values.punctualHistoryShaderResourceViewIndex_ = punctualAccumulatedShaderResourceViewIndex_[viewIndex][historySlot_];
			values.punctualAccumulatedUnorderedAccessViewIndex_ = punctualAccumulatedUnorderedAccessViewIndex_[viewIndex][writeSlot];
			values.punctualAccumulatedShaderResourceViewIndex_ = punctualAccumulatedShaderResourceViewIndex_[viewIndex][writeSlot];

			/// [JP] DLSS-RRが合成フレーム全体をデノイズするので、その間だけ
			///      「最終」読み取りは生の単一バッファテクスチャを直接指す
			///      (SVGFチェーンには一切触れない)。ディレクショナルは素の
			///      raw テクスチャ(.r がそのまま可視性)、パンクチュアルは
			///      チャンネルをずらした rawPunctualShaderResourceViewIndex_
			///      (.rgb が raw の gba = パンクチュアル放射輝度になる)を指す —
			///      同じビューを両方に使うと、パンクチュアル側が `.rgb` を
			///      読んだ時に raw の r(ディレクショナル可視性)まで拾って
			///      しまい、明るい場所ほど赤みがかる不具合になる。
			values.directionalVisibilityShaderResourceViewIndex_ = dlssRayReconstructionActive_ ? rawVisibilityShaderResourceViewIndex_ : directionalDenoisedShaderResourceViewIndex_[viewIndex];
			values.punctualRadianceShaderResourceViewIndex_ = dlssRayReconstructionActive_ ? rawPunctualShaderResourceViewIndex_ : punctualDenoisedShaderResourceViewIndex_[viewIndex];

			values.directionalAtrousScratch0ShaderResourceViewIndex_ = directionalAtrousScratchShaderResourceViewIndex_[viewIndex][0];
			values.directionalAtrousScratch0UnorderedAccessViewIndex_ = directionalAtrousScratchUnorderedAccessViewIndex_[viewIndex][0];
			values.directionalAtrousScratch1ShaderResourceViewIndex_ = directionalAtrousScratchShaderResourceViewIndex_[viewIndex][1];
			values.directionalAtrousScratch1UnorderedAccessViewIndex_ = directionalAtrousScratchUnorderedAccessViewIndex_[viewIndex][1];

			values.punctualAtrousScratch0ShaderResourceViewIndex_ = punctualAtrousScratchShaderResourceViewIndex_[viewIndex][0];
			values.punctualAtrousScratch0UnorderedAccessViewIndex_ = punctualAtrousScratchUnorderedAccessViewIndex_[viewIndex][0];
			values.punctualAtrousScratch1ShaderResourceViewIndex_ = punctualAtrousScratchShaderResourceViewIndex_[viewIndex][1];
			values.punctualAtrousScratch1UnorderedAccessViewIndex_ = punctualAtrousScratchUnorderedAccessViewIndex_[viewIndex][1];

			values.directionalMomentsHistoryShaderResourceViewIndex_ = directionalMomentsShaderResourceViewIndex_[viewIndex][historySlot_];
			values.directionalMomentsShaderResourceViewIndex_ = directionalMomentsShaderResourceViewIndex_[viewIndex][writeSlot];
			values.directionalMomentsUnorderedAccessViewIndex_ = directionalMomentsUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.punctualMomentsHistoryShaderResourceViewIndex_ = punctualMomentsShaderResourceViewIndex_[viewIndex][historySlot_];
			values.punctualMomentsShaderResourceViewIndex_ = punctualMomentsShaderResourceViewIndex_[viewIndex][writeSlot];
			values.punctualMomentsUnorderedAccessViewIndex_ = punctualMomentsUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.historyLengthHistoryShaderResourceViewIndex_ = historyLengthShaderResourceViewIndex_[viewIndex][historySlot_];
			values.historyLengthShaderResourceViewIndex_ = historyLengthShaderResourceViewIndex_[viewIndex][writeSlot];
			values.historyLengthUnorderedAccessViewIndex_ = historyLengthUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.depthNormalHistoryShaderResourceViewIndex_ = depthNormalShaderResourceViewIndex_[viewIndex][historySlot_];
			values.depthNormalShaderResourceViewIndex_ = depthNormalShaderResourceViewIndex_[viewIndex][writeSlot];
			values.depthNormalUnorderedAccessViewIndex_ = depthNormalUnorderedAccessViewIndex_[viewIndex][writeSlot];

			values.directionalDenoisedUnorderedAccessViewIndex_ = directionalDenoisedUnorderedAccessViewIndex_[viewIndex];
			values.punctualDenoisedUnorderedAccessViewIndex_ = punctualDenoisedUnorderedAccessViewIndex_[viewIndex];

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

		/// [JP] 履歴チェーンの一括ゼロクリア。生成直後の1回だけ、全ビュー・全
		///      スロットをまとめて潰す。ここを通さないと未初期化のビットパターンが
		///      履歴として読み戻され、そのまま自己再投入されて焼き付く。
		if (!historyCleared_)
		{
			historyCleared_ = true;

			ID3D12DescriptorHeap* clearHeaps[] = { heap };
			cmd->SetDescriptorHeaps(_countof(clearHeaps), clearHeaps);

			const Float zeroValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			for (Uint32 clearView = 0; clearView < viewCount; ++clearView)
			{
				for (Uint32 clearSlot = 0; clearSlot < accumulationSlotCount; ++clearSlot)
				{
					auto clearTexture = [&](Microsoft::WRL::ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES& state, Uint32 unorderedAccessViewIndex, Uint32 clearIndex)
					{
						if (state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						{
							cmdList->Barrier(resource.Get(), state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
							state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						}

						cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(unorderedAccessViewIndex), clearHeap_.CPUHandle(clearIndex), resource.Get(), zeroValues, 0, nullptr);
					};

					clearTexture(directionalAccumulatedResource_[clearView][clearSlot], directionalAccumulatedState_[clearView][clearSlot], directionalAccumulatedUnorderedAccessViewIndex_[clearView][clearSlot], clearDirectionalAccumulatedIndex_[clearView][clearSlot]);
					clearTexture(punctualAccumulatedResource_[clearView][clearSlot], punctualAccumulatedState_[clearView][clearSlot], punctualAccumulatedUnorderedAccessViewIndex_[clearView][clearSlot], clearPunctualAccumulatedIndex_[clearView][clearSlot]);
					clearTexture(directionalMomentsResource_[clearView][clearSlot], directionalMomentsState_[clearView][clearSlot], directionalMomentsUnorderedAccessViewIndex_[clearView][clearSlot], clearDirectionalMomentsIndex_[clearView][clearSlot]);
					clearTexture(punctualMomentsResource_[clearView][clearSlot], punctualMomentsState_[clearView][clearSlot], punctualMomentsUnorderedAccessViewIndex_[clearView][clearSlot], clearPunctualMomentsIndex_[clearView][clearSlot]);
					clearTexture(historyLengthResource_[clearView][clearSlot], historyLengthState_[clearView][clearSlot], historyLengthUnorderedAccessViewIndex_[clearView][clearSlot], clearHistoryLengthIndex_[clearView][clearSlot]);
					clearTexture(depthNormalResource_[clearView][clearSlot], depthNormalState_[clearView][clearSlot], depthNormalUnorderedAccessViewIndex_[clearView][clearSlot], clearDepthNormalIndex_[clearView][clearSlot]);
				}
			}
		}

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
			/// [JP] 追跡対象(TLAS)が無い、または PSO が無いフレーム。composite が
			///      実際に読む先(DLSS-RR経路なら生テクスチャ、通常経路なら
			///      denoised 出力)をクリアする。ディレクショナルは可視性
			///      乗数なので照射(1.0)、パンクチュアルはBRDF評価済み放射輝度の
			///      加算項なので、追跡できていない以上「何も足さない」意味で
			///      0.0 でクリアする(1.0 で埋めるとフラットな白を加算してしまう)。
			if (dlssRayReconstructionActive_)
			{
				if (rawVisibilityState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					rawVisibilityState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}

				const Float clearValues[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
				cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(rawVisibilityUnorderedAccessViewIndex_), clearHeap_.CPUHandle(clearRawIndex_), rawVisibilityResource_.Get(), clearValues, 0, nullptr);

				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			if (directionalDenoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(directionalDenoisedResource_[viewIndex].Get(), directionalDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				directionalDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float litValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(directionalDenoisedUnorderedAccessViewIndex_[viewIndex]), clearHeap_.CPUHandle(clearDirectionalDenoisedIndex_[viewIndex]), directionalDenoisedResource_[viewIndex].Get(), litValues, 0, nullptr);

			cmdList->Barrier(directionalDenoisedResource_[viewIndex].Get(), directionalDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			directionalDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			if (punctualDenoisedState_[viewIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(punctualDenoisedResource_[viewIndex].Get(), punctualDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				punctualDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			const Float zeroLitValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			cmd->ClearUnorderedAccessViewFloat(bindlessHeap_->GPUHandle(punctualDenoisedUnorderedAccessViewIndex_[viewIndex]), clearHeap_.CPUHandle(clearPunctualDenoisedIndex_[viewIndex]), punctualDenoisedResource_[viewIndex].Get(), zeroLitValues, 0, nullptr);

			cmdList->Barrier(punctualDenoisedResource_[viewIndex].Get(), punctualDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			punctualDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

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
				cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				rawVisibilityState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				return;
			}

			/// [JP] リプロジェクションが読む履歴側(両チェーンの輝度/モーメント、
			///      共有の履歴長/深度法線)をまとめて読み取り状態へ。
			auto toRead = [&](Microsoft::WRL::ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES& state)
			{
				if (state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				{
					cmdList->Barrier(resource.Get(), state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				}
			};

			auto toWrite = [&](Microsoft::WRL::ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES& state)
			{
				if (state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					cmdList->Barrier(resource.Get(), state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				}
			};

			toRead(directionalAccumulatedResource_[viewIndex][historySlot_], directionalAccumulatedState_[viewIndex][historySlot_]);
			toRead(punctualAccumulatedResource_[viewIndex][historySlot_], punctualAccumulatedState_[viewIndex][historySlot_]);
			toRead(directionalMomentsResource_[viewIndex][historySlot_], directionalMomentsState_[viewIndex][historySlot_]);
			toRead(punctualMomentsResource_[viewIndex][historySlot_], punctualMomentsState_[viewIndex][historySlot_]);
			toRead(historyLengthResource_[viewIndex][historySlot_], historyLengthState_[viewIndex][historySlot_]);
			toRead(depthNormalResource_[viewIndex][historySlot_], depthNormalState_[viewIndex][historySlot_]);

			/// [JP] パス1(リプロジェクション): raw + 履歴 → 両チェーンの scratch0 と、
			///      今フレームのモーメント(両チェーン)/履歴長/深度法線(共有)。
			toWrite(directionalAtrousScratchResource_[viewIndex][0], directionalAtrousScratchState_[viewIndex][0]);
			toWrite(punctualAtrousScratchResource_[viewIndex][0], punctualAtrousScratchState_[viewIndex][0]);
			toWrite(directionalMomentsResource_[viewIndex][writeSlot], directionalMomentsState_[viewIndex][writeSlot]);
			toWrite(punctualMomentsResource_[viewIndex][writeSlot], punctualMomentsState_[viewIndex][writeSlot]);
			toWrite(historyLengthResource_[viewIndex][writeSlot], historyLengthState_[viewIndex][writeSlot]);
			toWrite(depthNormalResource_[viewIndex][writeSlot], depthNormalState_[viewIndex][writeSlot]);

			cmd->SetPipelineState(denoisePipelineState);
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			/// [JP] 以降のパスはモーメント/履歴長/深度法線を読むだけなので、
			///      ここで一度だけ読み取り状態へ落として最後まで据え置く。
			toRead(directionalMomentsResource_[viewIndex][writeSlot], directionalMomentsState_[viewIndex][writeSlot]);
			toRead(punctualMomentsResource_[viewIndex][writeSlot], punctualMomentsState_[viewIndex][writeSlot]);
			toRead(historyLengthResource_[viewIndex][writeSlot], historyLengthState_[viewIndex][writeSlot]);
			toRead(depthNormalResource_[viewIndex][writeSlot], depthNormalState_[viewIndex][writeSlot]);
			toRead(directionalAtrousScratchResource_[viewIndex][0], directionalAtrousScratchState_[viewIndex][0]);
			toRead(punctualAtrousScratchResource_[viewIndex][0], punctualAtrousScratchState_[viewIndex][0]);

			/// [JP] パス2(FilterMoments): 両チェーンの scratch0 → scratch1。
			toWrite(directionalAtrousScratchResource_[viewIndex][1], directionalAtrousScratchState_[viewIndex][1]);
			toWrite(punctualAtrousScratchResource_[viewIndex][1], punctualAtrousScratchState_[viewIndex][1]);

			cmd->SetPipelineState(denoiseShader_.GetFilterMomentsPipelineState());
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			toRead(directionalAtrousScratchResource_[viewIndex][1], directionalAtrousScratchState_[viewIndex][1]);
			toRead(punctualAtrousScratchResource_[viewIndex][1], punctualAtrousScratchState_[viewIndex][1]);

			/// [JP] パス3(A-Trous step1): 両チェーンの scratch1 → scratch0。
			toWrite(directionalAtrousScratchResource_[viewIndex][0], directionalAtrousScratchState_[viewIndex][0]);
			toWrite(punctualAtrousScratchResource_[viewIndex][0], punctualAtrousScratchState_[viewIndex][0]);

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(0));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			toRead(directionalAtrousScratchResource_[viewIndex][0], directionalAtrousScratchState_[viewIndex][0]);
			toRead(punctualAtrousScratchResource_[viewIndex][0], punctualAtrousScratchState_[viewIndex][0]);

			/// [JP] パス4(A-Trous step2 = フィードバックタップ): 両チェーンの
			///      scratch0 → history write スロット。これが次フレームの履歴になる。
			toWrite(directionalAccumulatedResource_[viewIndex][writeSlot], directionalAccumulatedState_[viewIndex][writeSlot]);
			toWrite(punctualAccumulatedResource_[viewIndex][writeSlot], punctualAccumulatedState_[viewIndex][writeSlot]);

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(1));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			toRead(directionalAccumulatedResource_[viewIndex][writeSlot], directionalAccumulatedState_[viewIndex][writeSlot]);
			toRead(punctualAccumulatedResource_[viewIndex][writeSlot], punctualAccumulatedState_[viewIndex][writeSlot]);

			/// [JP] パス5(A-Trous step4): history write スロット → 両チェーンの
			///      denoised 出力。
			toWrite(directionalDenoisedResource_[viewIndex], directionalDenoisedState_[viewIndex]);
			toWrite(punctualDenoisedResource_[viewIndex], punctualDenoisedState_[viewIndex]);

			cmd->SetPipelineState(denoiseShader_.GetATrousPipelineState(2));
			cmd->Dispatch(groupCountX, groupCountY, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(directionalDenoisedResource_[viewIndex].Get(), directionalDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			directionalDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			cmdList->Barrier(punctualDenoisedResource_[viewIndex].Get(), punctualDenoisedState_[viewIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			punctualDenoisedState_[viewIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			/// [JP] 生テクスチャも最後にピクセルシェーダから読める状態へ戻す。
			///      デノイズチェーンが読むのは NON_PIXEL 状態で足りるが、
			///      ViewMode の「シャドウ（生）」表示は DeferredLightingPS
			///      ＝ピクセルシェーダから読むため、その状態のままだと不正な
			///      リソース状態での読み取りになり、表示される値が信用できない。
			cmdList->Barrier(rawVisibilityResource_.Get(), rawVisibilityState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			rawVisibilityState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}
	}
}
