#include <Editor/Editor/Panel/GuizmoPanel3D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
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

		if (!context_.selectionContext_.selectedActor_)
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

			Matrix worldMatrix = context_.selectionContext_.selectedActor_->GetWorldMatrix();

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

			Entity entity = context_.selectionContext_.selectedActor_->GetEntity();

			Bool isDragging = ImGuizmo::IsUsing();

			if (isDragging && !wasDragging_)
			{
				dragEntity_ = entity;

				Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
				Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));
				Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));

				if (positionData)
				{
					dragStartPosition_ = Vector3(positionData[0], positionData[1], positionData[2]);
				}
				if (rotationData)
				{
					dragStartRotation_ = Vector3(rotationData[0], rotationData[1], rotationData[2]);
				}
				if (scaleData)
				{
					dragStartScale_ = Vector3(scaleData[0], scaleData[1], scaleData[2]);
				}
			}

			Move(view, projection, worldMatrix, context_.viewportContext_.guizmo_.guizmoOperation_, ctrlPressed ? snapValues : nullptr);

			if (!isDragging && wasDragging_)
			{
				Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(dragEntity_, positionID));
				Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(dragEntity_, rotationID));
				Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(dragEntity_, scaleID));

				if (positionData)
				{
					Vector3 newPosition(positionData[0], positionData[1], positionData[2]);
					if (newPosition != dragStartPosition_)
					{
						context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, dragEntity_, positionID, 0, dragStartPosition_, newPosition));
					}
				}
				if (rotationData)
				{
					Vector3 newRotation(rotationData[0], rotationData[1], rotationData[2]);
					if (newRotation != dragStartRotation_)
					{
						context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, dragEntity_, rotationID, 0, dragStartRotation_, newRotation));
					}
				}
				if (scaleData)
				{
					Vector3 newScale(scaleData[0], scaleData[1], scaleData[2]);
					if (newScale != dragStartScale_)
					{
						context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, dragEntity_, scaleID, 0, dragStartScale_, newScale));
					}
				}
			}

			wasDragging_ = isDragging;
		}
	}

	void GuizmoPanel3D::Move(Matrix& view, Matrix& projection, Matrix& world, ImGuizmo::OPERATION operation, const Float* snap)
	{
		if (ImGuizmo::Manipulate(&view._11, &projection._11, operation, currentMode_, &world._11, nullptr, snap))
		{
			Actor* parentActor = context_.selectionContext_.selectedActor_->GetParent();
			Matrix localMatrix = (parentActor) ? world * parentActor->GetWorldMatrix().Invert() : world;

			Vector3 position, scale;
			Quaternion rotation;
			if (localMatrix.Decompose(scale, rotation, position))
			{
				Entity entity = context_.selectionContext_.selectedActor_->GetEntity();

				static const String positionString("Position");
				static const String rotationString("Rotation");
				static const String scaleString("Scale");

				ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
				ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
				ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

				Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
				Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));
				Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));

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
				if (scaleData)
				{
					scaleData[0] = scale.x;
					scaleData[1] = scale.y;
					scaleData[2] = scale.z;
				}
			}
		}
	}
}
