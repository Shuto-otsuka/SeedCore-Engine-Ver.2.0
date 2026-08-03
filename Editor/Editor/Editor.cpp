#include <Editor/Editor/Editor.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimatorControllerState.h>

namespace SeedCore
{
	Editor::Editor(EditorContext& context) :context_(context), imguiTexture_(context_)
	{
		hierarchyPanel_ = MakePtr<HierarchyPanel>(context_, imguiTexture_);
		inspectorPanel_ = MakePtr<InspectorPanel>(context_, imguiTexture_);
		toolPanel_ = MakePtr<ToolPanel>(context_, imguiTexture_);
		editorWindowPanel_ = MakePtr<EditorWindowPanel>(context_, imguiTexture_);
		gameWindowPanel_ = MakePtr<GameWindowPanel>(*context_.cameraSystem_);
		canvasViewPanel_ = MakePtr<CanvasViewPanel>(context_, imguiTexture_);
		contentsDrawerPanel_ = MakePtr<ContentsDrawerPanel>(context_, imguiTexture_);
		controlPanel_ = MakePtr<ControlPanel>(context_, imguiTexture_);
		menuBarPanel_ = MakePtr<MenuBarPanel>(context_);
		shortCutKeyPanel_ = MakePtr<ShortCutKeyPanel>(context_);
		specMemoPanel_ = MakePtr<SpecMemoPanel>(context_);
		todoListPanel_ = MakePtr<TodoListPanel>(context_);
		versionPanel_ = MakePtr<VersionPanel>(context_);
		configPanel_ = MakePtr<ConfigPanel>(context_);
		animatorControllerPanel_ = MakePtr<AnimatorControllerPanel>(context_);
		timelinePanel_ = MakePtr<TimelinePanel>(context_);
		modelTransformPanel_ = MakePtr<ModelTransformPanel>(context_);

		SC_LOG_NOTICE("エディターの初期化が完了しました");
	}

	Float Editor::DrawToolbar()
	{
		/// [EN] Must run before any ImGuizmo call this frame (per ImGuizmo's
		///      own contract: "call BeginFrame right after ImGui NewFrame").
		///      DrawToolbar() runs first each frame (Engine::MainLoop calls it
		///      before Draw()), and both TimelinePanel's preview grid and
		///      EditorWindowPanel's transform gizmo use ImGuizmo — a single
		///      BeginFrame() here covers both instead of leaving it buried
		///      inside EditorWindowPanel::Draw(), where TimelinePanel's grid
		///      (drawn earlier, from here) would run on last frame's stale
		///      ImGuizmo state.
		/// [JP] このフレームで最初のImGuizmo呼び出しより前に実行する必要がある
		///      (ImGuizmo自身の規約: 「BeginFrameはImGuiのNewFrame直後に呼ぶ」)。
		///      DrawToolbar()は毎フレーム最初に呼ばれる(Engine::MainLoopが
		///      Draw()より前に呼ぶ)、かつTimelinePanelのプレビューグリッドと
		///      EditorWindowPanelのトランスフォームギズモの両方がImGuizmoを
		///      使うため、ここで1回BeginFrame()すれば両方をカバーできる —
		///      EditorWindowPanel::Draw()内に埋もれたままだと、それより先に
		///      呼ばれるTimelinePanelのグリッドが前フレームの古いImGuizmo
		///      状態のまま描画されてしまう。
		ImGuizmo::BeginFrame();

		menuBarPanel_->Draw();
		if (menuBarPanel_->ConsumeShortCutKeyRequest())
		{
			shortCutKeyPanel_->Open();
		}
		if (menuBarPanel_->ConsumeSpecMemoRequest())
		{
			specMemoPanel_->Open();
		}
		if (menuBarPanel_->ConsumeConsoleRequest())
		{
			toolPanel_->ShowConsoleTab();
		}
		if (menuBarPanel_->ConsumeProfilerRequest())
		{
			toolPanel_->ShowProfilerTab();
		}
		if (menuBarPanel_->ConsumeTodoListRequest())
		{
			todoListPanel_->Open();
		}
		if (menuBarPanel_->ConsumeVersionRequest())
		{
			versionPanel_->Open();
		}
		if (menuBarPanel_->ConsumeConfigRequest())
		{
			configPanel_->Open();
		}
		if (menuBarPanel_->ConsumeAnimatorControllerRequest())
		{
			Animator* animator = context_.selectedActor_ ? const_cast<Animator*>(context_.selectedActor_->GetComponent<Animator>()) : nullptr;
			animatorControllerPanel_->Open(animator);
		}
		if (AnimatorControllerRequest::requested_)
		{
			AnimatorControllerRequest::requested_ = false;
			Animator* animator = context_.selectedActor_ ? const_cast<Animator*>(context_.selectedActor_->GetComponent<Animator>()) : nullptr;
			animatorControllerPanel_->Open(animator);
		}
		if (menuBarPanel_->ConsumeTimelineRequest())
		{
			timelinePanel_->Open();
		}
		if (TimelineRequest::requested_)
		{
			TimelineRequest::requested_ = false;
			timelinePanel_->Open();
		}
		if (menuBarPanel_->ConsumeModelTransformRequest())
		{
			modelTransformPanel_->Open();
		}
		shortCutKeyPanel_->Draw();
		specMemoPanel_->Draw();
		todoListPanel_->Draw();
		versionPanel_->Draw();
		configPanel_->Draw();
		animatorControllerPanel_->Draw();
		timelinePanel_->Draw();
		modelTransformPanel_->Draw();
		toolbarHeight_ = controlPanel_->Draw();
		return toolbarHeight_;
	}

	void Editor::Draw(D3D12_GPU_DESCRIPTOR_HANDLE editorFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE gameFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE canvasFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE previewFrameBufferHandle, const GpuProfiler& gpuProfiler)
	{
		timelinePanel_->SetPreviewHandle(previewFrameBufferHandle);
		modelTransformPanel_->SetPreviewHandle(previewFrameBufferHandle);

		gameWindowPanel_->Draw(gameFrameBufferHandle, toolbarHeight_);

		if (!gameWindowPanel_->IsFullscreen())
		{
			hierarchyPanel_->Draw();
			inspectorPanel_->Draw();
			toolPanel_->Draw(gpuProfiler);
			canvasViewPanel_->Draw(canvasFrameBufferHandle);
			editorWindowPanel_->Draw(editorFrameBufferHandle);
			contentsDrawerPanel_->Draw();
		}
	}

	ViewMode Editor::GetViewMode()const
	{
		return menuBarPanel_->GetViewMode();
	}

	Entity Editor::GetSelectedEntity()const
	{
		return context_.selectedEntity_;
	}

	const RaytracingContext& Editor::GetRaytracingSettings()const
	{
		return context_.raytracing_;
	}
}
