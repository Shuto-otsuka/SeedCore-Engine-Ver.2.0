#include <Editor/Editor/Engine.h>
#include <FoundationEngine/Utility/Bootstrap.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/ECS/LayerRegistry.h>

#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/D3D12/Context/D3D12Adapter.h>

#include <PhysicsEngine/Physics/PhysicsSystem.h>

namespace SeedCore
{
	void Engine::Boot(const Bootstrap& boot)
	{
		if (!BootWindow(boot))
		{
			return;
		}

		world_ = MakePtr<World>();

		system_ = MakePtr<SystemScheduler>();

		auto worker = MakeWorkerInterface<JobScheduler>();
		executor_ = MakePtr<JobExecutor>(std::thread::hardware_concurrency(), std::move(worker));

		joltManager_ = MakePtr<JoltManager>();
		if (!joltManager_->Initialize(*executor_))
		{
			return;
		}
		Gateway::BindJoltManager(joltManager_.get());

		graphics_ = MakePtr<Graphics>();
		if (!graphics_->Initialize(hwnd_, static_cast<Float>(boot.WindowDesc_.Width_), static_cast<Float>(boot.WindowDesc_.Height_)))
		{
			return;
		}

		ID3D12Device* device = graphics_->GetContext()->GetDevice();

		criManager_ = MakePtr<CriManager>();
		if (!criManager_->Initialize())
		{
			return;
		}
		Gateway::BindCriManager(criManager_.get());

		fontManager_ = MakePtr<FontManager>();
		if (!fontManager_->Initialize())
		{
			return;
		}
		Gateway::BindFontManager(fontManager_.get());

		imgui_ = MakePtr<ImGuiRenderer>();
		if (!imgui_->Initialize(hwnd_, device, static_cast<Int>(graphics_->GetSwapChain()->BufferCount())))
		{
			return;
		}

		graphics_->RegisterImGuiShaderResourceViews(device, imgui_->GetDescriptorHeap());

		loaderSystem_ = MakePtr<LoaderSystem>(device);
		resource_ = MakePtr<ResourceCache>(*loaderSystem_, device, graphics_->GetContext()->GetDirectQueue(), graphics_->GetBindlessHeap());
		resource_->Async();

		Scene::Initialize(*world_, *resource_, *executor_);
		Prefab::Initialize(*world_, *resource_);

		editorContext_.worldContext_.world_ = world_.get();
		editorContext_.worldContext_.resource_ = resource_.get();
		editorContext_.worldContext_.loader_ = loaderSystem_.get();
		editorContext_.worldContext_.gameTimer_ = &gameTimer_;
		editorContext_.worldContext_.system_ = system_.get();
		editorContext_.cameraContext_.editorCamera_ = &editorCamera_;
		editorContext_.cameraContext_.editorCameraController_ = &editorCameraController_;
		editorContext_.cameraContext_.canvasCamera_ = &canvasCamera_;
		editorContext_.cameraContext_.timelineCamera_ = &timelineCamera_;
		editorContext_.cameraContext_.modelTransformCamera_ = &modelTransformCamera_;
		editorContext_.cameraContext_.materialCamera_ = &materialCamera_;
		editorContext_.cameraContext_.timelineCameraController_ = &timelineCameraController_;
		editorContext_.cameraContext_.modelTransformCameraController_ = &modelTransformCameraController_;
		editorContext_.cameraContext_.materialCameraController_ = &materialCameraController_;
		editorContext_.graphicsContext_.graphics_ = graphics_.get();
		editorContext_.graphicsContext_.imgui_ = imgui_.get();
		editorContext_.cameraContext_.cameraSystem_ = &graphics_->GetCameraSystem();

		InputSystem::Initialize();
		LayerRegistry::Load();

		Char exePathBuffer[MAX_PATH]{};
		GetModuleFileNameA(nullptr, exePathBuffer, MAX_PATH);
		std::filesystem::path pluginDirectory = std::filesystem::path(exePathBuffer).parent_path();
		pluginHost_.Initialize(pluginDirectory, ImGui::GetCurrentContext());
		pluginHost_.Load(*world_);
		hotReload_.Initialize(pluginHost_);

		editorConfig_.Load();
		editorConfig_.Apply(editorCamera_, editorCameraController_, *imgui_);

		gameConfig_.Load();
		editorContext_.viewportContext_.outputResolution_ = gameConfig_.resolution_;
		editorContext_.viewportContext_.raytracing_.dlssRayReconstructionEnabled_ = gameConfig_.useDlss_;
		editorContext_.viewportContext_.raytracing_.upscaleMode_ = gameConfig_.upscaleMode_;
		editorContext_.viewportContext_.frameGeneration_.enabled_ = gameConfig_.useFrameGeneration_;
		editorContext_.viewportContext_.vsync_ = gameConfig_.vsync_;
		editorContext_.viewportContext_.resizeRequested_ = true;

		graphics_->Reflex(gameConfig_.useReflex_, false);
		graphics_->DeepDVC(gameConfig_.useDeepDVC_, 0.5f, 0.25f);
		graphics_->FrameGeneration(editorContext_.viewportContext_.frameGeneration_.enabled_);

		if (!editorConfig_.lastScenePath_.str().empty())
		{
			std::filesystem::path lastScenePath = editorConfig_.lastScenePath_.str();
			String raytracingSettingsJson;
			if (Scene::Load(*world_, *resource_, lastScenePath, &raytracingSettingsJson))
			{
				editorContext_.viewportContext_.raytracing_ = DeserializeRaytracingContext(raytracingSettingsJson);
				editorContext_.sceneContext_.currentScenePath_ = lastScenePath;
				editorContext_.viewportContext_.resizeRequested_ = true;
			}
		}

		editor_ = MakePtr<Editor>(editorContext_);
	}

	void Engine::Shutdown()
	{
		if (graphics_)
		{
			graphics_->WaitForGpuIdle();
		}

		if (imgui_)
		{
			editorConfig_.Capture(editorCamera_, editorCameraController_, *imgui_, editorContext_.sceneContext_.currentScenePath_);
			editorConfig_.Save();
		}

		if (window_)
		{
			window_->RestoreAccessibility();
		}

		if (world_)
		{
			pluginHost_.Unload(*world_);
		}

		InputSystem::Finalize();

		if (editor_)
		{
			editor_.reset();
			editor_ = nullptr;
		}

		if (resource_)
		{
			resource_.reset();
			resource_ = nullptr;
		}

		if (imgui_)
		{
			imgui_->Finalize();
			imgui_.reset();
			imgui_ = nullptr;
		}

		if (loaderSystem_)
		{
			loaderSystem_.reset();
			loaderSystem_ = nullptr;
		}
		
		if (fontManager_)
		{
			fontManager_->Finalize();
			fontManager_.reset();
			fontManager_ = nullptr;
		}

		if (joltManager_)
		{
			joltManager_->Finalize();
			joltManager_.reset();
			joltManager_ = nullptr;
		}

		if (criManager_)
		{
			criManager_->Finalize();
			criManager_.reset();
			criManager_ = nullptr;
		}

		if (graphics_)
		{
			graphics_->Finalize();
			graphics_.reset();
			graphics_ = nullptr;
		}
	}

	void Engine::MainLoop()
	{
		MSG msg{};
		while (WM_QUIT != msg.message) [[likely]]
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) [[unlikely]]
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				if (window_->ConsumeCloseRequested())
				{
					editorContext_.exitRequested_ = true;
				}

				window_->CalculateFrameStats();
				window_->GetTimer().Tick();
				worldTimer_.Tick(window_->GetTimer().Delta());
				gameTimer_.Tick(window_->GetTimer().Delta());

				InputSystem::Update();

				hotReload_.Tick(*world_);
				pluginHost_.Tick(*world_);

				editorContext_.uiFrame_++;

				if (editorContext_.viewportContext_.resizeRequested_)
				{
					editorContext_.viewportContext_.resizeRequested_ = false;

					ScResolution::ResSize outputSize = ToResSize(editorContext_.viewportContext_.outputResolution_);
					Uint32 outputWidth = static_cast<Uint32>(outputSize.Width);
					Uint32 outputHeight = static_cast<Uint32>(outputSize.Height);

					Float scale = UpscaleRenderScale(editorContext_.viewportContext_.raytracing_.upscaleMode_);
					Uint32 nativeWidth = Max<Uint32>(64, static_cast<Uint32>(outputWidth * scale + 0.5f));
					Uint32 nativeHeight = Max<Uint32>(64, static_cast<Uint32>(outputHeight * scale + 0.5f));

					graphics_->Resize(nativeWidth, nativeHeight, outputWidth, outputHeight, imgui_->GetDescriptorHeap());
				}

				if (editorContext_.viewportContext_.recreateRequested_)
				{
					editorContext_.viewportContext_.recreateRequested_ = false;

					graphics_->FrameGeneration(editorContext_.viewportContext_.frameGeneration_.enabled_);
				}

				graphics_->Begin();

				if (!graphics_->IsSplashFinished())
				{
					/// [EN] Loading now runs on its own background worker (see
					///      ResourceCache::StepAsync) instead of blocking this thread, so it
					///      can start immediately - no more need to hold it off until the
					///      intro finishes, since it no longer steals frame time from
					///      anything. StepAsync() is a no-op after its first call per Async()
					///      pass, so calling it every frame here is fine.
					/// [JP] 読み込みは今や専用のバックグラウンドワーカー上で走る
					///      （ResourceCache::StepAsync 参照）ので、このスレッドを
					///      ブロックしなくなった。もうフレーム時間を奪わないので、
					///      イントロが終わるまで遅らせる必要も無くなり、すぐ開始できる。
					///      StepAsync() は Async() パスごとの初回呼び出し以降は
					///      no-op なので、ここで毎フレーム呼んでも問題ない。
					resource_->StepAsync(*loaderSystem_, graphics_->GetContext()->GetDevice(), graphics_->GetContext()->GetDirectQueue(), graphics_->GetBindlessHeap(), graphics_->GetBC7CompressShader());

					graphics_->Clear();
					/// [JP] showWarning/showFiction は仮でtrue固定。Runtime書き出し時のチェックボックスから設定できるようにするのはこれから。
					graphics_->DrawSplashScreen(resource_->Complete(), resource_->Progress(), true, true);
					graphics_->End();
					graphics_->GetSwapChain()->Present(graphics_->GetContext()->GetDevice());
					continue;
				}

				InputSystem::SetInputEnabled(editor_->IsGameViewImageHovered());

				if (gameTimer_.IsPlaying())
				{
					PhysicsSystem::ResolveMeshColliders(*loaderSystem_, *resource_, *world_);
					PhysicsSystem::ResolveSoftbodies(*loaderSystem_, *resource_, *world_);

					joltManager_->SetActiveWorld(world_.get());

					while (worldTimer_.Step())
					{
						joltManager_->Execute(worldTimer_.FixedDeltaTime());
						system_->Step(*world_, worldTimer_.FixedDeltaTime());
					}
				}

				system_->Run(*world_, *resource_, *executor_, gameTimer_.DeltaTime(), gameTimer_.IsPlaying());

				if (gameTimer_.IsPlaying())
				{
					Scene::Update(gameTimer_.DeltaTime());
				}

				InputSystem::SetInputEnabled(true);

				criManager_->Execute();

				imgui_->NewFrame();

				editorCamera_.Tick(window_->GetTimer().Delta());

				canvasCamera_.Resize(ScResolution::SC_HD.Width, ScResolution::SC_HD.Height);
				canvasCamera_.Tick(window_->GetTimer().Delta());
				timelineCamera_.Tick(window_->GetTimer().Delta());
				modelTransformCamera_.Tick(window_->GetTimer().Delta());
				materialCamera_.Tick(window_->GetTimer().Delta());

				if (editorContext_.viewportContext_.raytracing_.daySystemEnabled_)
				{
					CelestialSystem::Advance(gameTimer_.DeltaTime(), editorContext_.viewportContext_.raytracing_.daySystem_);

					if (editorContext_.viewportContext_.raytracing_.sunLightEnabled_)
					{
						CelestialResult celestial = CelestialSystem::Compute(editorContext_.viewportContext_.raytracing_.daySystem_, editorContext_.viewportContext_.raytracing_.sunLight_, editorContext_.viewportContext_.raytracing_.moonLight_);
						for (Int index = 0; index < 3; index++)
						{
							editorContext_.viewportContext_.raytracing_.volumetricCloudScapes_.skyZenithColor_[index] = celestial.skyZenithColor_[index];
							editorContext_.viewportContext_.raytracing_.volumetricCloudScapes_.skyHorizonColor_[index] = celestial.skyHorizonColor_[index];
						}
					}
				}
				weatherSystem_.Execute(*world_, gameTimer_.DeltaTime(), editorContext_.viewportContext_.raytracing_.daySystem_.monthOfYear_, editorContext_.viewportContext_.raytracing_.volumetricCloudScapes_);

				graphics_->Raytracing(editor_->GetRaytracingSettings());
				graphics_->VerticalSync(editorContext_.viewportContext_.vsync_);

				graphics_->EditorRender(worldTimer_, editorCamera_, *loaderSystem_, *resource_, *world_, editor_->GetViewMode(), PhysicsSystem::GatherColliderInstances(*world_), editor_->GetSelectedEntity());
				graphics_->GameRender(gameTimer_, *loaderSystem_, *resource_, *world_);
				graphics_->CanvasRender(worldTimer_, canvasCamera_, *loaderSystem_, *resource_, *world_);

				if (editorContext_.timelinePreviewContext_.previewActive_)
				{
					graphics_->TimelineRender(worldTimer_, timelineCamera_, *loaderSystem_, *resource_, editorContext_.timelinePreviewContext_.previewMeshAssetId_, editorContext_.timelinePreviewContext_.previewAnimationAssetId_, editorContext_.timelinePreviewContext_.previewTime_, Matrix::Identity);
				}

				if (editorContext_.modelTransformPreviewContext_.previewActive_)
				{
					graphics_->ModelTransformRender(worldTimer_, modelTransformCamera_, *loaderSystem_, *resource_, editorContext_.modelTransformPreviewContext_.previewMeshAssetId_, 0, 0.0f, editorContext_.modelTransformPreviewContext_.previewWorldMatrix_);
				}

				if (editorContext_.materialPreviewContext_.previewActive_)
				{
					graphics_->MaterialRender(worldTimer_, materialCamera_, *loaderSystem_, *resource_, editorContext_.materialPreviewContext_.previewMeshAssetId_, editorContext_.materialPreviewContext_.previewSurfaceAssetId_, editorContext_.materialPreviewContext_.previewWorldMatrix_);
				}

				graphics_->Clear();

				imgui_->DockSpaceBegin(editor_->DrawToolbar());
				editor_->Draw(graphics_->EditorImGuiGPUHandle(), graphics_->GameImGuiGPUHandle(), graphics_->CanvasImGuiGPUHandle(), graphics_->TimelineImGuiGPUHandle(), graphics_->ModelTransformImGuiGPUHandle(), graphics_->MaterialImGuiGPUHandle(), graphics_->GetGpuProfiler());
				imgui_->DockSpaceEnd();

				imgui_->Render(graphics_->GetContext()->GetDirectList()->Get());

				graphics_->End();
				graphics_->GetSwapChain()->Present(graphics_->GetContext()->GetDevice());
			}
		}
	}

	Bool Engine::BootWindow(const Bootstrap& boot)
	{
		window_ = MakePtr<Window>();
		if (window_)
		{
			hwnd_ = window_->Create(boot);
			window_->GetTimer().Start();
			return true;
		}
		return false;
	}
}
