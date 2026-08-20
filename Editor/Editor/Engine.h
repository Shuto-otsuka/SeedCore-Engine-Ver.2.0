#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/System/SystemScheduler.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/Scene.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Time/WorldTimer.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <AudioEngine/CRI/CriManager.h>
#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <GraphicsEngine/Font/FontManager.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/EditorCameraController.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <GraphicsEngine/System/CelestialSystem.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <Editor/Editor/Window.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/Editor.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <Editor/Editor/Build/HotReload.h>
#include <Editor/Editor/Build/Config.h>

namespace SeedCore
{
	struct Bootstrap;

	class Engine :public NonTransferable
	{
	public:
		Engine() = default;
		~Engine() = default;

		void Boot(const Bootstrap& boot);
		void Shutdown();
		void MainLoop();

	private:
		Bool BootWindow(const Bootstrap& boot);

	private:
		HWND hwnd_;

	private:
		ResourcePtr<Window> window_;

		ResourcePtr<World> world_;

		ResourcePtr<SystemScheduler> system_;

		ResourcePtr<JobExecutor> executor_;

		ResourcePtr<ResourceCache> resource_;

		ResourcePtr<CriManager> criManager_;

		ResourcePtr<JoltManager> joltManager_;

		ResourcePtr<FontManager> fontManager_;

		ResourcePtr<Graphics> graphics_;

		ResourcePtr<Editor> editor_;

		ResourcePtr<ImGuiRenderer> imgui_;

		ResourcePtr<LoaderSystem> loaderSystem_;

		HotReload hotReload_;

		EditorConfig editorConfig_;

		GameConfig gameConfig_;

		EditorContext editorContext_;

		WorldTimer worldTimer_;

		GameTimer gameTimer_;

		EditorCamera editorCamera_;

		EditorCameraController editorCameraController_;

		CanvasCamera canvasCamera_;

		PreviewCamera timelineCamera_;
		PreviewCamera modelTransformCamera_;
		PreviewCameraController timelineCameraController_;
		PreviewCameraController modelTransformCameraController_;

		WeatherSystem weatherSystem_;
	};

	class JobScheduler : public JobWorkerInterface
	{
	public:
		void SchedulerPrologue(SeedCore::JobWorker& worker) override
		{
			/// No Code
		}

		void SchedulerEpilogue(SeedCore::JobWorker& worker, std::exception_ptr ptr) override
		{
			/// No Code
		}
	};
}