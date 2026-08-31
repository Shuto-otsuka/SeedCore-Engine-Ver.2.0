#include <Editor/Editor/Panel/GuizmoPanel3D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
#include <FoundationEngine/ECS/CompoundCommand.h>
#include <GraphicsEngine/Camera/EditorCamera.h>

namespace SeedCore
{
	GuizmoPanel3D::GuizmoPanel3D(EditorContext& context) :context_(context)
	{
		/// No Code
	}

	void GuizmoPanel3D::Draw(const Vector2& position, const Vector2& size)
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

		if (context_.viewportContext_.guizmo_.showGuizmo_)
		{
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(position.x, position.y, size.x, size.y);
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::AllowAxisFlip(false);
			ImGuizmo::SetGizmoSizeClipSpace(0.2f);

			Matrix view = context_.cameraContext_.editorCamera_->View();
			Matrix projection = Matrix::CreatePerspectiveFieldOfView(ToRadians(context_.cameraContext_.editorCamera_->Fov()), context_.cameraContext_.editorCamera_->AspectRatio(), context_.cameraContext_.editorCamera_->Near(), context_.cameraContext_.editorCamera_->Far());

			Bool isDragging = ImGuizmo::IsUsing();

			/// [EN] With a single actor selected, the gizmo matrix is that actor's own
			///      world matrix (rotate/scale pivot on the actor itself). With multiple
			///      actors selected, the gizmo instead sits at the unrotated average of
			///      their world positions (Unreal-style multi-select pivot) - every
			///      selected actor is then moved by whatever delta transform the pivot
			///      itself underwent (see Move()). Only rebuilt from the live selection
			///      while not dragging - see pivotMatrix_'s header comment for why.
			/// [JP] 単一選択時はギズモ行列をその actor 自身のワールド行列にする
			///      （回転/スケールは actor 自身を中心に行う）。複数選択時は代わりに、
			///      それらのワールド座標の（無回転の）平均位置にギズモを置く
			///      （Unreal 風の複数選択ピボット） - 選択中の各 actor は、ピボット
			///      自身が受けたデルタ変換で移動する（Move() 参照）。ドラッグ中でない
			///      間だけ現在の選択から作り直す — 理由は pivotMatrix_ のヘッダの
			///      コメント参照。
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

			Float snapValues[3] = {0.0f, 0.0f, 0.0f};

			Bool ctrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
			if (ctrlPressed)
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

			static const String positionString("Position");
			static const String rotationString("Rotation");
			static const String scaleString("Scale");

			ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
			ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
			ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

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

			Move(view, projection, pivotMatrix_, context_.viewportContext_.guizmo_.guizmoOperation_, ctrlPressed ? snapValues : nullptr);

			if (!isDragging && wasDragging_)
			{
				/// [EN] One drag over a multi-selection produces up to one edit per actor per changed channel - group them into a single CompoundCommand so Ctrl+Z reverts the whole drag at once instead of one actor's one channel.
				/// [JP] 複数選択を1回ドラッグすると、actor ごと・変化したチャンネルごとに最大1編集が出る - それらを1つの CompoundCommand にまとめ、Ctrl+Z がドラッグ全体を一度に取り消せるようにする(1 actor の1チャンネルだけではなく)。
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

	void GuizmoPanel3D::Move(Matrix& view, Matrix& projection, Matrix& pivot, ImGuizmo::OPERATION operation, const Float* snap)
	{
		if (ImGuizmo::Manipulate(&view._11, &projection._11, operation, currentMode_, &pivot._11, nullptr, snap))
		{
			/// [EN] The world-space delta the pivot underwent this frame relative to
			///      drag-start, applied identically to every dragged actor's own
			///      drag-start world matrix (see the header comment for the math).
			/// [JP] このフレームでピボットがドラッグ開始時から受けたワールド空間の
			///      デルタ変換。ドラッグ対象の各actorのドラッグ開始時ワールド行列に
			///      同じデルタを適用する（数式についてはヘッダのコメント参照）。
			Matrix pivotDelta = dragStartPivotMatrix_.Invert() * pivot;

			static const String positionString("Position");
			static const String rotationString("Rotation");
			static const String scaleString("Scale");

			ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
			ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
			ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

			/// [EN] Scale is handled separately from translate/rotate: instead of
			///      recomposing each actor's whole world matrix around the shared
			///      pivot (which would also push actors apart from/together toward
			///      the pivot as they scale), each actor's own Scale is multiplied by
			///      the pivot's scale delta in place - Position/Rotation are left
			///      untouched, so every selected actor scales individually around its
			///      own origin instead of the group scaling as a block.
			/// [JP] Scale は Translate/Rotate とは別扱いにする: 各 actor のワールド
			///      行列全体を共有ピボット中心に組み直す（スケールに伴って actor が
			///      ピボットから離れる/近づく）のではなく、ピボットのスケール
			///      デルタをその場で各 actor 自身の Scale に掛け合わせる。
			///      Position/Rotation はそのままにするので、グループ全体が1つの
			///      ブロックとして拡縮されるのではなく、選択中の各 actor がそれぞれ
			///      自分自身の原点を中心に個別に拡縮される。
			if (operation == ImGuizmo::SCALE)
			{
				Vector3 deltaScale;
				Vector3 deltaTranslation;
				Quaternion deltaRotation;
				pivotDelta.Decompose(deltaScale, deltaRotation, deltaTranslation);

				for (Size index = 0; index < dragEntities_.size(); ++index)
				{
					Entity entity = dragEntities_[index];
					Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));
					if (scaleData)
					{
						scaleData[0] = dragStartScales_[index].x * deltaScale.x;
						scaleData[1] = dragStartScales_[index].y * deltaScale.y;
						scaleData[2] = dragStartScales_[index].z * deltaScale.z;
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

					if (positionData)
					{
						positionData[0] = position.x;
						positionData[1] = position.y;
						positionData[2] = position.z;
					}
					if (rotationData)
					{
						Vector3 euler = rotation.ToEuler();
						rotationData[0] = ToDegrees(euler.x);
						rotationData[1] = ToDegrees(euler.y);
						rotationData[2] = ToDegrees(euler.z);
					}
				}
			}
		}
	}
}
