#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/ECS/System/SystemScheduler.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Resource/Config/GameConfig.h>
#include <FoundationEngine/Time/WorldTimer.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <FoundationEngine/Plugin/PluginHost.h>
#include <AudioEngine/CRI/CriManager.h>
#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <GraphicsEngine/Font/FontManager.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <Runtime/Application/Window.h>

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
		HWND hwnd_ = nullptr;

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

		ResourcePtr<LoaderSystem> loaderSystem_;

		PluginHost pluginHost_;

		GameConfig gameConfig_;

		WorldTimer worldTimer_;

		GameTimer gameTimer_;

		RaytracingContext raytracing_;

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
