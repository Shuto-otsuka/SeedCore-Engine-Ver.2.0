#include <Editor/Editor/Editor.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimatorControllerState.h>
#include <GraphicsEngine/Model/Material/MaterialState.h>
#include <GraphicsEngine/Model/Skeleton/SkeletonState.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

namespace SeedCore
{
	Editor::Editor(EditorContext& context) :context_(context), imguiTexture_(context_)
	{
		hierarchyPanel_ = MakePtr<HierarchyPanel>(context_, imguiTexture_);
		inspectorPanel_ = MakePtr<InspectorPanel>(context_, imguiTexture_);
		toolPanel_ = MakePtr<ToolPanel>(context_, imguiTexture_);
		editorWindowPanel_ = MakePtr<EditorWindowPanel>(context_, imguiTexture_);
		gameWindowPanel_ = MakePtr<GameWindowPanel>(*context_.cameraContext_.cameraSystem_, imguiTexture_);
		canvasViewPanel_ = MakePtr<CanvasViewPanel>(context_, imguiTexture_);
		contentsDrawerPanel_ = MakePtr<ContentsDrawerPanel>(context_, imguiTexture_);
		controlPanel_ = MakePtr<ControlPanel>(context_, imguiTexture_);
		menuBarPanel_ = MakePtr<MenuBarPanel>(context_);
		shortCutKeyPanel_ = MakePtr<ShortCutKeyPanel>(context_);
		specMemoPanel_ = MakePtr<SpecMemoPanel>(context_);
		todoListPanel_ = MakePtr<TodoListPanel>(context_, imguiTexture_);
		versionPanel_ = MakePtr<VersionPanel>(context_);
		configPanel_ = MakePtr<ConfigPanel>(context_, imguiTexture_);
		layerSettingsPanel_ = MakePtr<LayerSettingsPanel>(context_);
		animatorControllerPanel_ = MakePtr<AnimatorControllerPanel>(context_, imguiTexture_);
		timelinePanel_ = MakePtr<TimelinePanel>(context_);
		skeletonControllerPanel_ = MakePtr<SkeletonControllerPanel>(context_);
		materialViewerPanel_ = MakePtr<MaterialViewerPanel>(context_);
		modelTransformPanel_ = MakePtr<ModelTransformPanel>(context_);

		context_.panelContext_.animatorControllerPanel_ = &*animatorControllerPanel_;
		context_.panelContext_.timelinePanel_ = &*timelinePanel_;
		context_.panelContext_.layerSettingsPanel_ = &*layerSettingsPanel_;
		context_.panelContext_.materialViewerPanel_ = &*materialViewerPanel_;
		context_.panelContext_.skeletonControllerPanel_ = &*skeletonControllerPanel_;

		SC_LOG_NOTICE("エディターの初期化が完了しました");
	}

	void Editor::PruneDeadSelection()
	{
		World* world = context_.worldContext_.world_;
		if (!world)
		{
			return;
		}

		SelectionContext& selection = context_.selectionContext_;

		SeedCore::erase_if(selection.selectedActors_, [](Actor actor) { return !actor; });

		if (!selection.selectedActor_)
		{
			selection.selectedActor_ = selection.selectedActors_.empty() ? Actor() : selection.selectedActors_.back();
			selection.selectedEntity_ = selection.selectedActor_ ? selection.selectedActor_.GetEntity() : Entity::Null();
		}
	}

	Float Editor::DrawToolbar()
	{
		PruneDeadSelection();

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
			Animator* animator = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_.GetComponent<Animator>()) : nullptr;
			animatorControllerPanel_->Open(animator);
		}
		if (AnimatorControllerRequest::requested_)
		{
			AnimatorControllerRequest::requested_ = false;
			Animator* animator = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_.GetComponent<Animator>()) : nullptr;
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
		if (menuBarPanel_->ConsumeSkeletonControllerRequest())
		{
			skeletonControllerPanel_->Open();
		}
		if (SkeletonControllerRequest::requested_)
		{
			SkeletonControllerRequest::requested_ = false;
			skeletonControllerPanel_->Open();
		}
		if (menuBarPanel_->ConsumeMaterialViewerRequest())
		{
			materialViewerPanel_->Open();
		}
		if (MaterialPanelRequest::openRequested_)
		{
			MaterialPanelRequest::openRequested_ = false;
			materialViewerPanel_->Open();
		}
		if (menuBarPanel_->ConsumeModelTransformRequest())
		{
			modelTransformPanel_->Open();
		}
		if (context_.modelTransformPreviewContext_.requestedAssetId_ != 0)
		{
			modelTransformPanel_->Open(context_.modelTransformPreviewContext_.requestedAssetId_);
			context_.modelTransformPreviewContext_.requestedAssetId_ = 0;
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

	void Editor::Draw(D3D12_GPU_DESCRIPTOR_HANDLE editorFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE gameFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE canvasFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE timelinePreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE modelTransformPreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE materialPreviewFrameBufferHandle, D3D12_GPU_DESCRIPTOR_HANDLE skeletonControllerPreviewFrameBufferHandle, const GpuProfiler& gpuProfiler)
	{
		timelinePanel_->SetPreviewHandle(timelinePreviewFrameBufferHandle);
		modelTransformPanel_->SetPreviewHandle(modelTransformPreviewFrameBufferHandle);
		materialViewerPanel_->SetPreviewHandle(materialPreviewFrameBufferHandle);
		skeletonControllerPanel_->SetPreviewHandle(skeletonControllerPreviewFrameBufferHandle);

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
		skeletonControllerPanel_->Draw();
		materialViewerPanel_->Draw();
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

			/// [EN] Unreal-style "frame selected": Ctrl+F slides a viewport
			///      camera to the selected actor. Kept in sync with
			///      HierarchyPanel's double-click, which does the same - a
			///      Sprite-view Image/Text/Movie animates the CanvasView
			///      camera and pulls that panel forward (it is drawn only in
			///      the 2D canvas, offset +100000); a skinned actor (has an
			///      Animator) pans to the pivot at the current distance
			///      because its bind-pose Bounds is unreliable for a dolly
			///      fit; anything else dollies to fit its world-space Bounds
			///      centre. See HierarchyPanel::Draw for the full rationale.
			/// [JP] Unreal 風の「選択対象にフレーム」: Ctrl+F で選択中の
			///      アクターへビューポートのカメラがスライドする。
			///      HierarchyPanel のダブルクリックと同期 - 表示形式が
			///      Sprite の Image/Text/Movie は CanvasView のカメラを
			///      アニメーションで寄せ、そのパネルを前面に出す（2D
			///      キャンバスにしか描かれず +100000 ずれる）。スキンアクター
			///      （Animator を持つ）はバインドポーズ Bounds がドリー
			///      フィットに使えないため、現在の距離のままピボットへパン。
			///      それ以外はワールド空間 Bounds 中心へドリーフィット。
			///      詳細は HierarchyPanel::Draw のコメント参照。
			if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_F) && context_.selectionContext_.selectedActor_)
			{
				Actor selectedActor = context_.selectionContext_.selectedActor_;
				const Matrix& worldMatrix = selectedActor.GetWorldMatrix();

				const Image* image = selectedActor.GetComponent<Image>();
				const Text* text = selectedActor.GetComponent<Text>();
				const Movie* movie = selectedActor.GetComponent<Movie>();
				Bool isCanvasActor = (image && image->viewType_ == Image::ViewType::Sprite) || (text && text->viewType_ == Text::ViewType::Sprite) || (movie && movie->displayMode_ == Movie::DisplayMode::Sprite);

				if (isCanvasActor && context_.cameraContext_.canvasCamera_)
				{
					Vector3 canvasTarget = Vector3(100000.0f + worldMatrix._41, 100000.0f + (ScResolution::SC_HD.Height - worldMatrix._42), 100000.0f);
					context_.cameraContext_.canvasCamera_->FocusOn(canvasTarget);

					ImGui::SetWindowFocus("キャンバスビュー");
				}
				else if (context_.cameraContext_.editorCamera_)
				{
					Bool skinned = selectedActor.GetComponent<Animator>() != nullptr;

					Float radius = 0.0f;
					Vector3 target = Vector3(worldMatrix._41, worldMatrix._42, worldMatrix._43);

					const Bounds* bounds = selectedActor.GetComponent<Bounds>();
					if (bounds && !skinned)
					{
						Float worldScale = Max(Max(Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13).Length(), Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23).Length()), Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33).Length());
						radius = bounds->extent_.Length() * worldScale;
						target = Vector3::Transform(bounds->center_, worldMatrix);
					}

					context_.cameraContext_.editorCamera_->FocusOn(target, radius);

					ImGui::SetWindowFocus("エディタービュー");
				}
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
