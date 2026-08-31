#include <Editor/Editor/Panel/GuizmoPanel2D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
#include <FoundationEngine/ECS/CompoundCommand.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>

namespace SeedCore
{
	GuizmoPanel2D::GuizmoPanel2D(EditorContext& context) :context_(context)
	{
		/// No Code
	}

	void GuizmoPanel2D::Draw(const Vector2& position, const Vector2& size)
	{
		Bool ctrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl, false) || ImGui::IsKeyDown(ImGuiKey_RightCtrl, false);

		if (ctrlPressed)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = false;
				context_.viewportContext_.guizmo_.guizmoOperation_ = (ImGuizmo::OPERATION)0;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::SCALE;
			}
		}

		const DynamicArray<Actor>& selectedActors = context_.selectionContext_.selectedActors_;
		if (selectedActors.empty())
		{
			return;
		}

		if (!context_.cameraContext_.canvasCamera_)
		{
			return;
		}

		if (context_.viewportContext_.guizmo_.showGuizmo_)
		{
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(position.x, position.y, size.x, size.y);
			ImGuizmo::SetOrthographic(true);
			ImGuizmo::AllowAxisFlip(false);
			ImGuizmo::SetGizmoSizeClipSpace(0.2f);

			Float renderWidth = ScResolution::SC_HD.Width;
			Float renderHeight = ScResolution::SC_HD.Height;

			Vector3 canvasFocus = context_.cameraContext_.canvasCamera_->Focus();
			Float panX = canvasFocus.x - (100000.0f + renderWidth * 0.5f);
			Float panY = (100000.0f + renderHeight * 0.5f) - canvasFocus.y;

			Matrix view = Matrix::Identity;
			Matrix projection = Matrix::CreateOrthographicOffCenter(panX, renderWidth + panX, renderHeight + panY, panY, 0.01f, 100.0f);

			static const String positionString("Position");
			static const String rotationString("Rotation");
			static const String scaleString("Scale");
			ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
			ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
			ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

			Bool isDragging = ImGuizmo::IsUsing();

			/// [EN] With one actor selected the gizmo matrix is that actor's own world matrix; with several selected it sits at the unrotated average of their world positions (2D needs no rotation averaging). Only rebuilt from the live selection while not dragging - see pivotMatrix_'s header comment.
			/// [JP] 単一選択時はギズモ行列をその actor 自身のワールド行列にする。複数選択時はそれらのワールド座標の(無回転の)平均位置に置く(2D では回転の平均は不要)。ドラッグ中でない間だけ現在の選択から作り直す — 理由は pivotMatrix_ のヘッダコメント参照。
			if (!isDragging)
			{
				if (selectedActors.size() == 1)
				{
					pivotMatrix_ = selectedActors[0].GetWorldMatrix();
				}
				else
				{
					Vector3 averagePosition = Vector3::Zero;
					for (Actor actor : selectedActors)
					{
						averagePosition += actor.GetWorldMatrix().Translation();
					}
					averagePosition /= static_cast<Float>(selectedActors.size());
					pivotMatrix_ = Matrix::CreateTranslation(averagePosition);
				}
			}

			/// [EN] Capture the pre-drag world matrix and Position/Rotation/Scale of every selected actor on the frame the drag starts, so drag-end can build one command per changed channel (Move() writes the components directly every frame in between).
			/// [JP] ドラッグが始まるフレームで、選択中の全 actor のドラッグ前ワールド行列と Position/Rotation/Scale を捕捉し、終了時に変化したチャンネルごとにコマンドを組み立てられるようにする(その間 Move() が毎フレーム直接コンポーネントへ書き込む)。
			if (isDragging && !wasDragging_)
			{
				dragEntities_.clear();
				dragStartWorldMatrices_.clear();
				dragStartPositions_.clear();
				dragStartRotations_.clear();
				dragStartScales_.clear();
				dragStartPivotMatrix_ = pivotMatrix_;

				for (Actor actor : selectedActors)
				{
					Entity entity = actor.GetEntity();
					dragEntities_.push_back(entity);
					dragStartWorldMatrices_.push_back(actor.GetWorldMatrix());

					Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
					Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));
					Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));

					dragStartPositions_.push_back(positionData ? Vector3(positionData[0], positionData[1], positionData[2]) : Vector3::Zero);
					dragStartRotations_.push_back(rotationData ? Vector3(rotationData[0], rotationData[1], rotationData[2]) : Vector3::Zero);
					dragStartScales_.push_back(scaleData ? Vector3(scaleData[0], scaleData[1], scaleData[2]) : Vector3::One);
				}
			}

			Float snapValues[3] = {0.0f, 0.0f, 0.0f};

			Bool snapCtrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
			if (snapCtrlPressed)
			{
				if (context_.viewportContext_.guizmo_.guizmoOperation_ == ImGuizmo::TRANSLATE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.viewportContext_.guizmo_.translateSnap_;
				}
				else if (context_.viewportContext_.guizmo_.guizmoOperation_ == ImGuizmo::ROTATE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.viewportContext_.guizmo_.rotateSnap_;
				}
				else if (context_.viewportContext_.guizmo_.guizmoOperation_ == ImGuizmo::SCALE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.viewportContext_.guizmo_.scaleSnap_;
				}
			}

			ImGuizmo::OPERATION op = context_.viewportContext_.guizmo_.guizmoOperation_;
			if (op == ImGuizmo::TRANSLATE)
			{
				op = ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
			}
			else if (op == ImGuizmo::ROTATE)
			{
				op = ImGuizmo::ROTATE_Z;
			}
			else if (op == ImGuizmo::SCALE)
			{
				op = ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
			}

			Move(view, projection, pivotMatrix_, op, snapCtrlPressed ? snapValues : nullptr);

			/// [EN] Drag just ended: for every dragged actor, diff each channel against its captured start value and add one command per channel that actually moved to a single CompoundCommand, so Ctrl+Z reverts the whole multi-select drag at once.
			/// [JP] ドラッグが今終わった: ドラッグ対象の各 actor について、各チャンネルを捕捉した開始値と比較し、実際に動いたチャンネルごとに1つコマンドを1つの CompoundCommand へ足す。こうして Ctrl+Z が複数選択ドラッグ全体を一度に取り消す。
			if (!isDragging && wasDragging_)
			{
				ResourcePtr<CompoundCommand> dragCommand = MakePtr<CompoundCommand>();

				for (Size index = 0; index < dragEntities_.size(); ++index)
				{
					Entity entity = dragEntities_[index];

					Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
					Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));
					Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));

					if (positionData)
					{
						Vector3 newPosition(positionData[0], positionData[1], positionData[2]);
						if (newPosition != dragStartPositions_[index])
						{
							dragCommand->Add(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, entity, positionID, 0, dragStartPositions_[index], newPosition));
						}
					}
					if (rotationData)
					{
						Vector3 newRotation(rotationData[0], rotationData[1], rotationData[2]);
						if (newRotation != dragStartRotations_[index])
						{
							dragCommand->Add(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, entity, rotationID, 0, dragStartRotations_[index], newRotation));
						}
					}
					if (scaleData)
					{
						Vector3 newScale(scaleData[0], scaleData[1], scaleData[2]);
						if (newScale != dragStartScales_[index])
						{
							dragCommand->Add(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, entity, scaleID, 0, dragStartScales_[index], newScale));
						}
					}
				}

				if (!dragCommand->Empty())
				{
					context_.sceneContext_.history_.Push(std::move(dragCommand));
				}
			}

			wasDragging_ = isDragging;
		}
	}

	void GuizmoPanel2D::Move(Matrix& view, Matrix& projection, Matrix& pivot, ImGuizmo::OPERATION operation, const Float* snap)
	{
		if (ImGuizmo::Manipulate(&view._11, &projection._11, operation, currentMode_, &pivot._11, nullptr, snap))
		{
			/// [EN] The world-space delta the pivot underwent this frame relative to drag-start, applied identically to every dragged actor's own drag-start world matrix (same scheme as GuizmoPanel3D).
			/// [JP] このフレームでピボットがドラッグ開始時から受けたワールド空間のデルタ変換。ドラッグ対象の各 actor のドラッグ開始時ワールド行列に同じデルタを適用する(GuizmoPanel3D と同じ方式)。
			Matrix pivotDelta = dragStartPivotMatrix_.Invert() * pivot;

			static const String positionString("Position");
			static const String rotationString("Rotation");
			static const String scaleString("Scale");

			ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
			ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
			ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

			/// [EN] Scale is applied per-actor around each actor's own origin (multiply its own Scale by the pivot's scale delta), not by recomposing the whole world matrix around the shared pivot - otherwise a multi-selection would spread apart / draw together as it scales.
			/// [JP] Scale は各 actor 自身の原点を中心に個別適用する(自分の Scale にピボットのスケールデルタを掛ける)。共有ピボット中心にワールド行列全体を組み直すと、複数選択が拡縮に伴って離れる/寄るため。
			if (operation & ImGuizmo::SCALE)
			{
				Vector3 deltaScale;
				Vector3 deltaTranslation;
				Quaternion deltaRotation;
				pivotDelta.Decompose(deltaScale, deltaRotation, deltaTranslation);

				for (Size index = 0; index < dragEntities_.size(); ++index)
				{
					Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(dragEntities_[index], scaleID));
					if (scaleData)
					{
						scaleData[0] = dragStartScales_[index].x * deltaScale.x;
						scaleData[1] = dragStartScales_[index].y * deltaScale.y;
					}
				}

				return;
			}

			for (Size index = 0; index < dragEntities_.size(); ++index)
			{
				Entity entity = dragEntities_[index];
				Actor actor = context_.worldContext_.world_->GetActor(entity);

				Matrix newWorldMatrix = dragStartWorldMatrices_[index] * pivotDelta;

				Actor parentActor = actor ? actor.GetParent() : Actor();
				Matrix localMatrix = (parentActor) ? newWorldMatrix * parentActor.GetWorldMatrix().Invert() : newWorldMatrix;

				Vector3 position, scale;
				Quaternion rotation;
				if (localMatrix.Decompose(scale, rotation, position))
				{
					Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
					Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));

					if (positionData && (operation & ImGuizmo::TRANSLATE))
					{
						positionData[0] = position.x;
						positionData[1] = position.y;
					}
					if (rotationData && (operation & ImGuizmo::ROTATE))
					{
						Vector3 euler = rotation.ToEuler();
						rotationData[0] = ToDegrees(euler.z);
					}
				}
			}
		}
	}
}
