#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Context/D3D12Context.h>
#include <GraphicsEngine/D3D12/SwapChain/SwapChain.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/Model/BC7CompressShader.h>
#include <GraphicsEngine/DLSS/DlssManager.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/CameraSystem.h>
#include <GraphicsEngine/System/MovieSystem.h>
#include <GraphicsEngine/Renderer/Renderer.h>
#include <GraphicsEngine/System/SplashScreen.h>
#include <GraphicsEngine/System/FadeScreen.h>

namespace SeedCore
{
	class WorldTimer;
	class EditorCamera;
	class CanvasCamera;
	class PreviewCamera;
	class World;
	struct LoaderSystem;
	class ResourceCache;

	class GameTimer;

	class SEEDCORE_API Graphics
	{
	public:
		Graphics() = default;
		~Graphics() = default;

		Bool Initialize(HWND hwnd, Float width, Float height);

		void Finalize();

		void WaitForGpuIdle();

		void Resize(Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight, DescriptorHeap* imguiHeap);

		void EditorRender(WorldTimer& timer, const EditorCamera& editorCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, ViewMode viewMode, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity = Entity::Null());

		void GameRender(GameTimer& timer, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world);

		void CanvasRender(WorldTimer& timer, const CanvasCamera& canvasCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world);

		void TimelineRender(WorldTimer& timer, const PreviewCamera& timelineCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void ModelTransformRender(WorldTimer& timer, const PreviewCamera& modelTransformCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void MaterialRender(WorldTimer& timer, const PreviewCamera& materialCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 surfaceAssetId, const Matrix& worldMatrix);

		void SkeletonControllerRender(WorldTimer& timer, const PreviewCamera& skeletonControllerCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix, Int selectedNodeIndex);

		void Begin();

		void End();

		void Clear();

		void VerticalSync(Bool vsync);

		Bool VerticalSync()const;

	public:
		void Raytracing(const RaytracingContext& settings);

		void Reflex(Bool enable, Bool useBoost);

		void DeepDVC(Bool enable, Float intensity, Float saturationBoost);

		void FrameGeneration(Bool enable);

	public:
		D3D12Context* GetContext()const;

		SwapChain* GetSwapChain()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetEditorGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGameGPUHandle()const;

		BindlessHeap* GetBindlessHeap()const;

		ShaderCache& GetShaderCache()const;

		BC7CompressShader& GetBC7CompressShader()const;

	public:
		CameraSystem& GetCameraSystem();

	public:
		void DrawSplashScreen(Bool loadComplete, Float progress, Bool showWarning, Bool showFiction);

		[[nodiscard]] Bool IsSplashFinished()const;

	public:
		static void SetImGuiContext(ImGuiContext* context);

		void RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] const GpuProfiler& GetGpuProfiler()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE TimelineImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ModelTransformImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE MaterialImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE SkeletonControllerImGuiGPUHandle()const;

	private:
		Float width_ = ScResolution::SC_HD.Width;

		Float height_ = ScResolution::SC_HD.Height;

		Uint32 nativeWidth_ = static_cast<Uint32>(ScResolution::SC_HD.Width);
		Uint32 nativeHeight_ = static_cast<Uint32>(ScResolution::SC_HD.Height);
		Uint32 outputWidth_ = static_cast<Uint32>(ScResolution::SC_HD.Width);
		Uint32 outputHeight_ = static_cast<Uint32>(ScResolution::SC_HD.Height);

		ResourcePtr<D3D12Context> context_;

		ResourcePtr<SwapChain> swapChain_;

		ResourcePtr<ShaderCache> shaderCache_;

		ResourcePtr<BC7CompressShader> bc7CompressShader_;

		ResourcePtr<BindlessHeap> bindlessHeap_;

		ResourcePtr<DlssManager> dlssManager_;

		ResourcePtr<SceneSystem> editorSceneSystem_;
		ResourcePtr<SceneSystem> gameSceneSystem_;
		ResourcePtr<SceneSystem> canvasSceneSystem_;

		CameraSystem cameraSystem_;

		MovieSystem movieSystem_;

		ResourcePtr<Renderer> renderer_;

		SplashScreen splashScreen_;

		FadeScreen fadeScreen_;
	};
}