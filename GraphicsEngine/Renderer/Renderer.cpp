#include <GraphicsEngine/Renderer/Renderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <GraphicsEngine/Movie/MovieResource.h>
#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	Renderer::Renderer()
	{
		modelRenderer_ = MakePtr<ModelRenderer>(rootSignature_, pipelineStateObject_);
		imageRenderer_ = MakePtr<ImageRenderer>(rootSignature_, pipelineStateObject_);
		fontRenderer_ = MakePtr<FontRenderer>(rootSignature_, pipelineStateObject_);
		movieRenderer_ = MakePtr<MovieRenderer>(rootSignature_, pipelineStateObject_);
		outlineRenderer_ = MakePtr<OutlineRenderer>(rootSignature_, pipelineStateObject_);
		colliderRenderer_ = MakePtr<ColliderRenderer>();
		raytracingRenderer_ = MakePtr<RaytracingRenderer>(rootSignature_, pipelineStateObject_, raytracingStateObject_);
		skyRenderer_ = MakePtr<SkyRenderer>();
		timelineRenderer_ = MakePtr<TimelineRenderer>(rootSignature_, pipelineStateObject_);
		modelTransformRenderer_ = MakePtr<ModelTransformRenderer>(rootSignature_, pipelineStateObject_);
		effekseerRenderer_ = MakePtr<EffekseerRenderer>();
		effekseerManager_ = MakePtr<EffekseerManager>();
		postProcessRenderer_ = MakePtr<PostProcessRenderer>(rootSignature_, pipelineStateObject_);
		dlssRayReconstructionRenderer_ = MakePtr<DlssRayReconstructionRenderer>(rootSignature_, pipelineStateObject_);
		taauUpsamplingRenderer_ = MakePtr<TaauUpsamplingRenderer>(rootSignature_, pipelineStateObject_);
		materialResolveShader_ = MakePtr<MaterialResolveShader>(rootSignature_, pipelineStateObject_);
	}

	void Renderer::Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 swapBufferCount, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		device_ = device;
		nativeWidth_ = width;
		nativeHeight_ = height;

		effekseerRenderer_->Create(device, commandQueue, swapBufferCount, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT);
		effekseerManager_->Initialize(*effekseerRenderer_);

		indicesSystem_ = MakePtr<IndicesSystem>(device, bindlessHeap);

		RasterizerState::Create();
		BlendState::Create();
		DepthStencilState::Create();

		editorRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		editorDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		gameRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		gameDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		canvasRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		canvasDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

		editorFrameBuffer_ = MakePtr<FrameBuffer>(device, &editorRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &editorDepthStencilViewHeap_);
		gameFrameBuffer_ = MakePtr<FrameBuffer>(device, &gameRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &gameDepthStencilViewHeap_);
		canvasFrameBuffer_ = MakePtr<FrameBuffer>(device, &canvasRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &canvasDepthStencilViewHeap_);

		geometryBuffer_.Create(device, bindlessHeap, width, height);
		materialSortBuffer_.Create(device, bindlessHeap, *indicesSystem_, width, height);

		indicesSystem_->SetGBuffer0Index(geometryBuffer_.ColorShaderResourceViewIndex(0));
		indicesSystem_->SetGBuffer1Index(geometryBuffer_.ColorShaderResourceViewIndex(1));
		indicesSystem_->SetGBuffer2Index(geometryBuffer_.ColorShaderResourceViewIndex(2));
		indicesSystem_->SetGBuffer3Index(geometryBuffer_.ColorShaderResourceViewIndex(3));
		indicesSystem_->SetGBuffer4Index(geometryBuffer_.ColorShaderResourceViewIndex(4));
		indicesSystem_->SetGBufferDepthIndex(geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetGBufferVelocityUnorderedAccessViewIndex(geometryBuffer_.VelocityUnorderedAccessViewIndex());
		indicesSystem_->SetGBuffer0UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(0));
		indicesSystem_->SetGBuffer1UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(1));
		indicesSystem_->SetGBuffer3UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(3));

		hiZBuffer_.Create(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, width, height, geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetHiZIndex(hiZBuffer_.ShaderResourceViewIndex());

		debugDepthResizeBuffer_.Create(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, width, height);

		/// [EN] Not a frame-ring resource — its SRV index is stable across frames.
		/// [JP] フレームリングではないため SRV インデックスは毎フレーム固定。
		selectionMaskRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		selectionMaskFrameBuffer_ = MakePtr<FrameBuffer>(device, &selectionMaskRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R8_UNORM, nullptr, 0.0f, 0.0f, 0.0f, 0.0f);
		indicesSystem_->SetSelectionMaskIndex(selectionMaskFrameBuffer_->ColorShaderResourceViewIndex());

		lightSystem_ = MakePtr<LightSystem>(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, width, height);
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());

		modelRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_, width, height);
		imageRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_);
		fontRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_);
		movieRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_);
		outlineRenderer_->Create(device, bindlessHeap, shaderCache);
		colliderRenderer_->Create(device, bindlessHeap, shaderCache);
		raytracingRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_, width, height);
		skyRenderer_->Create(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_);
		timelineRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		modelTransformRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		postProcessRenderer_->Create(device, bindlessHeap, shaderCache, width, height, width, height);
		dlssRayReconstructionRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_, width, height, width, height);
		taauUpsamplingRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		materialResolveShader_->Create(shaderCache, device);

		gpuProfiler_.Create(device, commandQueue, swapBufferCount);
	}

	void Renderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight)
	{
		device_ = device;
		bindlessHeap_ = bindlessHeap;
		nativeWidth_ = nativeWidth;
		nativeHeight_ = nativeHeight;

		editorRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		editorDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		gameRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		gameDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		canvasRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		canvasDepthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

		editorFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		gameFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		canvasFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);

		geometryBuffer_.Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		materialSortBuffer_.Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight);

		indicesSystem_->SetGBuffer0Index(geometryBuffer_.ColorShaderResourceViewIndex(0));
		indicesSystem_->SetGBuffer1Index(geometryBuffer_.ColorShaderResourceViewIndex(1));
		indicesSystem_->SetGBuffer2Index(geometryBuffer_.ColorShaderResourceViewIndex(2));
		indicesSystem_->SetGBuffer3Index(geometryBuffer_.ColorShaderResourceViewIndex(3));
		indicesSystem_->SetGBuffer4Index(geometryBuffer_.ColorShaderResourceViewIndex(4));
		indicesSystem_->SetGBufferDepthIndex(geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetGBufferVelocityUnorderedAccessViewIndex(geometryBuffer_.VelocityUnorderedAccessViewIndex());
		indicesSystem_->SetGBuffer0UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(0));
		indicesSystem_->SetGBuffer1UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(1));
		indicesSystem_->SetGBuffer3UnorderedAccessViewIndex(geometryBuffer_.ColorUnorderedAccessViewIndex(3));

		hiZBuffer_.Resize(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, nativeWidth, nativeHeight, geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetHiZIndex(hiZBuffer_.ShaderResourceViewIndex());

		debugDepthResizeBuffer_.Resize(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, outputWidth, outputHeight);

		selectionMaskRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		selectionMaskFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		indicesSystem_->SetSelectionMaskIndex(selectionMaskFrameBuffer_->ColorShaderResourceViewIndex());

		lightSystem_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());

		modelRenderer_->Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight);
		raytracingRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		timelineRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		modelTransformRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		postProcessRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight, outputWidth, outputHeight);
		dlssRayReconstructionRenderer_->Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight, outputWidth, outputHeight);
		taauUpsamplingRenderer_->Resize(device, bindlessHeap, outputWidth, outputHeight);
	}

	void Renderer::SetDlssManager(DlssManager* dlssManager)
	{
		dlssManager_ = dlssManager;
	}

	Vector2 Renderer::PostProcessOutputSize()const
	{
		return postProcessRenderer_->OutputSize();
	}

	const GpuProfiler& Renderer::GetGpuProfiler()const
	{
		return gpuProfiler_;
	}

	void Renderer::BeginEditorFrame(D3D12CommandList* cmdList)
	{
		ProfilerStats::Reset();

		/// [JP] GPU プロファイラのフレーム境界。エンジンのフレームループは常に
		///      EditorRender→GameRender→CanvasRender の順なので、ここが
		///      「フレームの先頭」= 前フレームの resolve を積む場所になる。
		gpuProfiler_.Advance(cmdList);

		editorFrameBuffer_->Begin(cmdList);
		editorFrameBuffer_->Clear(cmdList);
	}

	void Renderer::EndEditorFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		editorFrameBuffer_->End(cmdList);

		ID3D12Resource* postProcessSource = editorFrameBuffer_->ColorResource();

		if (dlssRayReconstructionEnabled_ && dlssManager_)
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Editor, GpuProfileScope::DlssRayReconstruction);
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, dlssManager_, scene, editorFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(2), nativeWidth_, nativeHeight_, upscaleMode_);
			gpuProfiler_.End(cmdList, GpuProfileView::Editor, GpuProfileScope::DlssRayReconstruction);

			postProcessSource = dlssRayReconstructionRenderer_->OutputResource(RaytracingView::Editor);
		}
		else
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Editor, GpuProfileScope::Taau);
			taauUpsamplingRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, geometryBuffer_.ColorResource(2), editorFrameBuffer_->ColorShaderResourceViewIndex(), geometryBuffer_.DepthShaderResourceViewIndex(), geometryBuffer_.ColorShaderResourceViewIndex(2), nativeWidth_, nativeHeight_);
			gpuProfiler_.End(cmdList, GpuProfileView::Editor, GpuProfileScope::Taau);

			postProcessSource = taauUpsamplingRenderer_->OutputResource(RaytracingView::Editor);
		}

		gpuProfiler_.Begin(cmdList, GpuProfileView::Editor, GpuProfileScope::PostProcess);
		postProcessRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, postProcessSource, true);
		gpuProfiler_.End(cmdList, GpuProfileView::Editor, GpuProfileScope::PostProcess);

		/// [EN] Debug overlay: draws directly onto PostProcess's tone-mapped
		///      display texture, after PostProcess (and every raytracing
		///      effect that fed into it) has already run - so neither
		///      exposure/tone mapping/bloom nor any RT effect alters these
		///      colors, unlike when they used to draw into editorFrameBuffer_
		///      inside EditorFlush (see the comment there).
		/// [EN] The collider wireframe is depth-tested against
		///      debugDepthResizeBuffer_'s point-resampled copy of
		///      geometryBuffer_'s native depth (see DepthResizeCS.hlsl) -
		///      PostProcess's active output chain is always the upscaled
		///      (outputWidth_/outputHeight_) one now (DLSS-RR or TAAU, see
		///      dlssRayReconstructionEnabled_), so binding geometryBuffer_'s
		///      native depth directly against it would depth-test at the
		///      wrong pixel coordinates.
		/// [EN] The selection outline's pixel shader still indexes
		///      selectionMaskFrameBuffer_ (native resolution) with SV_Position,
		///      but now rescales it by SceneConstantBuffer's
		///      screen_size_/display_size_ first (see SelectionOutlinePS.hlsl
		///      and where editorSceneConstantBuffer.displaySize_ is set in
		///      Graphics::EditorRender), so it stays correct at whichever
		///      resolution this pass is actually drawing at.
		/// [JP] デバッグオーバーレイ: PostProcess(およびそこへ流れ込む全ての
		///      レイトレーシングエフェクト)が既に実行された後、PostProcess の
		///      トーンマップ済み表示テクスチャへ直接描画する - そのため
		///      露出/トーンマップ/ブルームもRTエフェクトも、これらの色には
		///      一切影響しない。以前 EditorFlush 内で editorFrameBuffer_ へ
		///      描画していた時とは異なる(そちらのコメント参照)。
		/// [JP] コライダーワイヤーフレームは、debugDepthResizeBuffer_ が
		///      geometryBuffer_ のネイティブ深度をポイントリサンプルした
		///      コピー(DepthResizeCS.hlsl参照)に対して深度テストされる -
		///      PostProcess のアクティブな出力チェーンは(DLSS-RR/TAAUどちらでも、
		///      dlssRayReconstructionEnabled_ 参照)常にアップスケール版
		///      (outputWidth_/outputHeight_)になったため、そこへ geometryBuffer_
		///      のネイティブ深度を直接バインドすると誤ったピクセル座標で
		///      深度テストしてしまう。
		/// [JP] 選択アウトラインのピクセルシェーダは、いまも
		///      selectionMaskFrameBuffer_(ネイティブ解像度)をSV_Positionで
		///      インデックスするが、まずSceneConstantBufferの
		///      screen_size_/display_size_で座標をスケールし直すようになった
		///      (SelectionOutlinePS.hlsl、およびGraphics::EditorRenderで
		///      editorSceneConstantBuffer.displaySize_を設定している箇所を
		///      参照) - これによりこのパスが実際にどの解像度で描画していても
		///      正しく動作する。
		postProcessRenderer_->BeginDebugOverlay(cmdList, RaytracingView::Editor);

		D3D12_CPU_DESCRIPTOR_HANDLE debugRenderTargetView = postProcessRenderer_->OutputRenderTargetViewHandle(RaytracingView::Editor);
		D3D12_VIEWPORT debugViewport = postProcessRenderer_->Viewport(RaytracingView::Editor);

		debugDepthResizeBuffer_.Dispatch(cmdList, bindlessHeap_->Heap(), geometryBuffer_, nativeWidth_, nativeHeight_);
		colliderRenderer_->Draw(cmdList, debugRenderTargetView, debugDepthResizeBuffer_.DepthStencilViewHandle(), debugViewport, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress());
		geometryBuffer_.BeginDepth(cmdList);

		outlineRenderer_->Draw(cmdList, debugRenderTargetView, debugViewport, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress());

		postProcessRenderer_->EndDebugOverlay(cmdList, RaytracingView::Editor);

		RefreshImGuiOutputView(RaytracingView::Editor);
	}

	void Renderer::BeginGameFrame(D3D12CommandList* cmdList)
	{
		gameFrameBuffer_->Begin(cmdList);
		gameFrameBuffer_->Clear(cmdList);
	}

	void Renderer::EndGameFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		gameFrameBuffer_->End(cmdList);

		ID3D12Resource* postProcessSource = gameFrameBuffer_->ColorResource();

		if (dlssRayReconstructionEnabled_ && dlssManager_)
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Game, GpuProfileScope::DlssRayReconstruction);
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, dlssManager_, scene, gameFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(2), nativeWidth_, nativeHeight_, upscaleMode_);
			gpuProfiler_.End(cmdList, GpuProfileView::Game, GpuProfileScope::DlssRayReconstruction);

			postProcessSource = dlssRayReconstructionRenderer_->OutputResource(RaytracingView::Game);
		}
		else
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Game, GpuProfileScope::Taau);
			taauUpsamplingRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, geometryBuffer_.ColorResource(2), gameFrameBuffer_->ColorShaderResourceViewIndex(), geometryBuffer_.DepthShaderResourceViewIndex(), geometryBuffer_.ColorShaderResourceViewIndex(2), nativeWidth_, nativeHeight_);
			gpuProfiler_.End(cmdList, GpuProfileView::Game, GpuProfileScope::Taau);

			postProcessSource = taauUpsamplingRenderer_->OutputResource(RaytracingView::Game);
		}

		gpuProfiler_.Begin(cmdList, GpuProfileView::Game, GpuProfileScope::PostProcess);
		postProcessRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, postProcessSource, true);
		gpuProfiler_.End(cmdList, GpuProfileView::Game, GpuProfileScope::PostProcess);

		/// [EN] Sprite/Billboard: draws directly onto PostProcess's tone-mapped
		///      display texture, after DLSS Ray Reconstruction and PostProcess
		///      have both already run - so DLSS-RR's temporal reprojection
		///      never sees these pixels (they carry no motion vectors, so
		///      DLSS-RR would otherwise ghost them whenever the camera or the
		///      sprite itself moves). Same pattern as EditorFlush's debug
		///      overlay (collider wireframe/selection outline) - see
		///      EndEditorFrame.
		/// [JP] Sprite/Billboard: DLSS Ray ReconstructionとPostProcessが両方
		///      既に実行された後、PostProcessのトーンマップ済み表示テクスチャへ
		///      直接描画する - こうすることでDLSS-RRの時間的再投影がこれらの
		///      ピクセルを一切見ない(モーションベクターを持たないため、
		///      そのままだとカメラやSprite自身が動くたびにゴーストしていた)。
		///      EditorFlushのデバッグオーバーレイ(コライダーワイヤーフレーム/
		///      選択アウトライン)と同じパターン - EndEditorFrame参照。
		postProcessRenderer_->BeginDebugOverlay(cmdList, RaytracingView::Game);

		D3D12_CPU_DESCRIPTOR_HANDLE gameDisplayRenderTargetView = postProcessRenderer_->OutputRenderTargetViewHandle(RaytracingView::Game);
		D3D12_VIEWPORT gameDisplayViewport = postProcessRenderer_->Viewport(RaytracingView::Game);
		cmdList->Get()->OMSetRenderTargets(1, &gameDisplayRenderTargetView, FALSE, nullptr);
		cmdList->Get()->RSSetViewports(1, &gameDisplayViewport);
		D3D12_RECT gameDisplayScissorRect = { 0, 0, static_cast<LONG>(gameDisplayViewport.Width), static_cast<LONG>(gameDisplayViewport.Height) };
		cmdList->Get()->RSSetScissorRects(1, &gameDisplayScissorRect);

		ID3D12DescriptorHeap* spriteHeap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS spriteConstantAddr = indicesSystem_->GameConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS spriteStructuredAddr = indicesSystem_->StructuredAddress();

		imageRenderer_->Upload();
		imageRenderer_->DrawSprite(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);
		imageRenderer_->DrawBillboard(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);

		fontRenderer_->Upload();
		fontRenderer_->DrawSprite(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);
		fontRenderer_->DrawBillboard(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);

		movieRenderer_->Upload();
		movieRenderer_->DrawBillboard(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);
		movieRenderer_->DrawSprite(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);
		movieRenderer_->DrawFullscreen(cmdList->Get(), spriteHeap, spriteConstantAddr, spriteStructuredAddr);

		postProcessRenderer_->EndDebugOverlay(cmdList, RaytracingView::Game);

		RefreshImGuiOutputView(RaytracingView::Game);
	}

	void Renderer::BeginCanvasFrame(D3D12CommandList* cmdList)
	{
		canvasFrameBuffer_->Begin(cmdList);
		canvasFrameBuffer_->Clear(cmdList);
	}

	void Renderer::EndCanvasFrame(D3D12CommandList* cmdList)
	{
		canvasFrameBuffer_->End(cmdList);
	}

	void Renderer::BeginTimelineFrame(D3D12CommandList* cmdList)
	{
		timelineRenderer_->Begin(cmdList);
	}

	void Renderer::EndTimelineFrame(D3D12CommandList* cmdList)
	{
		timelineRenderer_->End(cmdList);
	}

	void Renderer::BeginModelTransformFrame(D3D12CommandList* cmdList)
	{
		modelTransformRenderer_->Begin(cmdList);
	}

	void Renderer::EndModelTransformFrame(D3D12CommandList* cmdList)
	{
		modelTransformRenderer_->End(cmdList);
	}

	void Renderer::Gather(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, const SceneConstantBuffer& scene, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity)
	{
		colliderRenderer_->Clear();
		for (const ColliderInstance& instance : colliderInstances)
		{
			colliderRenderer_->AddInstance(static_cast<ColliderShapeKind>(instance.shapeKind_), instance.position_, instance.rotation_, instance.dimensions_, instance.color_);
		}

		constraintSystem_.Execute(world);

		ModelResource* modelResource = resourceCache.GetModelResource();
		AnimationResource* animationResource = resourceCache.GetAnimationResource();
		modelRenderer_->Gather(loaderSystem, *modelResource, *animationResource, world, scene, selectedEntity);
		raytracingRenderer_->Gather(loaderSystem, *modelResource, world, *modelRenderer_);

		ImageResource* imageResource = resourceCache.GetImageResource();
		Vector2 gameDisplaySize = PostProcessOutputSize();
		imageRenderer_->Gather(loaderSystem, *imageResource, world, gameDisplaySize, selectedEntity);

		FontResource* fontResource = resourceCache.GetFontResource();
		fontRenderer_->Gather(*fontResource, world, gameDisplaySize, selectedEntity);

		MovieResource* movieResource = resourceCache.GetMovieResource();
		movieRenderer_->Gather(*movieResource, world, gameDisplaySize, selectedEntity);

		celestialResult_ = CelestialSystem::Compute(daySystem_, sunLight_, moonLight_);
		Bool sunOverride = daySystemEnabled_ && sunLightEnabled_;
		weatherState_ = WeatherSystem::ReadGpuState(world);
		lightSystem_->Gather(loaderSystem, *modelResource, world, sunOverride ? &celestialResult_ : nullptr, &weatherState_);

		lastCameraPosition_ = Vector3(scene.cameraPosition_.x, scene.cameraPosition_.y, scene.cameraPosition_.z);

		skyRenderer_->Gather(loaderSystem, resourceCache, world);

		postProcessRenderer_->Gather(world);

		effekseerRenderer_->SetCamera(scene);
	}

	void Renderer::GatherTimelinePreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
	{
		ModelResource* modelResource = resourceCache.GetModelResource();
		AnimationResource* animationResource = resourceCache.GetAnimationResource();
		timelineRenderer_->Gather(loaderSystem, *modelResource, *animationResource, meshAssetId, animationAssetId, time, worldMatrix);
	}

	void Renderer::GatherModelTransformPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
	{
		ModelResource* modelResource = resourceCache.GetModelResource();
		AnimationResource* animationResource = resourceCache.GetAnimationResource();
		modelTransformRenderer_->Gather(loaderSystem, *modelResource, *animationResource, meshAssetId, animationAssetId, time, worldMatrix);
	}

	void Renderer::SetRaytracingSettings(const RaytracingContext& settings)
	{
		raytracingRenderer_->SetRaytracingSettings(settings);
		dlssRayReconstructionEnabled_ = settings.dlssRayReconstructionEnabled_;
		upscaleMode_ = settings.upscaleMode_;

		daySystemEnabled_ = settings.daySystemEnabled_;
		daySystem_ = settings.daySystem_;
		sunLightEnabled_ = settings.sunLightEnabled_;
		sunLight_ = settings.sunLight_;
		moonLightEnabled_ = settings.moonLightEnabled_;
		moonLight_ = settings.moonLight_;

		/// [JP] プロシージャル空(雲機能)が有効な間、SkyRenderer にも伝えて
		///      スカイマップ無効時の IBL を解析的な空から生成させる。ハッシュは
		///      空/雲設定のバイト列の FNV-1a — 変化した時だけ再畳み込みが走る
		///      (太陽の回転はハッシュ外なので SkyRenderer 側の定期リフレッシュが
		///      拾う)。lightIndex は SetIndices と同じ LightSystem の定数
		///      インデックス。
		const Uint8* bytes = reinterpret_cast<const Uint8*>(&settings.volumetricCloudScapes_);
		Uint32 hash = 2166136261u;
		for (Size byteIndex = 0; byteIndex < sizeof(settings.volumetricCloudScapes_); byteIndex++)
		{
			hash = (hash ^ bytes[byteIndex]) * 16777619u;
		}

		/// [JP] 時刻(風スクロール)は EditorFlush で蓄積した skyTotalTime_ を渡す
		///      (1フレーム遅れだが定期リフレッシュ間隔からすれば誤差)。
		skyRenderer_->SetProceduralSky(settings.volumetricCloudScapesEnabled_, hash, lightSystem_ ? lightSystem_->GetIndex() : 0, skyTotalTime_);
	}

	void Renderer::EditorFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime, ViewMode viewMode)
	{
		skyTotalTime_ += deltaTime;

		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = indicesSystem_->EditorConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = indicesSystem_->StructuredAddress();

		indicesSystem_->SetEditorViewMode(static_cast<Uint>(viewMode));
		indicesSystem_->SetEditorSceneIndex(sceneSystem->GetIndex());
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());
		skyRenderer_->SetIndices(*indicesSystem_);
		lightSystem_->Upload();
		modelRenderer_->Upload();

		/// [EN] BLAS/TLAS build (and prepping every RT-effect renderer's per-frame
		///      data, e.g. ShadowRenderer::PrepareFrame) has no G-Buffer
		///      dependency, so it can run here — before UploadEditor bakes this
		///      frame's structured indices, which is where the TLAS SRV index
		///      needs to already be set. The actual shadow ray dispatch runs
		///      later, once the G-Buffer's depth/normal exist (see below).
		/// [JP] BLAS/TLAS 構築(および各 RT エフェクトレンダラーの毎フレームデータ
		///      準備、例: ShadowRenderer::PrepareFrame)は G-Buffer に依存しない
		///      ので、UploadEditor が今フレームの structured indices を確定する
		///      前 — TLAS SRV インデックスが既に設定済みである必要がある地点 —
		///      でここで実行できる。実際のシャドウレイディスパッチは、G-Buffer
		///      の深度/法線が存在するようになった後で行う（下記参照）。
		raytracingRenderer_->Build(cmdList, device_, *modelRenderer_, deltaTime, celestialResult_.nightFactor_, lastCameraPosition_, skyTotalTime_, weatherState_.snowIntensity_);

		Uint32 editorPostProcessSourceColorIndex;
		if (dlssRayReconstructionEnabled_)
		{
			editorPostProcessSourceColorIndex = dlssRayReconstructionRenderer_->OutputShaderResourceViewIndex(RaytracingView::Editor);
		}
		else
		{
			taauUpsamplingRenderer_->PrepareView(RaytracingView::Editor);
			editorPostProcessSourceColorIndex = taauUpsamplingRenderer_->OutputShaderResourceViewIndex(RaytracingView::Editor);
		}
		postProcessRenderer_->PrepareView(*indicesSystem_, RaytracingView::Editor, editorPostProcessSourceColorIndex, true);

		indicesSystem_->UploadEditor();

		const GpuProfileView profileView = GpuProfileView::Editor;

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::DepthPrepass);
		geometryBuffer_.BeginDepthOnly(cmdList);
		geometryBuffer_.ClearDepth(cmdList);
		modelRenderer_->DrawDepthPrepass(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::DepthPrepass);

		/// [EN] Build the Hi-Z pyramid from the prepass depth so the G-Buffer
		///      and transparent passes can occlusion-cull against it.
		/// [JP] プリパス深度から Hi-Z ピラミッドを構築し、G-Buffer パスと
		///      透過パスがオクルージョンカリングに使えるようにする。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::HiZBuild);
		hiZBuffer_.Build(cmdList, geometryBuffer_, heap);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::HiZBuild);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::GeometryBuffer);
		geometryBuffer_.BeginVisibility(cmdList);
		geometryBuffer_.ClearVisibility(cmdList);
		modelRenderer_->DrawOpaque(cmdList, heap, constantAddr, structuredAddr);
		geometryBuffer_.End(cmdList);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::GeometryBuffer);

		/// [JP] VisibilityBuffer マテリアル解決: RT4(visibility id)+depth から
		///      RT0/1/2/3 を書き直す。RT0-3 は geometryBuffer_.End() で
		///      PIXEL_SHADER_RESOURCE へ遷移済みなので、一時的に
		///      UNORDERED_ACCESS へ戻し、書き終えたらまた PIXEL_SHADER_RESOURCE へ
		///      戻す(DlssBackgroundVelocityCS と同じやり方 - GeometryBuffer 側の
		///      状態追跡と整合するよう、進入時と同じ状態で抜ける)。以降の
		///      シャドウ/AO/反射/GI レイトレパスと DeferredLightingPS が RT0-3 を
		///      PIXEL_SHADER_RESOURCE として読むため、それより前に実行する。
		///      マテリアルソート(Classify→PrefixSum→Scatter)→Resolve の4パス構成 -
		///      Model/Material/MaterialResolveShader.h 参照。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::MaterialResolve);
		{
			ID3D12PipelineState* classifyPipelineState = materialResolveShader_->GetClassifyPipelineState();
			ID3D12PipelineState* prefixSumPipelineState = materialResolveShader_->GetPrefixSumPipelineState();
			ID3D12PipelineState* scatterPipelineState = materialResolveShader_->GetScatterPipelineState();
			ID3D12PipelineState* resolvePipelineState = materialResolveShader_->GetPipelineState();
			if (classifyPipelineState && prefixSumPipelineState && scatterPipelineState && resolvePipelineState)
			{
				ID3D12GraphicsCommandList* cmd = cmdList->Get();

				cmdList->Barrier(geometryBuffer_.ColorResource(0), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(1), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(2), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(3), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				materialSortBuffer_.Clear(cmd);
				materialSortBuffer_.Barrier(cmd);

				ID3D12DescriptorHeap* heaps[] = { heap };
				cmd->SetDescriptorHeaps(_countof(heaps), heaps);
				cmd->SetComputeRootSignature(materialResolveShader_->GetRootSignature());
				cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
				cmd->SetComputeRootConstantBufferView(2, constantAddr);
				cmd->SetComputeRootConstantBufferView(3, structuredAddr);

				/// [JP] パス1/4: バケットごとのピクセル数を数える。全画面走査する
				///      唯一のパスなので、背景ピクセルのRT0-3ゼロ書きもここで行う。
				cmd->SetPipelineState(classifyPipelineState);
				cmd->Dispatch((nativeWidth_ + 7) / 8, (nativeHeight_ + 7) / 8, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				/// [JP] パス2/4: バケットカウント→排他的スキャンのオフセットへ(1ディスパッチ)。
				cmd->SetPipelineState(prefixSumPipelineState);
				cmd->Dispatch(1, 1, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				/// [JP] パス3/4: 各前景ピクセルをそのバケット範囲へ書き出す。
				cmd->SetPipelineState(scatterPipelineState);
				cmd->Dispatch((nativeWidth_ + 7) / 8, (nativeHeight_ + 7) / 8, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				/// [JP] パス4/4: ソート済みリストを1Dで辿ってマテリアルを解決する。
				Uint32 totalPixels = nativeWidth_ * nativeHeight_;
				cmd->SetPipelineState(resolvePipelineState);
				cmd->Dispatch((totalPixels + 63) / 64, 1, 1);
				ProfilerStats::AddDrawCall();

				cmdList->Barrier(geometryBuffer_.ColorResource(0), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(1), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(2), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(3), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			}
		}
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::MaterialResolve);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::LightCluster);
		lightSystem_->DispatchCluster(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::LightCluster);

		/// [JP] G-Buffer の深度/法線と、このビューのクラスタライトリスト
		///      (ShadowRT の Point/Spot/Rect 確率サンプリングが読む)が揃った
		///      ので、シャドウレイをディスパッチする。クラスタより前に走らせる
		///      と前フレーム(しかも別ビュー)のライトリストを読んでしまう。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceShadow);
		raytracingRenderer_->DispatchShadow(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Editor);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceShadow);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceAmbientOcclusion);
		raytracingRenderer_->DispatchAmbientOcclusion(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Editor);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceAmbientOcclusion);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceSubsurfaceScattering);
		raytracingRenderer_->DispatchSubsurfaceScattering(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceSubsurfaceScattering);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceReflection);
		raytracingRenderer_->DispatchReflection(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Editor);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceReflection);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceRefraction);
		raytracingRenderer_->DispatchRefraction(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceRefraction);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceGlobalIllumination);
		raytracingRenderer_->DispatchGlobalIllumination(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Editor);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceGlobalIllumination);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricCloudScapes);
		raytracingRenderer_->DispatchVolumetricCloudScapes(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricCloudScapes);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricStar);
		raytracingRenderer_->DispatchVolumetricStar(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricStar);

		/// [JP] 天候パーティクルのシミュレートは共有のワールド空間状態を進める
		///      だけなので、フレームに1回(EditorFlushのみ)実行する。描画は
		///      ビューごとに別途行う(下の DrawTransparent 付近参照)。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::WeatherParticle);
		raytracingRenderer_->SimulateWeatherParticles(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::WeatherParticle);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricLight);
		raytracingRenderer_->DispatchVolumetricLight(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricLight);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::SkyGenerate);
		skyRenderer_->Generate(cmdList, heap, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::SkyGenerate);

		/// [JP] ワイヤーフレーム / メッシュレット表示: Lit 合成と透明を飛ばし、
		///      クリア済みフレームバッファにデバッグ描画だけを行う（シーン深度で遮蔽）。
		///      それ以外は通常。
		if (viewMode == ViewMode::Wireframe)
		{
			geometryBuffer_.BeginDepth(cmdList);
			modelRenderer_->DrawWireframe(cmdList, editorFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);
			geometryBuffer_.EndDepth(cmdList);
		}
		else if (viewMode == ViewMode::Meshlet)
		{
			geometryBuffer_.BeginDepth(cmdList);
			modelRenderer_->DrawMeshlet(cmdList, editorFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);
			geometryBuffer_.EndDepth(cmdList);
		}
		else
		{
			modelRenderer_->Compose(cmdList, editorFrameBuffer_.get(), heap, constantAddr, structuredAddr);

			geometryBuffer_.BeginDepth(cmdList);
			modelRenderer_->DrawTransparent(cmdList, editorFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);

			/// [JP] 雨/雪パーティクル: 不透明合成+透明の後、既存の深度に対して
			///      画素単位で遮蔽判定しながら加算合成する。DrawTransparent が
			///      自分で EndDepth 済みなので、ここで改めて Begin/End する
			///      (下のコライダーデバッグ描画と同じ作法)。
			gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::WeatherParticle);
			geometryBuffer_.BeginDepth(cmdList);
			raytracingRenderer_->DrawWeatherParticles(cmdList, editorFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);
			geometryBuffer_.EndDepth(cmdList);
			gpuProfiler_.End(cmdList, profileView, GpuProfileScope::WeatherParticle);
		}

		/// [EN] Collider/constraint debug wireframe and the selection outline
		///      both used to draw here (into editorFrameBuffer_, before
		///      EndEditorFrame's tone mapping/PostProcess) - which meant
		///      exposure/tone mapping/bloom altered their color, exactly like
		///      the rest of the lit scene. They are now drawn in
		///      EndEditorFrame, after PostProcess, directly onto the final
		///      display texture instead (see BeginDebugOverlay/EndDebugOverlay
		///      there). Only the selection mask itself (what the outline pass
		///      reads) still needs to be built here, before Model/Billboard
		///      finish drawing.
		/// [JP] コライダー/コンストレイントのデバッグワイヤーフレームと選択
		///      アウトラインは、以前はここ(EndEditorFrameのトーンマップ/
		///      PostProcessより前、editorFrameBuffer_)へ描画していた —
		///      つまり露出/トーンマップ/ブルームが、他のライティング済み
		///      シーンと全く同じようにこれらの色にも影響していた。現在は
		///      EndEditorFrame内、PostProcessの後に、最終表示テクスチャへ
		///      直接描画する(そちらのBeginDebugOverlay/EndDebugOverlay参照)。
		///      ここで構築が必要なのは、アウトラインパスが読み取る選択マスク
		///      自体のみ(Model/Billboardの描画が終わる前に)。
		/// [JP] 選択アウトライン用マスク: EditorFlush が実際に描画するのは Model と
		///      Billboard（Image の Billboard と、Font の Billboard = 3D ワールド
		///      テキスト）。Sprite と Font の Sprite（2D UI テキスト）は
		///      Canvas/Game 専用で EditorFlush では描かれないので、マスクへ
		///      書き込むのもこの 2 種だけ。Sprite 系の選択アウトラインは
		///      CanvasFlush 側で同じマスクを使い回して行う。
		selectionMaskFrameBuffer_->Begin(cmdList);
		selectionMaskFrameBuffer_->Clear(cmdList, 0.0f, 0.0f, 0.0f, 0.0f);
		modelRenderer_->DrawSelectionMask(cmdList, heap, constantAddr, structuredAddr);
		imageRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		fontRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		selectionMaskFrameBuffer_->End(cmdList);

		imageRenderer_->Upload();
		imageRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		/// [JP] Font の Billboard（3D ワールドテキスト）のみ。Sprite（2D UI テキスト）は
		///      Canvas/Game 専用なのでここでは描かない。
		fontRenderer_->Upload();
		fontRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		/// [JP] Movie の Billboard（3D ワールド動画）のみ。Sprite/Fullscreen は
		///      Canvas/Game 専用なのでここでは描かない。
		movieRenderer_->Upload();
		movieRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
	}

	void Renderer::GameFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = indicesSystem_->GameConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = indicesSystem_->StructuredAddress();

		indicesSystem_->SetGameSceneIndex(sceneSystem->GetIndex());
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());
		skyRenderer_->SetIndices(*indicesSystem_);
		lightSystem_->Upload();
		modelRenderer_->Upload();
		Uint32 gamePostProcessSourceColorIndex;
		if (dlssRayReconstructionEnabled_)
		{
			gamePostProcessSourceColorIndex = dlssRayReconstructionRenderer_->OutputShaderResourceViewIndex(RaytracingView::Game);
		}
		else
		{
			taauUpsamplingRenderer_->PrepareView(RaytracingView::Game);
			gamePostProcessSourceColorIndex = taauUpsamplingRenderer_->OutputShaderResourceViewIndex(RaytracingView::Game);
		}
		postProcessRenderer_->PrepareView(*indicesSystem_, RaytracingView::Game, gamePostProcessSourceColorIndex, true);
		indicesSystem_->UploadGame();

		const GpuProfileView profileView = GpuProfileView::Game;

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::DepthPrepass);
		geometryBuffer_.BeginDepthOnly(cmdList);
		geometryBuffer_.ClearDepth(cmdList);
		modelRenderer_->DrawDepthPrepass(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::DepthPrepass);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::HiZBuild);
		hiZBuffer_.Build(cmdList, geometryBuffer_, heap);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::HiZBuild);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::GeometryBuffer);
		geometryBuffer_.BeginVisibility(cmdList);
		geometryBuffer_.ClearVisibility(cmdList);
		modelRenderer_->DrawOpaque(cmdList, heap, constantAddr, structuredAddr);
		geometryBuffer_.End(cmdList);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::GeometryBuffer);

		/// [JP] VisibilityBuffer マテリアル解決。EditorFlush 側と同じ理由・同じ
		///      入退場状態で RT0-3 を一時的に UNORDERED_ACCESS へ戻して書く。
		///      マテリアルソート(Classify→PrefixSum→Scatter)→Resolve の4パス構成。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::MaterialResolve);
		{
			ID3D12PipelineState* classifyPipelineState = materialResolveShader_->GetClassifyPipelineState();
			ID3D12PipelineState* prefixSumPipelineState = materialResolveShader_->GetPrefixSumPipelineState();
			ID3D12PipelineState* scatterPipelineState = materialResolveShader_->GetScatterPipelineState();
			ID3D12PipelineState* resolvePipelineState = materialResolveShader_->GetPipelineState();
			if (classifyPipelineState && prefixSumPipelineState && scatterPipelineState && resolvePipelineState)
			{
				ID3D12GraphicsCommandList* cmd = cmdList->Get();

				cmdList->Barrier(geometryBuffer_.ColorResource(0), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(1), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(2), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				cmdList->Barrier(geometryBuffer_.ColorResource(3), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				materialSortBuffer_.Clear(cmd);
				materialSortBuffer_.Barrier(cmd);

				ID3D12DescriptorHeap* heaps[] = { heap };
				cmd->SetDescriptorHeaps(_countof(heaps), heaps);
				cmd->SetComputeRootSignature(materialResolveShader_->GetRootSignature());
				cmd->SetComputeRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));
				cmd->SetComputeRootConstantBufferView(2, constantAddr);
				cmd->SetComputeRootConstantBufferView(3, structuredAddr);

				cmd->SetPipelineState(classifyPipelineState);
				cmd->Dispatch((nativeWidth_ + 7) / 8, (nativeHeight_ + 7) / 8, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				cmd->SetPipelineState(prefixSumPipelineState);
				cmd->Dispatch(1, 1, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				cmd->SetPipelineState(scatterPipelineState);
				cmd->Dispatch((nativeWidth_ + 7) / 8, (nativeHeight_ + 7) / 8, 1);
				ProfilerStats::AddDrawCall();
				materialSortBuffer_.Barrier(cmd);

				Uint32 totalPixels = nativeWidth_ * nativeHeight_;
				cmd->SetPipelineState(resolvePipelineState);
				cmd->Dispatch((totalPixels + 63) / 64, 1, 1);
				ProfilerStats::AddDrawCall();

				cmdList->Barrier(geometryBuffer_.ColorResource(0), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(1), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(2), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				cmdList->Barrier(geometryBuffer_.ColorResource(3), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			}
		}
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::MaterialResolve);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::LightCluster);
		lightSystem_->DispatchCluster(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::LightCluster);

		/// [JP] TLAS 自体はビュー非依存（同じ 3D シーン）なので EditorFlush で
		///      毎フレーム 1 回だけ構築済み（Engine のフレームループは常に
		///      EditorRender→GameRender→CanvasRender の順）。ここでは Game
		///      カメラの G-Buffer とクラスタライトリストに対してシャドウレイの
		///      ディスパッチだけ行う（Editor 側と同じくクラスタの後）。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceShadow);
		raytracingRenderer_->DispatchShadow(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Game);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceShadow);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceAmbientOcclusion);
		raytracingRenderer_->DispatchAmbientOcclusion(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Game);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceAmbientOcclusion);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceSubsurfaceScattering);
		raytracingRenderer_->DispatchSubsurfaceScattering(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceSubsurfaceScattering);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceReflection);
		raytracingRenderer_->DispatchReflection(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Game);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceReflection);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceRefraction);
		raytracingRenderer_->DispatchRefraction(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceRefraction);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::RaytraceGlobalIllumination);
		raytracingRenderer_->DispatchGlobalIllumination(cmdList, heap, constantAddr, structuredAddr, RaytracingView::Game);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::RaytraceGlobalIllumination);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricCloudScapes);
		raytracingRenderer_->DispatchVolumetricCloudScapes(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricCloudScapes);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricStar);
		raytracingRenderer_->DispatchVolumetricStar(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricStar);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::VolumetricLight);
		raytracingRenderer_->DispatchVolumetricLight(cmdList, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::VolumetricLight);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::SkyGenerate);
		skyRenderer_->Generate(cmdList, heap, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::SkyGenerate);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::Composite);
		modelRenderer_->Compose(cmdList, gameFrameBuffer_.get(), heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::Composite);

		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::Transparent);
		geometryBuffer_.BeginDepth(cmdList);
		modelRenderer_->DrawTransparent(cmdList, gameFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::Transparent);

		/// [JP] 雨/雪パーティクル(EditorFlushと同じ作法、DrawTransparent が
		///      自分で EndDepth 済みなので改めて Begin/End する)。
		gpuProfiler_.Begin(cmdList, profileView, GpuProfileScope::WeatherParticle);
		geometryBuffer_.BeginDepth(cmdList);
		raytracingRenderer_->DrawWeatherParticles(cmdList, gameFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr, structuredAddr);
		geometryBuffer_.EndDepth(cmdList);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::WeatherParticle);

		effekseerManager_->Update(deltaTime);
		effekseerRenderer_->Draw(cmdList, *effekseerManager_);
	}

	void Renderer::CanvasFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = indicesSystem_->CanvasConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = indicesSystem_->StructuredAddress();

		indicesSystem_->SetCanvasSceneIndex(sceneSystem->GetIndex());
		indicesSystem_->UploadCanvas();

		imageRenderer_->Upload();
		imageRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		fontRenderer_->Upload();
		fontRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		movieRenderer_->Upload();
		movieRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawFullscreen(cmdList->Get(), heap, constantAddr, structuredAddr);

		selectionMaskFrameBuffer_->Begin(cmdList);
		selectionMaskFrameBuffer_->Clear(cmdList, 0.0f, 0.0f, 0.0f, 0.0f);
		imageRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		fontRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		selectionMaskFrameBuffer_->End(cmdList);

		outlineRenderer_->Draw(cmdList, canvasFrameBuffer_->RenderTargetViewHandle(), canvasFrameBuffer_->GetViewport(), heap, constantAddr, structuredAddr);
	}

	void Renderer::TimelineFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();

		timelineRenderer_->Upload();
		timelineRenderer_->Draw(cmdList, heap, scene);
	}

	void Renderer::ModelTransformFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();

		modelTransformRenderer_->Upload();
		modelTransformRenderer_->Draw(cmdList, heap, scene);
	}

	FrameBuffer* Renderer::GetEditorFrameBuffer()const
	{
		return editorFrameBuffer_.get();
	}

	FrameBuffer* Renderer::GetGameFrameBuffer()const
	{
		return gameFrameBuffer_.get();
	}

	FrameBuffer* Renderer::GetCanvasFrameBuffer()const
	{
		return canvasFrameBuffer_.get();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::EditorFrameBufferGPUHandle()const
	{
		return bindlessHeap_->GPUHandle(editorFrameBuffer_->ColorShaderResourceViewIndex());
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::GameFrameBufferGPUHandle()const
	{
		return bindlessHeap_->GPUHandle(gameFrameBuffer_->ColorShaderResourceViewIndex());
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::CanvasFrameBufferGPUHandle()const
	{
		return bindlessHeap_->GPUHandle(canvasFrameBuffer_->ColorShaderResourceViewIndex());
	}

	void Renderer::RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap)
	{
		imguiHeap_ = imguiHeap;

		auto createShaderResourceView = [&](ID3D12Resource* resource) -> Uint32
		{
			Uint32 index = imguiHeap->AllocateIndex();
			D3D12_RESOURCE_DESC desc = resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription{};
			shaderResourceViewDescription.Format = desc.Format;
			shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDescription.Texture2D.MipLevels = 1;

			device->CreateShaderResourceView(resource, &shaderResourceViewDescription, imguiHeap->CPUHandle(index));
			return index;
		};

		/// [EN] Editor/Game show PostProcessRenderer's tone-mapped, sRGB-encoded
		///      UNORM output, not the raw linear HDR FrameBuffer - see
		///      PostProcessRenderer's class doc comment and ToneMappingCS.hlsl
		///      for why the raw buffer was never correct to display directly.
		///      Canvas (pure 2D UI, no lighting) is untouched.
		/// [JP] Editor/Game は PostProcessRenderer のトーンマップ済み・sRGB
		///      エンコード済み UNORM 出力を表示する — 生のリニア HDR
		///      FrameBuffer ではない(そのまま表示するのが元々正しくなかった
		///      理由は PostProcessRenderer のクラスコメントと
		///      ToneMappingCS.hlsl 参照)。Canvas(純粋な2D UI、ライティング
		///      無し)は変更しない。
		editorImGuiShaderResourceViewIndex_ = createShaderResourceView(postProcessRenderer_->OutputResource(RaytracingView::Editor));
		gameImGuiShaderResourceViewIndex_ = createShaderResourceView(postProcessRenderer_->OutputResource(RaytracingView::Game));
		canvasImGuiShaderResourceViewIndex_ = createShaderResourceView(canvasFrameBuffer_->ColorResource());

		timelineRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
		modelTransformRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
	}

	void Renderer::RefreshImGuiOutputView(RaytracingView view)
	{
		if (!imguiHeap_)
		{
			return;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription{};
		shaderResourceViewDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDescription.Texture2D.MipLevels = 1;

		Uint32 index = view == RaytracingView::Editor ? editorImGuiShaderResourceViewIndex_ : gameImGuiShaderResourceViewIndex_;
		device_->CreateShaderResourceView(postProcessRenderer_->OutputResource(view), &shaderResourceViewDescription, imguiHeap_->CPUHandle(index));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::EditorImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(editorImGuiShaderResourceViewIndex_);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::GameImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(gameImGuiShaderResourceViewIndex_);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::CanvasImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(canvasImGuiShaderResourceViewIndex_);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::TimelineImGuiGPUHandle()const
	{
		return timelineRenderer_->ImGuiGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::ModelTransformImGuiGPUHandle()const
	{
		return modelTransformRenderer_->ImGuiGPUHandle();
	}

	EffekseerManager* Renderer::GetEffekseerManager()const
	{
		return effekseerManager_.get();
	}
}
