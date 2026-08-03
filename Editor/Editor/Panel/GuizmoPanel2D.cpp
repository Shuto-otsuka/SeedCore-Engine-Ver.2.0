#include <Editor/Editor/Panel/GuizmoPanel2D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

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
				context_.guizmo_.showGuizmo_ = false;
				context_.guizmo_.guizmoOperation_ = (ImGuizmo::OPERATION)0;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				context_.guizmo_.showGuizmo_ = true;
				context_.guizmo_.guizmoOperation_ = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				context_.guizmo_.showGuizmo_ = true;
				context_.guizmo_.guizmoOperation_ = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R))
			{
				context_.guizmo_.showGuizmo_ = true;
				context_.guizmo_.guizmoOperation_ = ImGuizmo::SCALE;
			}
		}

		if (!context_.selectedActor_)
		{
			return;
		}

		if (context_.guizmo_.showGuizmo_)
		{
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(position.x, position.y, size.x, size.y);
			ImGuizmo::SetOrthographic(true);
			ImGuizmo::AllowAxisFlip(false);
			ImGuizmo::SetGizmoSizeClipSpace(0.2f);

			Float renderWidth = ScResolution::SC_HD.Width;
			Float renderHeight = ScResolution::SC_HD.Height;

			Matrix view = Matrix::Identity;
			Matrix projection = Matrix::CreateOrthographicOffCenter(0.0f, renderWidth, renderHeight, 0.0f, 0.01f, 100.0f);

			Entity entity = context_.selectedActor_->GetEntity();
			static const String positionString("Position");
			static const String rotationString("Rotation");
			static const String scaleString("Scale");
			Float* position2D = static_cast<Float*>(context_.world_->GetComponent(entity, ComponentRegistry::GetComponentID(positionString)));
			Float* rotation2D = static_cast<Float*>(context_.world_->GetComponent(entity, ComponentRegistry::GetComponentID(rotationString)));
			Float* scale2D = static_cast<Float*>(context_.world_->GetComponent(entity, ComponentRegistry::GetComponentID(scaleString)));

			Vector3 cachePosition = position2D ? Vector3(position2D[0], position2D[1], 0.0f) : Vector3::Zero;
			Float cacheRotation = rotation2D ? ToRadians(rotation2D[0]) : 0.0f;
			Vector3 cacheScale = scale2D ? Vector3(scale2D[0], scale2D[1], 1.0f) : Vector3::One;

			Matrix worldMatrix = Matrix::CreateScale(cacheScale) * Matrix::CreateRotationZ(cacheRotation) * Matrix::CreateTranslation(cachePosition);

			Float snapValues[3] = {0.0f, 0.0f, 0.0f};

			Bool snapCtrlPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
			if (snapCtrlPressed)
			{
				if (context_.guizmo_.guizmoOperation_ == ImGuizmo::TRANSLATE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.guizmo_.translateSnap_;
				}
				else if (context_.guizmo_.guizmoOperation_ == ImGuizmo::ROTATE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.guizmo_.rotateSnap_;
				}
				else if (context_.guizmo_.guizmoOperation_ == ImGuizmo::SCALE)
				{
					snapValues[0] = snapValues[1] = snapValues[2] = context_.guizmo_.scaleSnap_;
				}
			}

			ImGuizmo::OPERATION op = context_.guizmo_.guizmoOperation_;
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

			Move(view, projection, worldMatrix, op, snapCtrlPressed ? snapValues : nullptr);
		}
	}

	void GuizmoPanel2D::Move(Matrix& view, Matrix& projection, Matrix& world, ImGuizmo::OPERATION operation, const Float* snap)
	{
		if (ImGuizmo::Manipulate(&view._11, &projection._11, operation, currentMode_, &world._11, nullptr, snap))
		{
			Actor* parentActor = context_.selectedActor_->GetParent();
			Matrix localMatrix = (parentActor) ? world * parentActor->GetWorldMatrix().Invert() : world;

			Vector3 position, scale;
			Quaternion rotation;
			if (localMatrix.Decompose(scale, rotation, position))
			{
				Entity entity = context_.selectedActor_->GetEntity();

				static const String positionString("Position");
				static const String rotationString("Rotation");
				static const String scaleString("Scale");

				ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
				ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
				ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

				Float* positionData = static_cast<Float*>(context_.world_->GetComponent(entity, positionID));
				Float* rotationData = static_cast<Float*>(context_.world_->GetComponent(entity, rotationID));
				Float* scaleData = static_cast<Float*>(context_.world_->GetComponent(entity, scaleID));

				if (positionData)
				{
					positionData[0] = position.x;
					positionData[1] = position.y;
					positionData[2] = position.z;
				}
				if (rotationData)
				{
					Vector3 euler = rotation.ToEuler();
					rotationData[0] = ToDegrees(euler.z);
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
