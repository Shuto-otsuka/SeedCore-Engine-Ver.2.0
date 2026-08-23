#include <Editor/Editor/Editor.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimatorControllerState.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

namespace SeedCore
{
	Editor::Editor(EditorContext& context) :context_(context), imguiTexture_(context_)
	{
		hierarchyPanel_ = MakePtr<HierarchyPanel>(context_, imguiTexture_);
		inspectorPanel_ = MakePtr<InspectorPanel>(context_, imguiTexture_);
		toolPanel_ = MakePtr<ToolPanel>(context_, imguiTexture_);
		editorWindowPanel_ = MakePtr<EditorWindowPanel>(context_, imguiTexture_);
		gameWindowPanel_ = MakePtr<GameWindowPanel>(*context_.cameraContext_.cameraSystem_);
		canvasViewPanel_ = MakePtr<CanvasViewPanel>(context_, imguiTexture_);
		contentsDrawerPanel_ = MakePtr<ContentsDrawerPanel>(context_, imguiTexture_);
		controlPanel_ = MakePtr<ControlPanel>(context_, imguiTexture_);
		menuBarPanel_ = MakePtr<MenuBarPanel>(context_);
		shortCutKeyPanel_ = MakePtr<ShortCutKeyPanel>(context_);
		specMemoPanel_ = MakePtr<SpecMemoPanel>(context_);
		todoListPanel_ = MakePtr<TodoListPanel>(context_);
		versionPanel_ = MakePtr<VersionPanel>(context_);
		configPanel_ = MakePtr<ConfigPanel>(context_);
		layerSettingsPanel_ = MakePtr<LayerSettingsPanel>(context_);
		animatorControllerPanel_ = MakePtr<AnimatorControllerPanel>(context_);
		timelinePanel_ = MakePtr<TimelinePanel>(context_);
		boneControllerPanel_ = MakePtr<BoneControllerPanel>(context_);
		materialPanel_ = MakePtr<MaterialPanel>(context_);
		modelTransformPanel_ = MakePtr<ModelTransformPanel>(context_);

		context_.panelContext_.animatorControllerPanel_ = &*animatorControllerPanel_;
		context_.panelContext_.timelinePanel_ = &*timelinePanel_;
		context_.panelContext_.layerSettingsPanel_ = &*layerSettingsPanel_;

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
		if (menuBarPanel_->ConsumeLayerSettingsRequest())
		{
			layerSettingsPanel_->Open();
		}
		if (menuBarPanel_->ConsumeAnimatorControllerRequest())
		{
			Animator* animator = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_->GetComponent<Animator>()) : nullptr;
			animatorControllerPanel_->Open(animator);
		}
		if (AnimatorControllerRequest::requested_)
		{
			AnimatorControllerRequest::requested_ = false;
			Animator* animator = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_->GetComponent<Animator>()) : nullptr;
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
		if (menuBarPanel_->ConsumeBoneControllerRequest())
		{
			boneControllerPanel_->Open();
		}
		if (menuBarPanel_->ConsumeMaterialRequest())
		{
			materialPanel_->Open();
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
		layerSettingsPanel_->Draw();
		toolbarHeight_ = controlPanel_->Draw();
		return toolbarHeight_;
	}

	void Editor::Draw(D3D12_GPU_DESCRIPTOR_HANDLE editorFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE gameFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE canvasFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE timelinePreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE modelTransformPreviewFrameBufferHandle, const GpuProfiler& gpuProfiler)
	{
		timelinePanel_->SetPreviewHandle(timelinePreviewFrameBufferHandle);
		modelTransformPanel_->SetPreviewHandle(modelTransformPreviewFrameBufferHandle);

		/// [EN] Must run after DockSpaceBegin() (called by Engine before this
		///      function) so ImGui::DockSpace() has already created/refreshed
		///      this frame's dock node — DockBuilderDockWindow() inside these
		///      panels' Draw() would otherwise target a node ImGui hasn't
		///      registered as active yet this frame and silently drop the
		///      request (this was the actual reason forced docking never
		///      took effect when these were called from DrawToolbar(),
		///      which runs before DockSpaceBegin()).
		/// [JP] (Engineがこの関数より前に呼ぶ)DockSpaceBegin()の後に実行する
		///      必要がある — ImGui::DockSpace()が今フレームのドックノードを
		///      生成/更新済みである前提のため。これらのパネルのDraw()内の
		///      DockBuilderDockWindow()は、そうしないとImGuiが今フレームまだ
		///      アクティブと認識していないノードを対象にリクエストしてしまい、
		///      黙って無視されていた(DockSpaceBegin()より前に実行される
		///      DrawToolbar()から呼んでいた際、強制ドッキングが一切効かなかった
		///      本当の原因)。
		animatorControllerPanel_->Draw();
		timelinePanel_->Draw();
		boneControllerPanel_->Draw();
		materialPanel_->Draw();
		modelTransformPanel_->Draw();

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

		if (!context_.worldContext_.gameTimer_->IsPlaying() && !ImGui::GetIO().WantTextInput)
		{
			Bool ctrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
			if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_Z))
			{
				context_.sceneContext_.history_.Undo();
			}
			if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_Y))
			{
				context_.sceneContext_.history_.Redo();
			}

			/// [EN] Unreal-style "frame selected": Ctrl+F slides the editor
			///      viewport camera to the currently selected actor (see
			///      EditorCamera::FocusOn, and HierarchyPanel's
			///      double-click, which does the same).
			/// [JP] Unreal 風の「選択対象にフレーム」: Ctrl+F で現在選択中の
			///      アクターへエディタビューポートのカメラがスライドする
			///      （EditorCamera::FocusOn 参照。HierarchyPanel の
			///      ダブルクリックも同じ処理）。
			if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_F) && context_.selectionContext_.selectedActor_ && context_.cameraContext_.editorCamera_)
			{
				Actor* selectedActor = context_.selectionContext_.selectedActor_;
				const Matrix& worldMatrix = selectedActor->GetWorldMatrix();

				Float radius = 0.0f;
				const Bounds* bounds = selectedActor->GetComponent<Bounds>();
				if (bounds)
				{
					Float worldScale = Max(Max(Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13).Length(), Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23).Length()), Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33).Length());
					radius = bounds->extent_.Length() * worldScale;
				}

				context_.cameraContext_.editorCamera_->FocusOn(Vector3(worldMatrix._41, worldMatrix._42, worldMatrix._43), radius);
			}
		}
	}

	ViewMode Editor::GetViewMode()const
	{
		return menuBarPanel_->GetViewMode();
	}

	Entity Editor::GetSelectedEntity()const
	{
		return context_.selectionContext_.selectedEntity_;
	}

	const RaytracingContext& Editor::GetRaytracingSettings()const
	{
		return context_.viewportContext_.raytracing_;
	}
}
