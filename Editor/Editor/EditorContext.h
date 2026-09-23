#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/World/ECS/Entity/Entity.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/Command/History.h>
#include <FoundationEngine/Resource/Scene/Scene.h>
#include <Editor/Editor/GizmoContext.h>
#include <FoundationEngine/Resource/ResourceSync.h>
#include <Editor/Editor/Panel/ResourceSyncControlPanel.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/ScreenSpace/ScreenSpaceContext.h>
#include <GraphicsEngine/Rasterization/RasterizationContext.h>
#include <GraphicsEngine/Quality/GraphicsQuality.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;
	class SystemScheduler;
	class GameTimer;
	class EditorCamera;
	class EditorCameraController;
	class CanvasCamera;
	class PreviewCamera;
	class PreviewCameraController;
	class CameraSystem;
	struct LoaderSystem;
	class Graphics;
	class ImGuiRenderer;
	class AnimatorControllerPanel;
	class TimelinePanel;
	class LayerSettingsPanel;
	class MaterialViewerPanel;
	class SkeletonControllerPanel;
	class AvatarPanel;
	class BootScreenPanel;
	class AvatarMesh;
	class BootScreenRenderer;
	struct BootConfig;

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
		Graphics* graphics_ = nullptr;

		ImGuiRenderer* imgui_ = nullptr;
	};

	struct CameraContext
	{
		EditorCamera* editorCamera_ = nullptr;
		EditorCameraController* editorCameraController_ = nullptr;
		CanvasCamera* canvasCamera_ = nullptr;
		PreviewCamera* timelineCamera_ = nullptr;
		PreviewCamera* modelTransformCamera_ = nullptr;
		PreviewCamera* materialCamera_ = nullptr;
		PreviewCamera* skeletonControllerCamera_ = nullptr;
		PreviewCamera* avatarCamera_ = nullptr;
		PreviewCameraController* timelineCameraController_ = nullptr;
		PreviewCameraController* modelTransformCameraController_ = nullptr;
		PreviewCameraController* materialCameraController_ = nullptr;
		PreviewCameraController* skeletonControllerCameraController_ = nullptr;
		PreviewCameraController* avatarCameraController_ = nullptr;
		CameraSystem* cameraSystem_ = nullptr;
	};

	struct SelectionContext
	{
		Entity selectedEntity_ = Entity::Null();
		Actor selectedActor_;
		DynamicArray<Actor> selectedActors_;
	};

	struct SceneContext
	{
		Scene playModeScene_;
		RaytracingContext playModeRaytracing_;
		ScreenSpaceContext playModeScreenSpace_;
		RasterizationContext playModeRasterization_;
		Float playModeMasterVolume_ = 1.0f;
		DynamicArray<Float> playModeCategoryVolumes_;
		History history_;
		std::filesystem::path currentScenePath_;
		Uint32 requestedSceneAssetID_ = 0;
	};

	struct FrameGenerationContext
	{
		Bool enabled_ = false;
	};

	struct UpscaleContext
	{
		Bool dlssRayReconstructionEnabled_ = false;
		UpscaleMode upscaleMode_ = UpscaleMode::Balanced;
	};

	struct ViewportContext
	{
		GuizmoContext guizmo_;
		ViewMode viewMode_ = ViewMode::Lit;
		RaytracingContext raytracing_;
		ScreenSpaceContext screenSpace_;
		RasterizationContext rasterization_;
		GraphicsQualityPreset qualityPreset_ = GraphicsQualityPreset::Custom;
		FrameGenerationContext frameGeneration_;
		UpscaleContext upscale_;
		ResolutionPreset outputResolution_ = ResolutionPreset::HD;
		Bool vsync_ = false;
		Bool resizeRequested_ = false;
		Bool recreateRequested_ = false;
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
		Uint32 requestedAssetId_ = 0;
	};

	struct MaterialPreviewContext
	{
		Bool previewActive_ = false;
		Uint32 previewMeshAssetId_ = 0;
		Uint32 previewSurfaceAssetId_ = 0;
		Matrix previewWorldMatrix_ = Matrix::Identity;
	};

	struct SkeletonControllerPreviewContext
	{
		Bool previewActive_ = false;
		Uint32 previewMeshAssetId_ = 0;
		Matrix previewWorldMatrix_ = Matrix::Identity;
		Int selectedNodeIndex_ = -1;
	};

	struct AvatarPreviewContext
	{
		Bool previewActive_ = false;
		AvatarMesh* mesh_ = nullptr;
		std::span<const Vector3> positions_;
		std::span<const Vector3> normals_;
		Uint32 boneCount_ = 0;
		Matrix previewWorldMatrix_ = Matrix::Identity;
		Uint32 regionTextureIndices_[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		Uint32 regionCount_ = 0;
	};

	struct BootScreenPreviewContext
	{
		Bool previewActive_ = false;
		BootScreenRenderer* renderer_ = nullptr;
		const BootConfig* config_ = nullptr;
		Float progress_ = 0.0f;
	};

	struct PanelContext
	{
		AnimatorControllerPanel* animatorControllerPanel_ = nullptr;
		TimelinePanel* timelinePanel_ = nullptr;
		LayerSettingsPanel* layerSettingsPanel_ = nullptr;
		MaterialViewerPanel* materialViewerPanel_ = nullptr;
		SkeletonControllerPanel* skeletonControllerPanel_ = nullptr;
		AvatarPanel* avatarPanel_ = nullptr;
		BootScreenPanel* bootScreenPanel_ = nullptr;
	};

	struct EditorContext
	{
		ResourceSync* resourceSync_ = nullptr;
		WorldContext worldContext_;
		GraphicsContext graphicsContext_;
		CameraContext cameraContext_;
		SelectionContext selectionContext_;
		SceneContext sceneContext_;
		ViewportContext viewportContext_;
		TimelinePreviewContext timelinePreviewContext_;
		ModelTransformPreviewContext modelTransformPreviewContext_;
		MaterialPreviewContext materialPreviewContext_;
		SkeletonControllerPreviewContext skeletonControllerPreviewContext_;
		AvatarPreviewContext avatarPreviewContext_;
		BootScreenPreviewContext bootScreenPreviewContext_;
		PanelContext panelContext_;

		Uint64 uiFrame_ = 0;

		Bool exitRequested_ = false;
	};
}
