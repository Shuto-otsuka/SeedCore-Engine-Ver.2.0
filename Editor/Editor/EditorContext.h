#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/WorldSnapshot.h>
#include <FoundationEngine/ECS/History.h>
#include <Editor/Editor/GizmoContext.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;
	class SystemScheduler;
	class DescriptorHeap;
	class GameTimer;
	class EditorCamera;
	class EditorCameraController;
	class CanvasCamera;
	class PreviewCamera;
	class PreviewCameraController;
	class CameraSystem;
	struct LoaderSystem;
	class BC7CompressShader;
	class BindlessHeap;
	class D3D12CommandQueue;
	class ImGuiRenderer;
	class AnimatorControllerPanel;
	class TimelinePanel;
	class LayerSettingsPanel;

	struct WorldContext
	{
		World* world_ = nullptr;
		ResourceCache* resource_ = nullptr;
		LoaderSystem* loader_ = nullptr;
		GameTimer* gameTimer_ = nullptr;
		SystemScheduler* system_ = nullptr;
	};

	struct GraphicsContext
	{
		ImGuiRenderer* imgui_ = nullptr;

		ID3D12Device* device_ = nullptr;
		D3D12CommandQueue* cmdQueue_ = nullptr;
		BC7CompressShader* bc7Shader_ = nullptr;
		IDXGIAdapter4* adapter_ = nullptr;
		DescriptorHeap* descHeap_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;
	};

	struct CameraContext
	{
		EditorCamera* editorCamera_ = nullptr;
		EditorCameraController* editorCameraController_ = nullptr;
		CanvasCamera* canvasCamera_ = nullptr;
		PreviewCamera* timelineCamera_ = nullptr;
		PreviewCamera* modelTransformCamera_ = nullptr;
		PreviewCameraController* timelineCameraController_ = nullptr;
		PreviewCameraController* modelTransformCameraController_ = nullptr;
		CameraSystem* cameraSystem_ = nullptr;
	};

	struct SelectionContext
	{
		Entity selectedEntity_ = Entity::Null();
		Actor* selectedActor_ = nullptr;
		DynamicArray<Actor*> selectedActors_;
	};

	struct SceneContext
	{
		WorldSnapshot worldSnapshot_;
		History history_;
		std::filesystem::path currentScenePath_;
		Uint32 requestedSceneAssetID_ = 0;
	};

	struct ViewportContext
	{
		GuizmoContext guizmo_;
		ViewMode viewMode_ = ViewMode::Lit;
		RaytracingContext raytracing_;
		ResolutionPreset outputResolution_ = ResolutionPreset::HD;
		Bool resizeRequested_ = false;
		Bool vsync_ = false;
	};

	struct TimelinePreviewContext
	{
		Bool previewActive_ = false;
		Uint32 previewMeshAssetId_ = 0;
		Uint32 previewAnimationAssetId_ = 0;
		Float previewTime_ = 0.0f;
	};

	struct ModelTransformPreviewContext
	{
		Bool previewActive_ = false;
		Uint32 previewMeshAssetId_ = 0;
		Matrix previewWorldMatrix_ = Matrix::Identity;
	};

	struct PanelContext
	{
		AnimatorControllerPanel* animatorControllerPanel_ = nullptr;
		TimelinePanel* timelinePanel_ = nullptr;
		LayerSettingsPanel* layerSettingsPanel_ = nullptr;
	};

	struct EditorContext
	{
		WorldContext worldContext_;
		GraphicsContext graphicsContext_;
		CameraContext cameraContext_;
		SelectionContext selectionContext_;
		SceneContext sceneContext_;
		ViewportContext viewportContext_;
		TimelinePreviewContext timelinePreviewContext_;
		ModelTransformPreviewContext modelTransformPreviewContext_;
		PanelContext panelContext_;

		Uint64 uiFrame_ = 0;

		Bool exitRequested_ = false;
	};
}
