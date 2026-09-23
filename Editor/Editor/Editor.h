#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>

#include <Editor/Editor/Panel/HierarchyPanel.h>
#include <Editor/Editor/Panel/InspectorPanel.h>
#include <Editor/Editor/Panel/DiagnosticsPanel.h>
#include <Editor/Editor/Panel/EditorWindowPanel.h>
#include <Editor/Editor/Panel/GameWindowPanel.h>
#include <Editor/Editor/Panel/CanvasViewPanel.h>
#include <Editor/Editor/Panel/ContentsDrawerPanel.h>
#include <Editor/Editor/Panel/ControlPanel.h>
#include <Editor/Editor/Panel/MenuBarPanel.h>
#include <Editor/Editor/Panel/ShortCutKeyPanel.h>
#include <Editor/Editor/Panel/SpecMemoPanel.h>
#include <Editor/Editor/Panel/TodoListPanel.h>
#include <Editor/Editor/Panel/VersionPanel.h>
#include <Editor/Editor/Panel/ConfigPanel.h>
#include <Editor/Editor/Panel/LayerSettingsPanel.h>
#include <Editor/Editor/Panel/AnimatorControllerPanel.h>
#include <Editor/Editor/Panel/TimelinePanel.h>
#include <Editor/Editor/Panel/SkeletonControllerPanel.h>
#include <Editor/Editor/Panel/MaterialViewerPanel.h>
#include <Editor/Editor/Panel/ModelTransformPanel.h>
#include <Editor/Editor/Panel/AvatarPanel.h>
#include <Editor/Editor/Panel/BootScreenPanel.h>

namespace SeedCore
{
	class Editor
	{
	public:
		Editor(EditorContext& context);
		~Editor() = default;

		Float DrawToolbar();

		/// [EN] gpuProfiler is forwarded to DiagnosticsPanel -> ProfilerPanel. Passed
		///      explicitly rather than stored in EditorContext so the panel's
		///      dependency on the renderer stays visible in the signatures.
		/// [JP] gpuProfiler は DiagnosticsPanel → ProfilerPanel へ受け渡す。EditorContext
		///      に持たせずに引数で通すことで、パネルがレンダラーに依存している
		///      ことをシグネチャに出しておく。
		void Draw(D3D12_GPU_DESCRIPTOR_HANDLE editorFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE gameFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE canvasFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE timelinePreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE modelTransformPreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE materialPreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE skeletonControllerPreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE avatarPreviewFrameBufferHandle, const GpuProfiler& gpuProfiler);

		[[nodiscard]] ViewMode GetViewMode()const;

		[[nodiscard]] DynamicArray<Entity> GetSelectedEntities()const;

		[[nodiscard]] const RaytracingContext& GetRaytracingSettings()const;

		[[nodiscard]] Bool GameViewImageHovered()const { return gameWindowPanel_->ImageHovered(); }

	private:
		void PruneDeadSelection();

	private:
		EditorContext& context_;
		ResourceSync resourceSync_;

		/// [EN] The scene path the library was last told about, so it is told again only when the Editor opens a different one.
		/// [JP] ライブラリへ最後に伝えた Scene の位置。別の Scene を開いた時だけ伝え直すために持つ。
		std::filesystem::path followedScenePath_;

		Float toolbarHeight_ = 0.0f;

		ImGuiTexture imguiTexture_;

		ResourcePtr<HierarchyPanel> hierarchyPanel_;
		ResourcePtr<InspectorPanel> inspectorPanel_;
		ResourcePtr<DiagnosticsPanel> diagnosticsPanel_;
		ResourcePtr<EditorWindowPanel> editorWindowPanel_;
		ResourcePtr<GameWindowPanel> gameWindowPanel_;
		ResourcePtr<CanvasViewPanel> canvasViewPanel_;
		ResourcePtr<ContentsDrawerPanel> contentsDrawerPanel_;
		ResourcePtr<ControlPanel> controlPanel_;
		ResourcePtr<MenuBarPanel> menuBarPanel_;
		ResourcePtr<ShortCutKeyPanel> shortCutKeyPanel_;
		ResourcePtr<SpecMemoPanel> specMemoPanel_;
		ResourcePtr<TodoListPanel> todoListPanel_;
		ResourcePtr<VersionPanel> versionPanel_;
		ResourcePtr<ConfigPanel> configPanel_;
		ResourcePtr<LayerSettingsPanel> layerSettingsPanel_;
		ResourcePtr<AnimatorControllerPanel> animatorControllerPanel_;
		ResourcePtr<TimelinePanel> timelinePanel_;
		ResourcePtr<SkeletonControllerPanel> skeletonControllerPanel_;
		ResourcePtr<MaterialViewerPanel> materialViewerPanel_;
		ResourcePtr<ModelTransformPanel> modelTransformPanel_;
		ResourcePtr<AvatarPanel> avatarPanel_;
		ResourcePtr<BootScreenPanel> bootScreenPanel_;
	};
}
