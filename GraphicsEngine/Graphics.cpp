#include <GraphicsEngine/Graphics.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/Scene.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Context/D3D12DebugLayer.h>
#include <FoundationEngine/Log/AftermathCrashTracker.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <FoundationEngine/Time/WorldTimer.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/Font/FontResource.h>
#include <GraphicsEngine/Movie/MovieResource.h>
#include <FoundationEngine/Time/GameTimer.h>

namespace SeedCore
{
	Bool Graphics::Initialize(HWND hwnd, Float width, Float height)
	{
		width_ = width;
		height_ = height;

		dlssManager_ = MakePtr<DlssManager>();
		if (!dlssManager_->Initialize())
		{
			SC_LOG_ERROR("DLSSの初期化に失敗しました");
			return false;
		}

#ifdef _DEBUG
		D3D12DebugLayer::Enable();
#endif

		/// [JP] デバイス作成前に呼ぶ必要がある(Enable() 呼び出し後に作成された
		///      デバイスのクラッシュしか Aftermath から見えないため)。
		AftermathCrashTracker::Enable();

		context_ = MakePtr<D3D12Context>();
		if (!context_->Initialize())
		{
			SC_LOG_ERROR("D3D12コンテキストの初期化に失敗しました");
			return false;
		}
		SC_LOG_NOTICE("D3D12コンテキストを初期化しました");

		AftermathCrashTracker::Create(context_->GetDevice());

		swapChain_ = MakePtr<SwapChain>(width, height);
		if (!swapChain_->Create(context_->GetFactory(), context_->GetDevice(), context_->GetDirectQueue()->GetCommandQueue(), hwnd))
		{
			SC_LOG_ERROR("スワップチェインの作成に失敗しました");
			return false;
		}
		SC_LOG_NOTICE("スワップチェインを作成しました ({}x{})", width, height);

		shaderCache_ = MakePtr<ShaderCache>();

		bc7CompressShader_ = MakePtr<BC7CompressShader>();
		bc7CompressShader_->Create(*shaderCache_, context_->GetDevice());

		bindlessHeap_ = MakePtr<BindlessHeap>();
		if (!bindlessHeap_->Create(context_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32768))
		{
			SC_LOG_ERROR("バインドレスヒープの作成に失敗しました");
			return false;
		}
		SC_LOG_NOTICE("バインドレスヒープを作成しました (32768 descriptors)");

		if (!dlssManager_->Prepare(context_->GetDevice()))
		{
			SC_LOG_WARNING("DLSSの準備に失敗しました - DLSS無効で続行します");
			return false;
		}

		renderer_ = MakePtr<Renderer>();
		renderer_->Create(context_->GetDevice(), context_->GetDirectQueue()->GetCommandQueue(), static_cast<Uint32>(swapChain_->BufferCount()), bindlessHeap_.get(), *shaderCache_, nativeWidth_, nativeHeight_);
		SC_LOG_NOTICE("レンダラーを作成しました ({}x{})", nativeWidth_, nativeHeight_);

		/// [JP] Renderer は DlssManager の Streamline SDK ライフサイクル(この
		///      関数の先頭で Initialize/Prepare 済み)を所有しない — DLSS-RR
		///      有効時に Tag()/EvaluateRayReconstruction() を駆動するための
		///      ポインタだけ渡す。
		renderer_->SetDlssManager(dlssManager_.get());

		editorSceneSystem_ = MakePtr<SceneSystem>(context_->GetDevice(), bindlessHeap_.get());
		gameSceneSystem_ = MakePtr<SceneSystem>(context_->GetDevice(), bindlessHeap_.get());
		canvasSceneSystem_ = MakePtr<SceneSystem>(context_->GetDevice(), bindlessHeap_.get());

		splashScreen_.Initialize(context_->GetDevice(), context_->GetDirectQueue(), bindlessHeap_.get());

		fadeScreen_.Initialize(context_->GetDevice());

		SC_LOG_NOTICE("グラフィックスエンジンの初期化が完了しました");
		return true;
	}

	void Graphics::WaitForGpuIdle()
	{
		context_->GetDirectQueue()->Signal();
		context_->GetDirectQueue()->Wait();

		context_->GetCopyQueue()->Signal();
		context_->GetCopyQueue()->Wait();

		context_->GetComputeQueue()->Signal();
		context_->GetComputeQueue()->Wait();
	}

	void Graphics::Resize(Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight, DescriptorHeap* imguiHeap)
	{
		WaitForGpuIdle();

		nativeWidth_ = nativeWidth;
		nativeHeight_ = nativeHeight;
		outputWidth_ = outputWidth;
		outputHeight_ = outputHeight;

		renderer_->Resize(context_->GetDevice(), bindlessHeap_.get(), *shaderCache_, nativeWidth_, nativeHeight_, outputWidth_, outputHeight_);

		if (imguiHeap)
		{
			RegisterImGuiShaderResourceViews(context_->GetDevice(), imguiHeap);
		}
	}

	void Graphics::Finalize()
	{
		WaitForGpuIdle();

		if (renderer_)
		{
			renderer_.reset();
			renderer_ = nullptr;
		}

		RootSignature::ClearCache();

		if (editorSceneSystem_)
		{
			editorSceneSystem_.reset();
			editorSceneSystem_ = nullptr;
		}

		if (gameSceneSystem_)
		{
			gameSceneSystem_.reset();
			gameSceneSystem_ = nullptr;
		}

		if (canvasSceneSystem_)
		{
			canvasSceneSystem_.reset();
			canvasSceneSystem_ = nullptr;
		}

		if (dlssManager_)
		{
			dlssManager_->Finalize();
			dlssManager_.reset();
			dlssManager_ = nullptr;
		}

		if (swapChain_)
		{
			swapChain_.reset();
			swapChain_ = nullptr;
		}

		if (bindlessHeap_)
		{
			bindlessHeap_.reset();
			bindlessHeap_ = nullptr;
		}

		if (bc7CompressShader_)
		{
			bc7CompressShader_.reset();
			bc7CompressShader_ = nullptr;
		}

		if (shaderCache_)
		{
			shaderCache_->Clear();
			shaderCache_.reset();
			shaderCache_ = nullptr;
		}

		fadeScreen_.Finalize();

		AftermathCrashTracker::Disable();

		if (context_)
		{
			context_.reset();
			context_ = nullptr;
		}

#ifdef _DEBUG
		D3D12DebugLayer::Report();
#endif
	}

	void Graphics::EditorRender(WorldTimer& timer, const EditorCamera& editorCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, ViewMode viewMode, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity)
	{
		SceneConstantBuffer editorSceneConstantBuffer{};
		editorSceneConstantBuffer.view_ = editorCamera.View();
		editorSceneConstantBuffer.inverseView_ = editorCamera.InverseView();
		editorSceneConstantBuffer.projection_ = editorCamera.Projection();
		editorSceneConstantBuffer.inverseProjection_ = editorCamera.InverseProjection();
		editorSceneConstantBuffer.nonJitterProjection_ = editorCamera.NonJitterProjection();
		editorSceneConstantBuffer.currentViewProjection_ = editorCamera.CurrentViewProjection();
		editorSceneConstantBuffer.previousViewProjection_ = editorCamera.PreviousViewProjection();
		editorSceneConstantBuffer.inverseViewProjection_ = editorCamera.InverseViewProjection();
		editorSceneConstantBuffer.nonJitterViewProjection_ = editorCamera.NonJitterViewProjection();
		editorSceneConstantBuffer.previousNonJitterViewProjection_ = editorCamera.PreviousNonJitterViewProjection();
		editorSceneConstantBuffer.cameraPosition_ = Vector4(editorCamera.Eye().x,editorCamera.Eye().y,editorCamera.Eye().z,1.0f);
		editorSceneConstantBuffer.cameraFocus_ = Vector4(editorCamera.Focus().x,editorCamera.Focus().y,editorCamera.Focus().z,1.0f);
		editorSceneConstantBuffer.fieldOfView_ = editorCamera.Fov();
		editorSceneConstantBuffer.nearPlane_ = editorCamera.Near();
		editorSceneConstantBuffer.farPlane_ = editorCamera.Far();
		editorSceneConstantBuffer.totalTime_ = timer.TotalTime();
		editorSceneConstantBuffer.deltaTime_ = timer.DeltaTime();
		editorSceneConstantBuffer.screenSize_ = Vector2(static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));
		editorSceneConstantBuffer.inverseScreenSize_ = Vector2(1.0f / nativeWidth_, 1.0f / nativeHeight_);
		editorSceneConstantBuffer.displaySize_ = renderer_->PostProcessOutputSize();
		editorSceneSystem_->Upload(editorSceneConstantBuffer);

		resourceCache.GetFontResource()->Update(context_->GetDevice(), context_->GetDirectQueue()->GetCommandQueue(), bindlessHeap_.get());

		movieSystem_.Update(world, resourceCache);
		resourceCache.GetMovieResource()->Update(context_->GetDevice(), context_->GetDirectList()->Get(), bindlessHeap_.get());

		/// [JP] ストリーミングの LOD 要求判定用に前フレームのカメラを渡す
		///      （カメラ更新は Gather の後 — 1 フレーム遅れで十分）。
		renderer_->Gather(loaderSystem, resourceCache, world, cameraSystem_.GetSceneConstantBuffer(), colliderInstances, selectedEntity);

		renderer_->BeginEditorFrame(context_->GetDirectList());
		renderer_->EditorFlush(context_->GetDirectList(), editorSceneSystem_.get(), timer.DeltaTime(), viewMode);
		renderer_->EndEditorFrame(context_->GetDirectList(), editorSceneConstantBuffer);
	}

	void Graphics::SetRaytracingSettings(const RaytracingContext& settings)
	{
		renderer_->SetRaytracingSettings(settings);
	}

	void Graphics::GameRender(GameTimer& timer, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world)
	{
		cameraSystem_.Update(world, timer, static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));

		SceneConstantBuffer gameSceneConstantBuffer = cameraSystem_.GetSceneConstantBuffer();
		gameSceneConstantBuffer.displaySize_ = renderer_->PostProcessOutputSize();

		renderer_->BeginGameFrame(context_->GetDirectList());

		Bool hasActiveCamera = cameraSystem_.HasActiveCamera();
		if (hasActiveCamera)
		{
			gameSceneSystem_->Upload(gameSceneConstantBuffer);
		}

		renderer_->GameFlush(context_->GetDirectList(), gameSceneSystem_.get(), timer.DeltaTime(), hasActiveCamera);

		fadeScreen_.Draw(context_->GetDirectList()->Get(), Scene::GetFadeAlpha(), static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));

		/// [JP] アクティブカメラが無いフレームでも EndGameFrame は必ず呼ぶ
		///      (PostProcess/DLSS-RRの経路を毎フレーム一貫させるため)。その
		///      場合 GetSceneConstantBuffer() は前回有効だった値(または既定値)
		///      を返す — DLSS-RR はこのフレームだけ多少不正確なリプロジェクション
		///      になり得るが、そもそも表示するゲーム画面が無い状況なので実害は
		///      無い。
		renderer_->EndGameFrame(context_->GetDirectList(), gameSceneConstantBuffer);
	}

	void Graphics::CanvasRender(WorldTimer& timer, const CanvasCamera& canvasCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world)
	{
		SceneConstantBuffer canvasSceneConstantBuffer{};
		canvasSceneConstantBuffer.view_ = canvasCamera.View();
		canvasSceneConstantBuffer.inverseView_ = canvasCamera.InverseView();
		canvasSceneConstantBuffer.projection_ = canvasCamera.Projection();
		canvasSceneConstantBuffer.inverseProjection_ = canvasCamera.InverseProjection();
		canvasSceneConstantBuffer.nonJitterProjection_ = canvasCamera.NonJitterProjection();
		canvasSceneConstantBuffer.currentViewProjection_ = canvasCamera.CurrentViewProjection();
		canvasSceneConstantBuffer.previousViewProjection_ = canvasCamera.PreviousViewProjection();
		canvasSceneConstantBuffer.inverseViewProjection_ = canvasCamera.InverseViewProjection();
		canvasSceneConstantBuffer.nonJitterViewProjection_ = canvasCamera.NonJitterViewProjection();
		canvasSceneConstantBuffer.cameraPosition_ = Vector4(canvasCamera.Eye().x, canvasCamera.Eye().y, canvasCamera.Eye().z, 1.0f);
		canvasSceneConstantBuffer.cameraFocus_ = Vector4(canvasCamera.Focus().x, canvasCamera.Focus().y, canvasCamera.Focus().z, 1.0f);
		canvasSceneConstantBuffer.fieldOfView_ = canvasCamera.Fov();
		canvasSceneConstantBuffer.nearPlane_ = canvasCamera.Near();
		canvasSceneConstantBuffer.farPlane_ = canvasCamera.Far();
		canvasSceneConstantBuffer.totalTime_ = timer.TotalTime();
		canvasSceneConstantBuffer.deltaTime_ = timer.DeltaTime();
		canvasSceneConstantBuffer.screenSize_ = Vector2(static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));
		canvasSceneConstantBuffer.inverseScreenSize_ = Vector2(1.0f / nativeWidth_, 1.0f / nativeHeight_);
		canvasSceneConstantBuffer.displaySize_ = canvasSceneConstantBuffer.screenSize_;
		canvasSceneSystem_->Upload(canvasSceneConstantBuffer);

		renderer_->BeginCanvasFrame(context_->GetDirectList());
		renderer_->CanvasFlush(context_->GetDirectList(), canvasSceneSystem_.get());
		renderer_->EndCanvasFrame(context_->GetDirectList());
	}

	void Graphics::TimelineRender(WorldTimer& timer, const PreviewCamera& timelineCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
	{
		renderer_->GatherTimelinePreview(loaderSystem, resourceCache, meshAssetId, animationAssetId, time, worldMatrix);

		SceneConstantBuffer previewSceneConstantBuffer{};
		previewSceneConstantBuffer.view_ = timelineCamera.View();
		previewSceneConstantBuffer.inverseView_ = timelineCamera.InverseView();
		previewSceneConstantBuffer.projection_ = timelineCamera.Projection();
		previewSceneConstantBuffer.inverseProjection_ = timelineCamera.InverseProjection();
		previewSceneConstantBuffer.nonJitterProjection_ = timelineCamera.NonJitterProjection();
		previewSceneConstantBuffer.currentViewProjection_ = timelineCamera.CurrentViewProjection();
		previewSceneConstantBuffer.previousViewProjection_ = timelineCamera.PreviousViewProjection();
		previewSceneConstantBuffer.inverseViewProjection_ = timelineCamera.InverseViewProjection();
		previewSceneConstantBuffer.nonJitterViewProjection_ = timelineCamera.NonJitterViewProjection();
		previewSceneConstantBuffer.cameraPosition_ = Vector4(timelineCamera.Eye().x, timelineCamera.Eye().y, timelineCamera.Eye().z, 1.0f);
		previewSceneConstantBuffer.cameraFocus_ = Vector4(timelineCamera.Focus().x, timelineCamera.Focus().y, timelineCamera.Focus().z, 1.0f);
		previewSceneConstantBuffer.fieldOfView_ = timelineCamera.Fov();
		previewSceneConstantBuffer.nearPlane_ = timelineCamera.Near();
		previewSceneConstantBuffer.farPlane_ = timelineCamera.Far();
		previewSceneConstantBuffer.totalTime_ = timer.TotalTime();
		previewSceneConstantBuffer.deltaTime_ = timer.DeltaTime();
		previewSceneConstantBuffer.screenSize_ = Vector2(static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));
		previewSceneConstantBuffer.inverseScreenSize_ = Vector2(1.0f / nativeWidth_, 1.0f / nativeHeight_);
		previewSceneConstantBuffer.displaySize_ = previewSceneConstantBuffer.screenSize_;

		renderer_->BeginTimelineFrame(context_->GetDirectList());
		renderer_->TimelineFlush(context_->GetDirectList(), previewSceneConstantBuffer);
		renderer_->EndTimelineFrame(context_->GetDirectList());
	}

	void Graphics::ModelTransformRender(WorldTimer& timer, const PreviewCamera& modelTransformCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
	{
		renderer_->GatherModelTransformPreview(loaderSystem, resourceCache, meshAssetId, animationAssetId, time, worldMatrix);

		SceneConstantBuffer previewSceneConstantBuffer{};
		previewSceneConstantBuffer.view_ = modelTransformCamera.View();
		previewSceneConstantBuffer.inverseView_ = modelTransformCamera.InverseView();
		previewSceneConstantBuffer.projection_ = modelTransformCamera.Projection();
		previewSceneConstantBuffer.inverseProjection_ = modelTransformCamera.InverseProjection();
		previewSceneConstantBuffer.nonJitterProjection_ = modelTransformCamera.NonJitterProjection();
		previewSceneConstantBuffer.currentViewProjection_ = modelTransformCamera.CurrentViewProjection();
		previewSceneConstantBuffer.previousViewProjection_ = modelTransformCamera.PreviousViewProjection();
		previewSceneConstantBuffer.inverseViewProjection_ = modelTransformCamera.InverseViewProjection();
		previewSceneConstantBuffer.nonJitterViewProjection_ = modelTransformCamera.NonJitterViewProjection();
		previewSceneConstantBuffer.cameraPosition_ = Vector4(modelTransformCamera.Eye().x, modelTransformCamera.Eye().y, modelTransformCamera.Eye().z, 1.0f);
		previewSceneConstantBuffer.cameraFocus_ = Vector4(modelTransformCamera.Focus().x, modelTransformCamera.Focus().y, modelTransformCamera.Focus().z, 1.0f);
		previewSceneConstantBuffer.fieldOfView_ = modelTransformCamera.Fov();
		previewSceneConstantBuffer.nearPlane_ = modelTransformCamera.Near();
		previewSceneConstantBuffer.farPlane_ = modelTransformCamera.Far();
		previewSceneConstantBuffer.totalTime_ = timer.TotalTime();
		previewSceneConstantBuffer.deltaTime_ = timer.DeltaTime();
		previewSceneConstantBuffer.screenSize_ = Vector2(static_cast<Float>(nativeWidth_), static_cast<Float>(nativeHeight_));
		previewSceneConstantBuffer.inverseScreenSize_ = Vector2(1.0f / nativeWidth_, 1.0f / nativeHeight_);
		previewSceneConstantBuffer.displaySize_ = previewSceneConstantBuffer.screenSize_;

		renderer_->BeginModelTransformFrame(context_->GetDirectList());
		renderer_->ModelTransformFlush(context_->GetDirectList(), previewSceneConstantBuffer);
		renderer_->EndModelTransformFrame(context_->GetDirectList());
	}

	void Graphics::Begin()
	{
		context_->BeginFrame();

		/// [JP] Streamlineのフレームトークンはフレーム単位(ビュー非依存)。
		///      Editor/Game 両方の DLSS-RR Tag()/Evaluate() より前に、
		///      このPresentフレームで1回だけ取得する(DlssManager::BeginFrame
		///      参照 — Tag() 呼び出しごとに取得すると後勝ちで上書きされ、
		///      先に処理したビューが誤ったトークンを使う事故になる)。
		dlssManager_->BeginFrame();

		auto cmdList = context_->GetDirectList();
		auto backBuffer = swapChain_->BackBuffer();

		cmdList->Barrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Graphics::End()
	{
		auto cmdList = context_->GetDirectList();
		auto backBuffer = swapChain_->BackBuffer();
		
		cmdList->Barrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		context_->EndFrame();

		/// [EN] Advance the deferred-reclaim ring once this frame's command list
		///      has been submitted, so descriptors and resources retired during
		///      the frame are only reused after every frame that referenced them
		///      has completed on the GPU.
		/// [JP] このフレームのコマンドリストを提出し終えてから遅延回収リングを
		///      進める。フレーム中に退役したディスクリプタとリソースは、それらを
		///      参照した全フレームが GPU 上で完了してから初めて再利用される。
		bindlessHeap_->Retire();
	}

	void Graphics::Clear()
	{
		auto cmdList = context_->GetDirectList();
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = swapChain_->Handle();
		cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);
		
		const Float clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
		cmdList->Get()->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);
	}

	D3D12Context* Graphics::GetContext()const
	{
		return context_.get();
	}

	SwapChain* Graphics::GetSwapChain()const
	{
		return swapChain_.get();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::GetEditorGPUHandle()const
	{
		return renderer_->EditorFrameBufferGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::GetGameGPUHandle()const
	{
		return renderer_->GameFrameBufferGPUHandle();
	}

	BindlessHeap* Graphics::GetBindlessHeap()const
	{
		return bindlessHeap_.get();
	}

	ShaderCache& Graphics::GetShaderCache()const
	{
		return *shaderCache_;
	}

	BC7CompressShader& Graphics::GetBC7CompressShader()const
	{
		return *bc7CompressShader_;
	}

	void Graphics::RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap)
	{
		renderer_->RegisterImGuiShaderResourceViews(device, imguiHeap);
	}

	const GpuProfiler& Graphics::GetGpuProfiler()const
	{
		return renderer_->GetGpuProfiler();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::EditorImGuiGPUHandle()const
	{
		return renderer_->EditorImGuiGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::GameImGuiGPUHandle()const
	{
		return renderer_->GameImGuiGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::CanvasImGuiGPUHandle()const
	{
		return renderer_->CanvasImGuiGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::TimelineImGuiGPUHandle()const
	{
		return renderer_->TimelineImGuiGPUHandle();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Graphics::ModelTransformImGuiGPUHandle()const
	{
		return renderer_->ModelTransformImGuiGPUHandle();
	}

	CameraSystem& Graphics::GetCameraSystem()
	{
		return cameraSystem_;
	}

	EffekseerManager* Graphics::GetEffekseerManager()const
	{
		return renderer_->GetEffekseerManager();
	}

	void Graphics::DrawSplashScreen(Bool loadComplete, Float progress, Bool showWarning, Bool showFiction)
	{
		if (splashScreen_.IsFinished())
		{
			return;
		}

		auto cmdList = context_->GetDirectList()->Get();
		auto rtvHandle = swapChain_->Handle();
		splashScreen_.Draw(cmdList, swapChain_->BackBuffer(), rtvHandle, width_, height_, loadComplete, progress, showWarning, showFiction);
	}

	Bool Graphics::IsSplashFinished()const
	{
		return splashScreen_.IsFinished();
	}

	void Graphics::SetImGuiContext(ImGuiContext* context)
	{
		ImGui::SetCurrentContext(context);
	}
}