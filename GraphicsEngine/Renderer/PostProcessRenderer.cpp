#include <GraphicsEngine/Renderer/PostProcessRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	PostProcessRenderer::PostProcessRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : autoExposureShader_(rootSignature, pipelineStateObject), toneMappingShader_(rootSignature, pipelineStateObject), lensFlareShader_(rootSignature, pipelineStateObject), depthOfFieldShader_(rootSignature, pipelineStateObject), bokehShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void PostProcessRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		autoExposureShader_.Create(shaderCache, device);
		toneMappingShader_.Create(shaderCache, device);
		lensFlareShader_.Create(shaderCache, device);
		depthOfFieldShader_.Create(shaderCache, device);
		bokehShader_.Create(shaderCache, device);

		CreateResources(device, bindlessHeap, width, height, outputWidth, outputHeight);
	}

	void PostProcessRenderer::Destroy(BindlessHeap* bindlessHeap)
	{
		DestroyView(bindlessHeap, editorView_);
		DestroyView(bindlessHeap, gameView_);
	}

	void PostProcessRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		Destroy(bindlessHeap);
		CreateResources(device, bindlessHeap, width, height, outputWidth, outputHeight);
	}

	void PostProcessRenderer::CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		bindlessHeap_ = bindlessHeap;
		width_ = width;
		height_ = height;
		outputWidth_ = outputWidth;
		outputHeight_ = outputHeight;

		/// [JP] ビューごとに histogram + exposure の RAW クリアビュー = 2枠、
		///      Editor/Game で計4枠。
		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, false);

		/// [JP] ビューごとに SD 出力 + UHD 出力 の RTV = 2枠、Editor/Game で計4枠。
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);

		CreateView(device, bindlessHeap, editorView_, width, height, outputWidth, outputHeight);
		CreateView(device, bindlessHeap, gameView_, width, height, outputWidth, outputHeight);
	}

	/**
	* [EN]
	* Creates one view's three resources: the UNORM display texture (UAV
	* only - ImGui builds its own SRV directly from the raw ID3D12Resource*,
	* the same way it already does for the HDR FrameBuffer, so no bindless SRV
	* is needed here), the 256-bin histogram buffer, and the persistent
	* 1-float exposure buffer. The histogram and exposure buffers are
	* STRUCTURED UAVs, so each also gets a RAW (R32_TYPELESS) view in
	* clearHeap_ for ClearUnorderedAccessViewUint - a structured UAV cannot be
	* cleared directly (same trick as LightSystem's cluster buffer).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1ビューぶんの3リソースを生成する: UNORM 表示テクスチャ(UAV のみ —
	* ImGui は既に HDR FrameBuffer に対してやっているのと同じく生の
	* ID3D12Resource* から自前で SRV を作るので、ここに bindless SRV は不要)、
	* 256ビンのヒストグラムバッファ、永続的な1要素 float 露出バッファ。
	* ヒストグラム/露出バッファは構造化 UAV なので、ClearUnorderedAccessViewUint
	* 用に clearHeap_ 側へ RAW(R32_TYPELESS)ビューも用意する — 構造化 UAV は
	* 直接クリアできない(LightSystem のクラスタバッファと同じ手法)。
	*/
	void PostProcessRenderer::CreateView(ID3D12Device* device, BindlessHeap* bindlessHeap, View& view, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		// ---- 表示テクスチャ ----
		D3D12_RESOURCE_DESC outputDesc{};
		outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		outputDesc.Width = width;
		outputDesc.Height = height;
		outputDesc.DepthOrArraySize = 1;
		outputDesc.MipLevels = 1;
		outputDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		outputDesc.SampleDesc.Count = 1;
		outputDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &outputDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.outputResource_));
		SC_HR_CHECK(hr, "ポストプロセス表示テクスチャの生成に失敗しました");
		view.outputState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_UNORDERED_ACCESS_VIEW_DESC outputUnorderedAccessViewDesc{};
		outputUnorderedAccessViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		outputUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		view.outputUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(view.outputResource_.Get(), nullptr, &outputUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.outputUnorderedAccessViewIndex_));

		/// [EN] RTV so a post-tonemap debug overlay (collider wireframes,
		///      selection outline) can draw directly onto this display
		///      texture - see BeginDebugOverlay/EndDebugOverlay.
		/// [JP] トーンマップ後のデバッグオーバーレイ(コライダー
		///      ワイヤーフレーム、選択アウトライン)がこの表示テクスチャへ
		///      直接描画できるようにするRTV - BeginDebugOverlay/
		///      EndDebugOverlay参照。
		D3D12_RENDER_TARGET_VIEW_DESC outputRenderTargetViewDesc{};
		outputRenderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		outputRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		view.outputRenderTargetViewIndex_ = renderTargetViewHeap_.AllocateIndex();
		device->CreateRenderTargetView(view.outputResource_.Get(), &outputRenderTargetViewDesc, renderTargetViewHeap_.CPUHandle(view.outputRenderTargetViewIndex_));

		// ---- 表示テクスチャ(UHD版、DLSS-RR有効時用) ----
		D3D12_RESOURCE_DESC outputDescUpscaled{};
		outputDescUpscaled.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		outputDescUpscaled.Width = static_cast<Uint64>(outputWidth);
		outputDescUpscaled.Height = outputHeight;
		outputDescUpscaled.DepthOrArraySize = 1;
		outputDescUpscaled.MipLevels = 1;
		outputDescUpscaled.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		outputDescUpscaled.SampleDesc.Count = 1;
		outputDescUpscaled.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &outputDescUpscaled, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.outputResourceUpscaled_));
		SC_HR_CHECK(hr, "ポストプロセス表示テクスチャ(UHD)の生成に失敗しました");
		view.outputStateUpscaled_ = D3D12_RESOURCE_STATE_COMMON;

		view.outputUnorderedAccessViewIndexUpscaled_ = bindlessHeap->AllocateIndex();
		device->CreateUnorderedAccessView(view.outputResourceUpscaled_.Get(), nullptr, &outputUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.outputUnorderedAccessViewIndexUpscaled_));

		view.outputRenderTargetViewIndexUpscaled_ = renderTargetViewHeap_.AllocateIndex();
		device->CreateRenderTargetView(view.outputResourceUpscaled_.Get(), &outputRenderTargetViewDesc, renderTargetViewHeap_.CPUHandle(view.outputRenderTargetViewIndexUpscaled_));

		// ---- ヒストグラムバッファ (256 x uint) ----
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = 256 * sizeof(Uint32);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.histogramResource_));
			SC_HR_CHECK(hr, "露出ヒストグラムバッファの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.NumElements = 256;
			unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(Uint32);

			view.histogramUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.histogramResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view.histogramUnorderedAccessViewIndex_));

			D3D12_UNORDERED_ACCESS_VIEW_DESC rawUnorderedAccessViewDesc{};
			rawUnorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			rawUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			rawUnorderedAccessViewDesc.Buffer.NumElements = 256;
			rawUnorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

			view.histogramClearUnorderedAccessViewIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(view.histogramResource_.Get(), nullptr, &rawUnorderedAccessViewDesc, clearHeap_.CPUHandle(view.histogramClearUnorderedAccessViewIndex_));

			view.histogramClearGpuUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.histogramResource_.Get(), nullptr, &rawUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.histogramClearGpuUnorderedAccessViewIndex_));
		}

		// ---- 永続露出バッファ (1 x float) ----
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = sizeof(Float);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.exposureResource_));
			SC_HR_CHECK(hr, "永続露出バッファの生成に失敗しました");

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.NumElements = 1;
			unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(Float);

			view.exposureUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.exposureResource_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(view.exposureUnorderedAccessViewIndex_));

			D3D12_UNORDERED_ACCESS_VIEW_DESC rawUnorderedAccessViewDesc{};
			rawUnorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			rawUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			rawUnorderedAccessViewDesc.Buffer.NumElements = 1;
			rawUnorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

			view.exposureClearUnorderedAccessViewIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(view.exposureResource_.Get(), nullptr, &rawUnorderedAccessViewDesc, clearHeap_.CPUHandle(view.exposureClearUnorderedAccessViewIndex_));

			view.exposureClearGpuUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.exposureResource_.Get(), nullptr, &rawUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.exposureClearGpuUnorderedAccessViewIndex_));
		}

		// ---- レンズフレア加算バッファ(ネイティブ解像度の1/4) ----
		{
			Uint32 lensFlareWidth = Max<Uint32>(1, width / 4);
			Uint32 lensFlareHeight = Max<Uint32>(1, height / 4);

			D3D12_RESOURCE_DESC lensFlareDesc{};
			lensFlareDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			lensFlareDesc.Width = lensFlareWidth;
			lensFlareDesc.Height = lensFlareHeight;
			lensFlareDesc.DepthOrArraySize = 1;
			lensFlareDesc.MipLevels = 1;
			lensFlareDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			lensFlareDesc.SampleDesc.Count = 1;
			lensFlareDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &lensFlareDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.lensFlareResource_));
			SC_HR_CHECK(hr, "レンズフレアバッファの生成に失敗しました");
			view.lensFlareState_ = D3D12_RESOURCE_STATE_COMMON;

			D3D12_UNORDERED_ACCESS_VIEW_DESC lensFlareUnorderedAccessViewDesc{};
			lensFlareUnorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			lensFlareUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			view.lensFlareUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.lensFlareResource_.Get(), nullptr, &lensFlareUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.lensFlareUnorderedAccessViewIndex_));

			D3D12_SHADER_RESOURCE_VIEW_DESC lensFlareShaderResourceViewDesc{};
			lensFlareShaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			lensFlareShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			lensFlareShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			lensFlareShaderResourceViewDesc.Texture2D.MipLevels = 1;

			view.lensFlareShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateShaderResourceView(view.lensFlareResource_.Get(), &lensFlareShaderResourceViewDesc, bindlessHeap->CPUHandle(view.lensFlareShaderResourceViewIndex_));
		}

		// ---- 被写界深度/ボケ出力(ネイティブ解像度) ----
		{
			D3D12_RESOURCE_DESC depthOfFieldDesc{};
			depthOfFieldDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			depthOfFieldDesc.Width = width;
			depthOfFieldDesc.Height = height;
			depthOfFieldDesc.DepthOrArraySize = 1;
			depthOfFieldDesc.MipLevels = 1;
			depthOfFieldDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			depthOfFieldDesc.SampleDesc.Count = 1;
			depthOfFieldDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthOfFieldDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&view.depthOfFieldResource_));
			SC_HR_CHECK(hr, "被写界深度バッファの生成に失敗しました");
			view.depthOfFieldState_ = D3D12_RESOURCE_STATE_COMMON;

			D3D12_UNORDERED_ACCESS_VIEW_DESC depthOfFieldUnorderedAccessViewDesc{};
			depthOfFieldUnorderedAccessViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			depthOfFieldUnorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			view.depthOfFieldUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateUnorderedAccessView(view.depthOfFieldResource_.Get(), nullptr, &depthOfFieldUnorderedAccessViewDesc, bindlessHeap->CPUHandle(view.depthOfFieldUnorderedAccessViewIndex_));

			D3D12_SHADER_RESOURCE_VIEW_DESC depthOfFieldShaderResourceViewDesc{};
			depthOfFieldShaderResourceViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			depthOfFieldShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			depthOfFieldShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			depthOfFieldShaderResourceViewDesc.Texture2D.MipLevels = 1;

			view.depthOfFieldShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
			device->CreateShaderResourceView(view.depthOfFieldResource_.Get(), &depthOfFieldShaderResourceViewDesc, bindlessHeap->CPUHandle(view.depthOfFieldShaderResourceViewIndex_));
		}
	}

	void PostProcessRenderer::DestroyView(BindlessHeap* bindlessHeap, View& view)
	{
		bindlessHeap->FreeIndex(view.outputUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.outputUnorderedAccessViewIndexUpscaled_);
		bindlessHeap->FreeIndex(view.histogramUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.histogramClearGpuUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.exposureUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.exposureClearGpuUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.lensFlareUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.lensFlareShaderResourceViewIndex_);
		bindlessHeap->FreeIndex(view.depthOfFieldUnorderedAccessViewIndex_);
		bindlessHeap->FreeIndex(view.depthOfFieldShaderResourceViewIndex_);

		view.outputResource_.Reset();
		view.outputResourceUpscaled_.Reset();
		view.histogramResource_.Reset();
		view.exposureResource_.Reset();
		view.lensFlareResource_.Reset();
		view.depthOfFieldResource_.Reset();

		view.activeIsUpscaled_ = false;
		view.exposureInitialized_ = false;
	}

	void PostProcessRenderer::Gather(World& world)
	{
		volumes_.clear();

		Query<Read<Active>, Read<PostProcess>> postProcessQuery(world);
		postProcessQuery.ForEach([&](EntityID entityID, const Active& active, const PostProcess& postProcess)
		{
			volumes_.push_back(postProcess);
		});
	}

	/**
	* [EN]
	* When no PostProcess entity exists in the scene, this returns a NEUTRAL
	* result (zero exposure compensation, no tone curve, auto exposure off) -
	* deliberately NOT PostProcess{}'s own field defaults (compensation_=1.65,
	* mode_=AcesFilmic), which are only a sensible starting point for when a
	* volume IS authored. An unauthored scene must render with zero implicit
	* correction: no PostProcess entity means no post-process effect, full
	* stop. The sRGB encode in ToneMappingCS.hlsl still always runs regardless
	* - that is a hard requirement of the UNORM backbuffer having no hardware
	* gamma view anywhere in this engine, not a postprocess "effect" to opt
	* into.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーンに PostProcess エンティティが1つも無い場合はニュートラルな結果
	* (露出補正0、トーンカーブ無し、自動露出オフ)を返す — 意図的に
	* PostProcess{} 自身のフィールド既定値(compensation_=1.65、
	* mode_=AcesFilmic)は使わない。あれはボリュームを実際に配置した時の
	* 妥当な初期値でしかない。ボリュームの無いシーンは暗黙の補正ゼロで
	* 描画されるべき — PostProcess エンティティが無い=ポストプロセス効果も
	* 無い、それだけのこと。ToneMappingCS.hlsl の sRGB エンコードはこれとは
	* 無関係に常に実行する — これはこのエンジンの UNORM バックバッファに
	* ハードウェア sRGB ビューが一切無いことに由来する必須処理であって、
	* オプトインするポストプロセス「効果」ではないため。
	*/
	const PostProcess& PostProcessRenderer::ResolveSettings()const
	{
		static const PostProcess neutralSettings = []()
		{
			PostProcess settings{};
			settings.exposure_.compensation_ = 0.0f;
			settings.exposure_.enabled_ = false;
			settings.toneMapping_.enabled_ = false;
			return settings;
		}();

		return volumes_.empty() ? neutralSettings : volumes_[0];
	}

	void PostProcessRenderer::PrepareView(IndicesSystem& indicesSystem, RaytracingView view, Uint32 sourceColorIndex, Bool useUpscaledOutput)
	{
		const PostProcess& settings = ResolveSettings();
		View& target = ViewFor(view);
		target.activeIsUpscaled_ = useUpscaledOutput;

		PostProcessIndices values{};
		values.outputUnorderedAccessViewIndex_ = useUpscaledOutput ? target.outputUnorderedAccessViewIndexUpscaled_ : target.outputUnorderedAccessViewIndex_;
		values.sourceColorIndex_ = sourceColorIndex;

		values.exposure_.histogramUnorderedAccessViewIndex_ = target.histogramUnorderedAccessViewIndex_;
		values.exposure_.exposureUnorderedAccessViewIndex_ = target.exposureUnorderedAccessViewIndex_;
		values.exposure_.autoExposureEnabled_ = settings.exposure_.enabled_ ? 1 : 0;
		values.exposure_.exposureCompensation_ = settings.exposure_.compensation_;

		values.exposure_.minLogLuminance_ = settings.exposure_.minLogLuminance_;
		values.exposure_.maxLogLuminance_ = settings.exposure_.maxLogLuminance_;
		values.exposure_.keyValue_ = settings.exposure_.keyValue_;
		values.exposure_.adaptSpeedToBright_ = settings.exposure_.adaptSpeedToBright_;
		values.exposure_.adaptSpeedToDark_ = settings.exposure_.adaptSpeedToDark_;

		values.toneMapping_.toneMappingEnabled_ = settings.toneMapping_.enabled_ ? 1 : 0;
		values.toneMapping_.toneMappingMode_ = static_cast<Uint>(settings.toneMapping_.mode_);

		values.lensFlare_.enabled_ = settings.lensFlare_.enabled_ ? 1 : 0;
		values.lensFlare_.unorderedAccessViewIndex_ = target.lensFlareUnorderedAccessViewIndex_;
		values.lensFlare_.shaderResourceViewIndex_ = target.lensFlareShaderResourceViewIndex_;
		values.lensFlare_.threshold_ = settings.lensFlare_.threshold_;
		values.lensFlare_.intensity_ = settings.lensFlare_.intensity_;
		values.lensFlare_.streakLength_ = settings.lensFlare_.streakLength_;
		values.lensFlare_.streakAttenuation_ = settings.lensFlare_.streakAttenuation_;
		values.lensFlare_.chromaticAberration_ = settings.lensFlare_.chromaticAberration_;
		values.lensFlare_.angleOffset_ = settings.lensFlare_.angleOffset_;

		values.depthOfField_.enabled_ = settings.depthOfField_.enabled_ ? 1 : 0;
		values.depthOfField_.unorderedAccessViewIndex_ = target.depthOfFieldUnorderedAccessViewIndex_;
		values.depthOfField_.shaderResourceViewIndex_ = target.depthOfFieldShaderResourceViewIndex_;
		values.depthOfField_.focusDistance_ = settings.depthOfField_.focusDistance_;
		values.depthOfField_.focusRange_ = settings.depthOfField_.focusRange_;
		values.depthOfField_.maxBlurRadius_ = settings.depthOfField_.maxBlurRadius_;

		values.bokeh_.enabled_ = (settings.depthOfField_.enabled_ && settings.bokeh_.enabled_) ? 1 : 0;
		values.bokeh_.highlightThreshold_ = settings.bokeh_.highlightThreshold_;
		values.bokeh_.highlightIntensity_ = settings.bokeh_.highlightIntensity_;
		values.bokeh_.bladeCount_ = settings.bokeh_.bladeCount_;

		if (view == RaytracingView::Editor)
		{
			indicesSystem.SetEditorPostProcessIndices(values);
		}
		else
		{
			indicesSystem.SetGamePostProcessIndices(values);
		}
	}

	void PostProcessRenderer::Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, ID3D12Resource* sourceColorResource, Bool useUpscaledOutput)
	{
		const PostProcess& settings = ResolveSettings();
		Bool autoExposureEnabled = settings.exposure_.enabled_;
		Bool lensFlareEnabled = settings.lensFlare_.enabled_;
		Bool depthOfFieldEnabled = settings.depthOfField_.enabled_;
		Bool bokehEnabled = settings.depthOfField_.enabled_ && settings.bokeh_.enabled_;

		View& target = ViewFor(view);
		auto* cmd = cmdList->Get();

		Microsoft::WRL::ComPtr<ID3D12Resource>& outputResource = useUpscaledOutput ? target.outputResourceUpscaled_ : target.outputResource_;
		D3D12_RESOURCE_STATES& outputState = useUpscaledOutput ? target.outputStateUpscaled_ : target.outputState_;
		Uint32 dispatchWidth = useUpscaledOutput ? outputWidth_ : width_;
		Uint32 dispatchHeight = useUpscaledOutput ? outputHeight_ : height_;

		/// [JP] HDR ソースはこの関数に入ってくる時点で PIXEL_SHADER_RESOURCE
		///      (FrameBuffer::End() が残した状態)。コンピュートから読むには
		///      NON_PIXEL_SHADER_RESOURCE が要るので一時的に遷移し、この関数を
		///      抜ける前に必ず元へ戻す — FrameBuffer 自身の状態管理(currentState_)
		///      はこの一時遷移を知らないため、戻し忘れると次フレームの Begin()
		///      が誤った "before" 状態でバリアを積んでデバッグレイヤ検証に
		///      引っかかる。
		cmdList->Barrier(sourceColorResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		if (outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			cmdList->Barrier(outputResource.Get(), outputState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		/// [JP] ヒストグラム/露出バッファは初回のみ COMMON→UNORDERED_ACCESS
		///      へ遷移し、以後は生存期間中ずっと UNORDERED_ACCESS のまま
		///      (このクラスの外は一切触らないため)。露出バッファは同じ回で
		///      0 初期化もしておく(以後は AverageCS の read-modify-write が
		///      唯一の書き手なので、二度とクリアしない)。
		if (!target.exposureInitialized_)
		{
			cmdList->Barrier(target.histogramResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmdList->Barrier(target.exposureResource_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			const Uint32 zeroValues[4] = { 0, 0, 0, 0 };
			cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(target.exposureClearGpuUnorderedAccessViewIndex_), clearHeap_.CPUHandle(target.exposureClearUnorderedAccessViewIndex_), target.exposureResource_.Get(), zeroValues, 0, nullptr);

			target.exposureInitialized_ = true;
		}

		ID3D12PipelineState* toneMappingPipelineState = toneMappingShader_.GetPipelineState();
		ID3D12PipelineState* histogramPipelineState = autoExposureShader_.GetHistogramPipelineState();
		ID3D12PipelineState* averagePipelineState = autoExposureShader_.GetAveragePipelineState();
		ID3D12PipelineState* lensFlarePipelineState = lensFlareShader_.GetPipelineState();
		ID3D12PipelineState* depthOfFieldPipelineState = depthOfFieldShader_.GetPipelineState();
		ID3D12PipelineState* bokehPipelineState = bokehShader_.GetPipelineState();

		if ((!toneMappingPipelineState || !histogramPipelineState || !averagePipelineState || !lensFlarePipelineState || !depthOfFieldPipelineState || !bokehPipelineState) && !pipelineStateMissingLogged_)
		{
			SC_LOG_WARNING("PostProcess のコンピュート PSO 作成に失敗しています。エディタ/ゲームビューは表示できません。");
			pipelineStateMissingLogged_ = true;
		}

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
		cmd->SetComputeRootConstantBufferView(2, constantIndex);
		cmd->SetComputeRootConstantBufferView(3, structuredIndex);

		if (autoExposureEnabled && histogramPipelineState && averagePipelineState)
		{
			const Uint32 zeroValues[4] = { 0, 0, 0, 0 };
			cmd->ClearUnorderedAccessViewUint(bindlessHeap_->GPUHandle(target.histogramClearGpuUnorderedAccessViewIndex_), clearHeap_.CPUHandle(target.histogramClearUnorderedAccessViewIndex_), target.histogramResource_.Get(), zeroValues, 0, nullptr);
			UnorderedAccessBarrier(cmdList, target.histogramResource_.Get());

			cmd->SetComputeRootSignature(autoExposureShader_.GetRootSignature());
			cmd->SetPipelineState(histogramPipelineState);
			cmd->Dispatch((dispatchWidth + 15) / 16, (dispatchHeight + 15) / 16, 1);
			ProfilerStats::AddDrawCall();

			UnorderedAccessBarrier(cmdList, target.histogramResource_.Get());

			cmd->SetPipelineState(averagePipelineState);
			cmd->Dispatch(1, 1, 1);
			ProfilerStats::AddDrawCall();

			UnorderedAccessBarrier(cmdList, target.exposureResource_.Get());
		}

		/// [JP] LensFlareCS.hlsl/ToneMappingCS.hlsl より前に走らせる —
		///      無効時は両シェーダとも生のHDRソース(source_color_index_)を
		///      直接読む(PrepareView が values.depthOfField_.enabled_ を
		///      落としているため、古いデータが表示に混ざることはない)。
		///      BokehCS.hlsl は DepthOfFieldCS.hlsl が書いた同じバッファを
		///      read-modify-write するので、この2つは必ずこの順で連続して積む
		///      (間に他のディスパッチを挟まない)。
		if (depthOfFieldEnabled && depthOfFieldPipelineState)
		{
			if (target.depthOfFieldState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(target.depthOfFieldResource_.Get(), target.depthOfFieldState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				target.depthOfFieldState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetComputeRootSignature(depthOfFieldShader_.GetRootSignature());
			cmd->SetPipelineState(depthOfFieldPipelineState);
			cmd->Dispatch((width_ + 7) / 8, (height_ + 7) / 8, 1);
			ProfilerStats::AddDrawCall();

			if (bokehEnabled && bokehPipelineState)
			{
				UnorderedAccessBarrier(cmdList, target.depthOfFieldResource_.Get());

				cmd->SetComputeRootSignature(bokehShader_.GetRootSignature());
				cmd->SetPipelineState(bokehPipelineState);
				cmd->Dispatch((width_ + 7) / 8, (height_ + 7) / 8, 1);
				ProfilerStats::AddDrawCall();
			}

			cmdList->Barrier(target.depthOfFieldResource_.Get(), target.depthOfFieldState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			target.depthOfFieldState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		/// [JP] ToneMappingCS.hlsl より前に走らせる — そちらがこのバッファを
		///      露出適用前のHDRカラーへ加算するため。無効時はディスパッチごと
		///      スキップする(PrepareView が values.lensFlare_.enabled_ を
		///      落としているので、ToneMappingCS.hlsl 側も読み取りをスキップし、
		///      古いデータが表示に混ざることはない)。
		if (lensFlareEnabled && lensFlarePipelineState)
		{
			if (target.lensFlareState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				cmdList->Barrier(target.lensFlareResource_.Get(), target.lensFlareState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				target.lensFlareState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			cmd->SetComputeRootSignature(lensFlareShader_.GetRootSignature());
			cmd->SetPipelineState(lensFlarePipelineState);
			Uint32 lensFlareDispatchWidth = Max<Uint32>(1, width_ / 4);
			Uint32 lensFlareDispatchHeight = Max<Uint32>(1, height_ / 4);
			cmd->Dispatch((lensFlareDispatchWidth + 7) / 8, (lensFlareDispatchHeight + 7) / 8, 1);
			ProfilerStats::AddDrawCall();

			cmdList->Barrier(target.lensFlareResource_.Get(), target.lensFlareState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			target.lensFlareState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (toneMappingPipelineState)
		{
			cmd->SetComputeRootSignature(toneMappingShader_.GetRootSignature());
			cmd->SetPipelineState(toneMappingPipelineState);
			cmd->Dispatch((dispatchWidth + 7) / 8, (dispatchHeight + 7) / 8, 1);
			ProfilerStats::AddDrawCall();
		}

		cmdList->Barrier(outputResource.Get(), outputState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		cmdList->Barrier(sourceColorResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	ID3D12Resource* PostProcessRenderer::OutputResource(RaytracingView view)const
	{
		const View& target = view == RaytracingView::Editor ? editorView_ : gameView_;
		return target.activeIsUpscaled_ ? target.outputResourceUpscaled_.Get() : target.outputResource_.Get();
	}

	Vector2 PostProcessRenderer::OutputSize()const
	{
		return Vector2(static_cast<Float>(outputWidth_), static_cast<Float>(outputHeight_));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE PostProcessRenderer::OutputRenderTargetViewHandle(RaytracingView view)const
	{
		const View& target = view == RaytracingView::Editor ? editorView_ : gameView_;
		Uint32 index = target.activeIsUpscaled_ ? target.outputRenderTargetViewIndexUpscaled_ : target.outputRenderTargetViewIndex_;
		return renderTargetViewHeap_.CPUHandle(index);
	}

	D3D12_VIEWPORT PostProcessRenderer::Viewport(RaytracingView view)const
	{
		const View& target = view == RaytracingView::Editor ? editorView_ : gameView_;

		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<Float>(target.activeIsUpscaled_ ? outputWidth_ : width_);
		viewport.Height = static_cast<Float>(target.activeIsUpscaled_ ? outputHeight_ : height_);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	/**
	* [EN]
	* Transitions view's active output chain from Dispatch()'s exit state
	* (PIXEL_SHADER_RESOURCE) to RENDER_TARGET, so a post-tonemap debug
	* overlay can draw directly onto the final display texture without
	* being affected by tone mapping/exposure. Call after Dispatch(), pair
	* with EndDebugOverlay().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* viewのアクティブな出力チェーンを、Dispatch()が抜ける時点の状態
	* (PIXEL_SHADER_RESOURCE)からRENDER_TARGETへ遷移する。これにより、
	* トーンマップ後のデバッグオーバーレイがトーンマップ/露出の影響を
	* 受けずに最終表示テクスチャへ直接描画できる。Dispatch()の後に呼び、
	* EndDebugOverlay()と対にすること。
	*/
	void PostProcessRenderer::BeginDebugOverlay(D3D12CommandList* cmdList, RaytracingView view)
	{
		View& target = ViewFor(view);

		Microsoft::WRL::ComPtr<ID3D12Resource>& outputResource = target.activeIsUpscaled_ ? target.outputResourceUpscaled_ : target.outputResource_;
		D3D12_RESOURCE_STATES& outputState = target.activeIsUpscaled_ ? target.outputStateUpscaled_ : target.outputState_;

		cmdList->Barrier(outputResource.Get(), outputState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		outputState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	/**
	* [EN]
	* Transitions view's active output chain from RENDER_TARGET back to
	* PIXEL_SHADER_RESOURCE, so RefreshImGuiOutputView can read it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* viewのアクティブな出力チェーンをRENDER_TARGETからPIXEL_SHADER_RESOURCE
	* へ戻す。RefreshImGuiOutputViewが読み取れるようにするため。
	*/
	void PostProcessRenderer::EndDebugOverlay(D3D12CommandList* cmdList, RaytracingView view)
	{
		View& target = ViewFor(view);

		Microsoft::WRL::ComPtr<ID3D12Resource>& outputResource = target.activeIsUpscaled_ ? target.outputResourceUpscaled_ : target.outputResource_;
		D3D12_RESOURCE_STATES& outputState = target.activeIsUpscaled_ ? target.outputStateUpscaled_ : target.outputState_;

		cmdList->Barrier(outputResource.Get(), outputState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void PostProcessRenderer::UnorderedAccessBarrier(D3D12CommandList* cmdList, ID3D12Resource* resource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource;
		cmdList->Get()->ResourceBarrier(1, &barrier);
	}

	PostProcessRenderer::View& PostProcessRenderer::ViewFor(RaytracingView view)
	{
		return view == RaytracingView::Editor ? editorView_ : gameView_;
	}
}
