#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>

#include <Editor/Editor/Panel/HierarchyPanel.h>
#include <Editor/Editor/Panel/InspectorPanel.h>
#include <Editor/Editor/Panel/ToolPanel.h>
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
#include <Editor/Editor/Panel/AnimatorControllerPanel.h>
#include <Editor/Editor/Panel/TimelinePanel.h>
#include <Editor/Editor/Panel/ModelTransformPanel.h>

namespace SeedCore
{
	class Editor
	{
	public:
		Editor(EditorContext& context);
		~Editor() = default;

		Float DrawToolbar();

		/// [EN] gpuProfiler is forwarded to ToolPanel -> ProfilerPanel. Passed
		///      explicitly rather than stored in EditorContext so the panel's
		///      dependency on the renderer stays visible in the signatures.
		/// [JP] gpuProfiler は ToolPanel → ProfilerPanel へ受け渡す。EditorContext
		///      に持たせずに引数で通すことで、パネルがレンダラーに依存している
		///      ことをシグネチャに出しておく。
		void Draw(D3D12_GPU_DESCRIPTOR_HANDLE editorFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE gameFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE canvasFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE previewFrameBufferHandle, const GpuProfiler& gpuProfiler);

		[[nodiscard]] ViewMode GetViewMode()const;

		[[nodiscard]] Entity GetSelectedEntity()const;

		[[nodiscard]] const RaytracingContext& GetRaytracingSettings()const;

		[[nodiscard]] Bool IsGameViewImageHovered()const { return gameWindowPanel_->IsImageHovered(); }

	private:
		EditorContext& context_;

		Float toolbarHeight_ = 0.0f;

		ImGuiTexture imguiTexture_;

		ResourcePtr<HierarchyPanel> hierarchyPanel_;
		ResourcePtr<InspectorPanel> inspectorPanel_;
		ResourcePtr<ToolPanel> toolPanel_;
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
		ResourcePtr<AnimatorControllerPanel> animatorControllerPanel_;
		ResourcePtr<TimelinePanel> timelinePanel_;
		ResourcePtr<ModelTransformPanel> modelTransformPanel_;
	};
}