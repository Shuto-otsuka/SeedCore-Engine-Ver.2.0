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
		previewRenderer_ = MakePtr<PreviewRenderer>(rootSignature_, pipelineStateObject_);
		effekseerRenderer_ = MakePtr<EffekseerRenderer>();
		effekseerManager_ = MakePtr<EffekseerManager>();
		postProcessRenderer_ = MakePtr<PostProcessRenderer>(rootSignature_, pipelineStateObject_);
		dlssRayReconstructionRenderer_ = MakePtr<DlssRayReconstructionRenderer>(rootSignature_, pipelineStateObject_);
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

		indicesSystem_->SetGBuffer0Index(geometryBuffer_.ColorShaderResourceViewIndex(0));
		indicesSystem_->SetGBuffer1Index(geometryBuffer_.ColorShaderResourceViewIndex(1));
		indicesSystem_->SetGBuffer2Index(geometryBuffer_.ColorShaderResourceViewIndex(2));
		indicesSystem_->SetGBuffer3Index(geometryBuffer_.ColorShaderResourceViewIndex(3));
		indicesSystem_->SetGBuffer4Index(geometryBuffer_.ColorShaderResourceViewIndex(4));
		indicesSystem_->SetGBufferDepthIndex(geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetGBufferVelocityUnorderedAccessViewIndex(geometryBuffer_.VelocityUnorderedAccessViewIndex());

		hiZBuffer_.Create(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, width, height, geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetHiZIndex(hiZBuffer_.ShaderResourceViewIndex());

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
		previewRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		postProcessRenderer_->Create(device, bindlessHeap, shaderCache, width, height, width, height);
		dlssRayReconstructionRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_, width, height, width, height);

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

		indicesSystem_->SetGBuffer0Index(geometryBuffer_.ColorShaderResourceViewIndex(0));
		indicesSystem_->SetGBuffer1Index(geometryBuffer_.ColorShaderResourceViewIndex(1));
		indicesSystem_->SetGBuffer2Index(geometryBuffer_.ColorShaderResourceViewIndex(2));
		indicesSystem_->SetGBuffer3Index(geometryBuffer_.ColorShaderResourceViewIndex(3));
		indicesSystem_->SetGBuffer4Index(geometryBuffer_.ColorShaderResourceViewIndex(4));
		indicesSystem_->SetGBufferDepthIndex(geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetGBufferVelocityUnorderedAccessViewIndex(geometryBuffer_.VelocityUnorderedAccessViewIndex());

		hiZBuffer_.Resize(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_, nativeWidth, nativeHeight, geometryBuffer_.DepthShaderResourceViewIndex());
		indicesSystem_->SetHiZIndex(hiZBuffer_.ShaderResourceViewIndex());

		selectionMaskRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		selectionMaskFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		indicesSystem_->SetSelectionMaskIndex(selectionMaskFrameBuffer_->ColorShaderResourceViewIndex());

		lightSystem_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());

		modelRenderer_->Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight);
		raytracingRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		previewRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		postProcessRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight, outputWidth, outputHeight);
		dlssRayReconstructionRenderer_->Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight, outputWidth, outputHeight);
	}

	void Renderer::SetDlssManager(DlssManager* dlssManager)
	{
		dlssManager_ = dlssManager;
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
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, dlssManager_, scene, editorFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(3), nativeWidth_, nativeHeight_, dlssMode_);
			gpuProfiler_.End(cmdList, GpuProfileView::Editor, GpuProfileScope::DlssRayReconstruction);

			postProcessSource = dlssRayReconstructionRenderer_->OutputResource(RaytracingView::Editor);
		}

		gpuProfiler_.Begin(cmdList, GpuProfileView::Editor, GpuProfileScope::PostProcess);
		postProcessRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, postProcessSource, dlssRayReconstructionEnabled_);
		gpuProfiler_.End(cmdList, GpuProfileView::Editor, GpuProfileScope::PostProcess);

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
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, dlssManager_, scene, gameFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(3), nativeWidth_, nativeHeight_, dlssMode_);
			gpuProfiler_.End(cmdList, GpuProfileView::Game, GpuProfileScope::DlssRayReconstruction);

			postProcessSource = dlssRayReconstructionRenderer_->OutputResource(RaytracingView::Game);
		}

		gpuProfiler_.Begin(cmdList, GpuProfileView::Game, GpuProfileScope::PostProcess);
		postProcessRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, postProcessSource, dlssRayReconstructionEnabled_);
		gpuProfiler_.End(cmdList, GpuProfileView::Game, GpuProfileScope::PostProcess);

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

	void Renderer::BeginPreviewFrame(D3D12CommandList* cmdList)
	{
		previewRenderer_->Begin(cmdList);
	}

	void Renderer::EndPreviewFrame(D3D12CommandList* cmdList)
	{
		previewRenderer_->End(cmdList);
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
		imageRenderer_->Gather(loaderSystem, *imageResource, world, selectedEntity);

		FontResource* fontResource = resourceCache.GetFontResource();
		fontRenderer_->Gather(*fontResource, world, selectedEntity);

		MovieResource* movieResource = resourceCache.GetMovieResource();
		movieRenderer_->Gather(*movieResource, world, selectedEntity);

		celestialResult_ = CelestialSystem::Compute(daySystem_, sunLight_, moonLight_);
		Bool sunOverride = daySystemEnabled_ && sunLightEnabled_;
		weatherState_ = WeatherSystem::ReadGpuState(world);
		lightSystem_->Gather(loaderSystem, *modelResource, world, sunOverride ? &celestialResult_ : nullptr, &weatherState_);

		lastCameraPosition_ = Vector3(scene.cameraPosition_.x, scene.cameraPosition_.y, scene.cameraPosition_.z);

		skyRenderer_->Gather(loaderSystem, resourceCache, world);

		postProcessRenderer_->Gather(world);

		effekseerRenderer_->SetCamera(scene);
	}

	void Renderer::GatherPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
	{
		ModelResource* modelResource = resourceCache.GetModelResource();
		AnimationResource* animationResource = resourceCache.GetAnimationResource();
		previewRenderer_->Gather(loaderSystem, *modelResource, *animationResource, meshAssetId, animationAssetId, time, worldMatrix);
	}

	void Renderer::SetRaytracingSettings(const RaytracingContext& settings)
	{
		raytracingRenderer_->SetRaytracingSettings(settings);
		dlssRayReconstructionEnabled_ = settings.dlssRayReconstructionEnabled_;
		dlssMode_ = settings.dlssMode_;

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

		postProcessRenderer_->PrepareView(*indicesSystem_, RaytracingView::Editor, dlssRayReconstructionEnabled_ ? dlssRayReconstructionRenderer_->OutputShaderResourceViewIndex(RaytracingView::Editor) : editorFrameBuffer_->ColorShaderResourceViewIndex(), dlssRayReconstructionEnabled_);

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
		geometryBuffer_.BeginColor(cmdList);
		geometryBuffer_.ClearColor(cmdList);
		modelRenderer_->DrawOpaque(cmdList, heap, constantAddr, structuredAddr);
		geometryBuffer_.End(cmdList);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::GeometryBuffer);

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

		if (skyRenderer_->RealtimeCaptureReady(deltaTime))
		{
			GpuProfileScopeGuard captureScope(gpuProfiler_, cmdList, profileView, GpuProfileScope::SkyRealtimeCapture);

			Uint captureFace = skyRenderer_->CurrentCaptureFace();
			skyRenderer_->BeginFaceCapture(cmdList, heap, captureFace, lightSystem_->GetIndex());
			modelRenderer_->DrawOpaqueForward(cmdList, heap, skyRenderer_->CaptureConstantAddress(), structuredAddr);
			skyRenderer_->EndFaceCapture(cmdList, captureFace);
			skyRenderer_->ConvolveRealtime(cmdList, heap);
			skyRenderer_->AdvanceCaptureFace();
		}

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

		/// [EN] Collider/constraint debug wireframe: reuses the same
		///      color+depth binding pattern as DrawWireframe/DrawMeshlet
		///      above (editor frame buffer color, geometry-buffer depth,
		///      read-only). ColliderRenderer::Draw is a no-op if this frame's
		///      Gather() was passed an empty colliderInstances list.
		/// [JP] コライダー/コンストレイントのデバッグワイヤーフレーム:
		///      上の DrawWireframe/DrawMeshlet と同じ色+深度バインド
		///      パターン（エディタフレームバッファの色、ジオメトリバッファの
		///      深度、読み取り専用）を再利用する。このフレームの Gather() に
		///      渡された colliderInstances が空であれば
		///      ColliderRenderer::Draw は何もしない。
		geometryBuffer_.BeginDepth(cmdList);
		colliderRenderer_->Draw(cmdList, editorFrameBuffer_.get(), &geometryBuffer_, heap, constantAddr);
		geometryBuffer_.EndDepth(cmdList);

		/// [JP] 選択アウトライン: EditorFlush が実際に描画するのは Model と Billboard
		///      （Image の Billboard と、Font の Billboard = 3D ワールドテキスト）。
		///      Sprite と Font の Sprite（2D UI テキスト）は Canvas/Game 専用で
		///      EditorFlush では描かれないので、マスクへ描き込むのもこの 2 種だけ。
		///      Sprite 系の選択アウトラインは CanvasFlush 側で同じマスクを
		///      使い回して行う。
		selectionMaskFrameBuffer_->Begin(cmdList);
		selectionMaskFrameBuffer_->Clear(cmdList, 0.0f, 0.0f, 0.0f, 0.0f);
		modelRenderer_->DrawSelectionMask(cmdList, heap, constantAddr, structuredAddr);
		imageRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		fontRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawSelectionMaskBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		selectionMaskFrameBuffer_->End(cmdList);

		outlineRenderer_->Draw(cmdList, editorFrameBuffer_.get(), heap, constantAddr, structuredAddr);

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
		postProcessRenderer_->PrepareView(*indicesSystem_, RaytracingView::Game, dlssRayReconstructionEnabled_ ? dlssRayReconstructionRenderer_->OutputShaderResourceViewIndex(RaytracingView::Game) : gameFrameBuffer_->ColorShaderResourceViewIndex(), dlssRayReconstructionEnabled_);
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
		geometryBuffer_.BeginColor(cmdList);
		geometryBuffer_.ClearColor(cmdList);
		modelRenderer_->DrawOpaque(cmdList, heap, constantAddr, structuredAddr);
		geometryBuffer_.End(cmdList);
		gpuProfiler_.End(cmdList, profileView, GpuProfileScope::GeometryBuffer);

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

		if (skyRenderer_->RealtimeCaptureReady(deltaTime))
		{
			GpuProfileScopeGuard captureScope(gpuProfiler_, cmdList, profileView, GpuProfileScope::SkyRealtimeCapture);

			Uint captureFace = skyRenderer_->CurrentCaptureFace();
			skyRenderer_->BeginFaceCapture(cmdList, heap, captureFace, lightSystem_->GetIndex());
			modelRenderer_->DrawOpaqueForward(cmdList, heap, skyRenderer_->CaptureConstantAddress(), structuredAddr);
			skyRenderer_->EndFaceCapture(cmdList, captureFace);
			skyRenderer_->ConvolveRealtime(cmdList, heap);
			skyRenderer_->AdvanceCaptureFace();
		}

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

		imageRenderer_->Upload();
		imageRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		imageRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		fontRenderer_->Upload();
		fontRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		fontRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);

		movieRenderer_->Upload();
		movieRenderer_->DrawBillboard(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawFullscreen(cmdList->Get(), heap, constantAddr, structuredAddr);
	}

	void Renderer::CanvasFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = indicesSystem_->CanvasConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = indicesSystem_->StructuredAddress();

		indicesSystem_->SetCanvasSceneIndex(sceneSystem->GetIndex());
		indicesSystem_->UploadCanvas();

		imageRenderer_->Upload();
		imageRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);

		/// [JP] Font の Sprite（2D UI テキスト）のみ。Billboard は EditorFlush 専用。
		fontRenderer_->Upload();
		fontRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);

		movieRenderer_->Upload();
		movieRenderer_->DrawSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawFullscreen(cmdList->Get(), heap, constantAddr, structuredAddr);

		/// [JP] 選択アウトライン: Sprite/Font-Sprite は CanvasFlush でのみ実際に
		///      描画されるので、ここで同じ共有マスクを使い回して合成する
		///      （EditorFlush 側の Model/Billboard 用マスク描画とは時間的に分離）。
		selectionMaskFrameBuffer_->Begin(cmdList);
		selectionMaskFrameBuffer_->Clear(cmdList, 0.0f, 0.0f, 0.0f, 0.0f);
		imageRenderer_->DrawSelectionMaskSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		fontRenderer_->DrawSelectionMaskSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		movieRenderer_->DrawSelectionMaskSprite(cmdList->Get(), heap, constantAddr, structuredAddr);
		selectionMaskFrameBuffer_->End(cmdList);

		outlineRenderer_->Draw(cmdList, canvasFrameBuffer_.get(), heap, constantAddr, structuredAddr);
	}

	void Renderer::PreviewFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();

		previewRenderer_->Upload();
		previewRenderer_->Draw(cmdList, heap, scene);
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

		previewRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
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

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::PreviewImGuiGPUHandle()const
	{
		return previewRenderer_->ImGuiGPUHandle();
	}

	EffekseerManager* Renderer::GetEffekseerManager()const
	{
		return effekseerManager_.get();
	}
}
