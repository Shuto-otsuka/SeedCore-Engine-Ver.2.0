#include <GraphicsEngine/Renderer/Renderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <GraphicsEngine/Movie/MovieResource.h>
#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/Model/Material/MaterialResource.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	Renderer::Renderer()
	{
		effekseerManager_ = MakePtr<EffekseerManager>();

		modelRenderer_ = MakePtr<ModelRenderer>(rootSignature_, pipelineStateObject_);
		imageRenderer_ = MakePtr<ImageRenderer>(rootSignature_, pipelineStateObject_);
		fontRenderer_ = MakePtr<FontRenderer>(rootSignature_, pipelineStateObject_);
		movieRenderer_ = MakePtr<MovieRenderer>(rootSignature_, pipelineStateObject_);
		outlineRenderer_ = MakePtr<OutlineRenderer>(rootSignature_, pipelineStateObject_);
		hudComposeRenderer_ = MakePtr<HUDComposeRenderer>(rootSignature_, pipelineStateObject_);
		colliderRenderer_ = MakePtr<ColliderRenderer>();
		raytracingRenderer_ = MakePtr<RaytracingRenderer>(rootSignature_, pipelineStateObject_, raytracingStateObject_);
		skyRenderer_ = MakePtr<SkyRenderer>();
		timelineRenderer_ = MakePtr<TimelineRenderer>(rootSignature_, pipelineStateObject_);
		modelTransformRenderer_ = MakePtr<ModelTransformRenderer>(rootSignature_, pipelineStateObject_);
		materialRenderer_ = MakePtr<MaterialRenderer>(rootSignature_, pipelineStateObject_);
		effekseerRenderer_ = MakePtr<EffekseerRenderer>();
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
		Gateway::BindEffekseerManager(effekseerManager_.get());

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
		uiColorAlphaRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);

		editorFrameBuffer_ = MakePtr<FrameBuffer>(device, &editorRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &editorDepthStencilViewHeap_);
		gameFrameBuffer_ = MakePtr<FrameBuffer>(device, &gameRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &gameDepthStencilViewHeap_);
		canvasFrameBuffer_ = MakePtr<FrameBuffer>(device, &canvasRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &canvasDepthStencilViewHeap_);

		uiColorAlphaFrameBuffer_ = MakePtr<FrameBuffer>(device, &uiColorAlphaRenderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, nullptr, 0.0f, 0.0f, 0.0f, 0.0f);
		indicesSystem_->SetUIColorAlphaIndex(uiColorAlphaFrameBuffer_->ColorShaderResourceViewIndex());

		hudlessBuffer_.Create(device, width, height);

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
		hudComposeRenderer_->Create(device, bindlessHeap, shaderCache);
		colliderRenderer_->Create(device, bindlessHeap, shaderCache);
		raytracingRenderer_->Create(device, bindlessHeap, shaderCache, *indicesSystem_, width, height);
		skyRenderer_->Create(device, bindlessHeap, shaderCache, rootSignature_, pipelineStateObject_);
		timelineRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		modelTransformRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
		materialRenderer_->Create(device, bindlessHeap, shaderCache, width, height);
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
		uiColorAlphaRenderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);

		editorFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		gameFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		canvasFrameBuffer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		uiColorAlphaFrameBuffer_->Resize(device, bindlessHeap, outputWidth, outputHeight);
		indicesSystem_->SetUIColorAlphaIndex(uiColorAlphaFrameBuffer_->ColorShaderResourceViewIndex());

		hudlessBuffer_.Resize(device, outputWidth, outputHeight);

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
		materialRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight);
		postProcessRenderer_->Resize(device, bindlessHeap, nativeWidth, nativeHeight, outputWidth, outputHeight);
		dlssRayReconstructionRenderer_->Resize(device, bindlessHeap, *indicesSystem_, nativeWidth, nativeHeight, outputWidth, outputHeight);
		taauUpsamplingRenderer_->Resize(device, bindlessHeap, outputWidth, outputHeight);
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

		if (Gateway::GetDlssManager().RayReconstructionEnable())
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Editor, GpuProfileScope::DlssRayReconstruction);
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Editor, &Gateway::GetDlssManager(), scene, editorFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(2), nativeWidth_, nativeHeight_, upscaleMode_);
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

		if (Gateway::GetDlssManager().DeepDVCEnable())
		{
			ID3D12Resource* deepDVCTarget = postProcessRenderer_->OutputResource(RaytracingView::Editor);
			Vector2 deepDVCOutputSize = postProcessRenderer_->OutputSize();
			cmdList->Barrier(deepDVCTarget, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Gateway::GetDlssManager().EvaluateDeepDVC(cmdList->Get(), deepDVCTarget, 0, static_cast<Uint32>(deepDVCOutputSize.x), static_cast<Uint32>(deepDVCOutputSize.y));
			cmdList->Barrier(deepDVCTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		postProcessRenderer_->BeginDebugOverlay(cmdList, RaytracingView::Editor);

		D3D12_CPU_DESCRIPTOR_HANDLE debugRenderTargetView = postProcessRenderer_->OutputRenderTargetViewHandle(RaytracingView::Editor);
		D3D12_VIEWPORT debugViewport = postProcessRenderer_->Viewport(RaytracingView::Editor);

		debugDepthResizeBuffer_.Dispatch(cmdList, bindlessHeap_->Heap(), geometryBuffer_, nativeWidth_, nativeHeight_);
		colliderRenderer_->Draw(cmdList, debugRenderTargetView, debugDepthResizeBuffer_.DepthStencilViewHandle(), debugViewport, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress());
		geometryBuffer_.BeginDepth(cmdList);

		outlineRenderer_->Draw(cmdList, debugRenderTargetView, debugViewport, bindlessHeap_->Heap(), indicesSystem_->EditorConstantAddress(), indicesSystem_->StructuredAddress());

		postProcessRenderer_->EndDebugOverlay(cmdList, RaytracingView::Editor);

		RefreshImGui(RaytracingView::Editor);
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

		if (Gateway::GetDlssManager().RayReconstructionEnable())
		{
			gpuProfiler_.Begin(cmdList, GpuProfileView::Game, GpuProfileScope::DlssRayReconstruction);
			dlssRayReconstructionRenderer_->Dispatch(cmdList, bindlessHeap_->Heap(), indicesSystem_->GameConstantAddress(), indicesSystem_->StructuredAddress(), RaytracingView::Game, &Gateway::GetDlssManager(), scene, gameFrameBuffer_->ColorResource(), geometryBuffer_.DepthResource(), geometryBuffer_.ColorResource(2), nativeWidth_, nativeHeight_, upscaleMode_);
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

		if (Gateway::GetDlssManager().DeepDVCEnable())
		{
			ID3D12Resource* deepDVCTarget = postProcessRenderer_->OutputResource(RaytracingView::Game);
			Vector2 deepDVCOutputSize = postProcessRenderer_->OutputSize();
			cmdList->Barrier(deepDVCTarget, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Gateway::GetDlssManager().EvaluateDeepDVC(cmdList->Get(), deepDVCTarget, 1, static_cast<Uint32>(deepDVCOutputSize.x), static_cast<Uint32>(deepDVCOutputSize.y));
			cmdList->Barrier(deepDVCTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		postProcessRenderer_->BeginDebugOverlay(cmdList, RaytracingView::Game);
		postProcessRenderer_->CaptureHudless(cmdList, hudlessBuffer_, RaytracingView::Game);

		D3D12_CPU_DESCRIPTOR_HANDLE gameDisplayRenderTargetView = postProcessRenderer_->OutputRenderTargetViewHandle(RaytracingView::Game);
		D3D12_VIEWPORT gameDisplayViewport = postProcessRenderer_->Viewport(RaytracingView::Game);

		ID3D12DescriptorHeap* spriteHeap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS spriteConstantAddr = indicesSystem_->GameConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS spriteStructuredAddr = indicesSystem_->StructuredAddress();

		uiColorAlphaFrameBuffer_->Begin(cmdList);
		uiColorAlphaFrameBuffer_->Clear(cmdList, 0.0f, 0.0f, 0.0f, 0.0f);

		D3D12_VIEWPORT uiColorAlphaViewport = uiColorAlphaFrameBuffer_->GetViewport();
		cmdList->Get()->RSSetViewports(1, &uiColorAlphaViewport);
		D3D12_RECT uiColorAlphaScissorRect = { 0, 0, static_cast<LONG>(uiColorAlphaViewport.Width), static_cast<LONG>(uiColorAlphaViewport.Height) };
		cmdList->Get()->RSSetScissorRects(1, &uiColorAlphaScissorRect);

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

		uiColorAlphaFrameBuffer_->End(cmdList);

		if (Gateway::GetDlssManager().FrameGenerationEnable())
		{
			DlssBufferTag frameGenerationBufferTag{};
			frameGenerationBufferTag.depthBuffer_ = geometryBuffer_.DepthResource();
			frameGenerationBufferTag.velocityBuffer_ = geometryBuffer_.ColorResource(2);
			frameGenerationBufferTag.width_ = static_cast<Float>(nativeWidth_);
			frameGenerationBufferTag.height_ = static_cast<Float>(nativeHeight_);

			Gateway::GetDlssManager().FrameGenerationTag(frameGenerationBufferTag, cmdList->Get(), hudlessBuffer_.ColorResource(), uiColorAlphaFrameBuffer_->ColorResource(), 1, static_cast<Uint32>(gameDisplayViewport.Width), static_cast<Uint32>(gameDisplayViewport.Height));
		}

		hudComposeRenderer_->Draw(cmdList, gameDisplayRenderTargetView, gameDisplayViewport, spriteHeap, spriteConstantAddr, spriteStructuredAddr);

		postProcessRenderer_->EndDebugOverlay(cmdList, RaytracingView::Game);

		RefreshImGui(RaytracingView::Game);
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

	void Renderer::BeginMaterialFrame(D3D12CommandList* cmdList)
	{
		materialRenderer_->Begin(cmdList);
	}

	void Renderer::EndMaterialFrame(D3D12CommandList* cmdList)
	{
		materialRenderer_->End(cmdList);
	}

	void Renderer::GatherScenePreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, const SceneConstantBuffer& scene, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity)
	{
		colliderRenderer_->Clear();
		for (const ColliderInstance& instance : colliderInstances)
		{
			colliderRenderer_->AddInstance(static_cast<ColliderShapeKind>(instance.shapeKind_), instance.position_, instance.rotation_, instance.dimensions_, instance.color_);
		}

		ModelResource* modelResource = resourceCache.GetModelResource();
		MaterialResource* materialResource = resourceCache.GetMaterialResource();
		AnimationResource* animationResource = resourceCache.GetAnimationResource();

		constraintSystem_.Execute(world);
		animationSystem_.Execute(world, loaderSystem, *animationResource, *modelResource);
		constraintSystem_.Execute(world);

		modelRenderer_->Gather(loaderSystem, *modelResource, *materialResource, *animationResource, world, scene, selectedEntity);
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

	void Renderer::GatherMaterialPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 surfaceAssetId, const Matrix& worldMatrix)
	{
		ModelResource* modelResource = resourceCache.GetModelResource();
		MaterialResource* materialResource = resourceCache.GetMaterialResource();
		materialRenderer_->Gather(loaderSystem, *modelResource, *materialResource, meshAssetId, surfaceAssetId, worldMatrix);
	}

	void Renderer::Raytracing(const RaytracingContext& settings)
	{
		Gateway::GetDlssManager().RayReconstructionEnable(settings.dlssRayReconstructionEnabled_);
		raytracingRenderer_->SetRaytracingSettings(settings);
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
		if (Gateway::GetDlssManager().RayReconstructionEnable())
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

	void Renderer::GameFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime, Bool hasActiveCamera)
	{
		/// [JP] indexの更新だけは、カメラが無いフレームでも必ずやる - EndGameFrame
		///      はカメラの有無に関わらず毎フレームGameView用のTAAU/PostProcess
		///      をディスパッチし続けるため(そちらのコメント参照)、ここを
		///      hasActiveCamera で丸ごとスキップすると、後続のResize()で破棄
		///      済みとなったリソースのindexを読み続け、遅延回収リングが実際に
		///      解放した時点でフォルトする。
		indicesSystem_->SetGameSceneIndex(sceneSystem->GetIndex());
		indicesSystem_->SetLightIndex(lightSystem_->GetIndex());
		indicesSystem_->SetClusterConstantIndex(lightSystem_->GetClusterConstantIndex());
		skyRenderer_->SetIndices(*indicesSystem_);
		lightSystem_->Upload();
		modelRenderer_->Upload();
		Uint32 gamePostProcessSourceColorIndex;
		if (Gateway::GetDlssManager().RayReconstructionEnable())
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

		if (!hasActiveCamera)
		{
			return;
		}

		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();
		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = indicesSystem_->GameConstantAddress();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = indicesSystem_->StructuredAddress();

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

	void Renderer::MaterialFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene)
	{
		ID3D12DescriptorHeap* heap = bindlessHeap_->Heap();

		materialRenderer_->Upload();
		materialRenderer_->Draw(cmdList, heap, scene);
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
		/// [EN] Called again on every resize (see Graphics::Resize).
		///      DescriptorHeap is a bump allocator with no FreeIndex, so
		///      re-allocating a fresh index here every time would leak 3
		///      slots per resize until the heap is exhausted and
		///      AllocateIndex starts handing back garbage. Instead, reuse
		///      the same index across every call after the first (matching
		///      RefreshImGui's existing pattern) and only
		///      overwrite the descriptor at that slot.
		/// [JP] リサイズのたびに再度呼ばれる(Graphics::Resize参照)。
		///      DescriptorHeap は FreeIndex を持たない増加専用アロケータなので、
		///      毎回新しい Index を確保すると1リサイズにつき3枠ずつ漏れ続け、
		///      いずれ枯渇して AllocateIndex がおかしな値を返すようになる。
		///      代わりに初回以降は同じ Index を使い回し(RefreshImGui
		///      が既にやってるのと同じパターン)、そのスロットのディスクリプタ
		///      だけ上書きする。
		Bool alreadyRegistered = imguiHeap_ != nullptr;
		imguiHeap_ = imguiHeap;

		auto createShaderResourceView = [&](ID3D12Resource* resource, Uint32& index) -> Uint32
		{
			if (!alreadyRegistered)
			{
				index = imguiHeap->AllocateIndex();
			}

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
		createShaderResourceView(postProcessRenderer_->OutputResource(RaytracingView::Editor), editorImGuiShaderResourceViewIndex_);
		createShaderResourceView(postProcessRenderer_->OutputResource(RaytracingView::Game), gameImGuiShaderResourceViewIndex_);
		createShaderResourceView(canvasFrameBuffer_->ColorResource(), canvasImGuiShaderResourceViewIndex_);

		timelineRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
		modelTransformRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
		materialRenderer_->RegisterImGuiShaderResourceView(device, imguiHeap);
	}

	void Renderer::RefreshImGui(RaytracingView view)
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

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::MaterialImGuiGPUHandle()const
	{
		return materialRenderer_->ImGuiGPUHandle();
	}
}
