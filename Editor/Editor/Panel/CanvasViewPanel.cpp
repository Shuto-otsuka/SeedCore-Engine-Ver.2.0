#include <Editor/Editor/Panel/CanvasViewPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	CanvasViewPanel::CanvasViewPanel(EditorContext& context, ImGuiTexture& imguiTexture) :context_(context), imguiTexture_(imguiTexture), guizmoPanel_(context)
	{
		/// No Code
	}

	void CanvasViewPanel::DrawGizmoMenu()
	{
		ImVec2 iconSize(24, 24);
		ImVec4 transparent(0, 0, 0, 0);
		ImVec4 activeColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		ImVec4 hoverColor = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);

		auto& op = context_.viewportContext_.guizmo_.guizmoOperation_;

		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

		ImGui::PushStyleColor(ImGuiCol_Button, !context_.viewportContext_.guizmo_.showGuizmo_ ? activeColor : transparent);
		if (ImGui::ImageButton("##NonSelected", imguiTexture_.Icon(IconType::NonSelected), iconSize))
		{
			context_.viewportContext_.guizmo_.showGuizmo_ = !context_.viewportContext_.guizmo_.showGuizmo_;
			op = (ImGuizmo::OPERATION)0;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, (op == ImGuizmo::TRANSLATE) ? activeColor : transparent);
		if (ImGui::ImageButton("##Translate", imguiTexture_.Icon(IconType::Translate), iconSize))
		{
			context_.viewportContext_.guizmo_.showGuizmo_ = true;
			op = ImGuizmo::TRANSLATE;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, (op == ImGuizmo::ROTATE) ? activeColor : transparent);
		if (ImGui::ImageButton("##Rotate", imguiTexture_.Icon(IconType::Rotate), iconSize))
		{
			context_.viewportContext_.guizmo_.showGuizmo_ = true;
			op = ImGuizmo::ROTATE;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, (op == ImGuizmo::SCALE) ? activeColor : transparent);
		if (ImGui::ImageButton("##Scale", imguiTexture_.Icon(IconType::Scale), iconSize))
		{
			context_.viewportContext_.guizmo_.showGuizmo_ = true;
			op = ImGuizmo::SCALE;
		}
		ImGui::PopStyleColor();

		ImGui::PopStyleColor(2);

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, transparent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		if (ImGui::ImageButton("##GizmoIcon2D", imguiTexture_.Icon(IconType::Guizmo), iconSize))
		{
			ImGui::OpenPopup("##GizmoSettings2D");
		}
		ImGui::PopStyleColor(3);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		if (ImGui::BeginPopup("##GizmoSettings2D"))
		{
			ImGui::SeparatorText("スナップ");

			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat("移動", &context_.viewportContext_.guizmo_.translateSnap_, 0.1f, 0.01f, 100.0f, "%.2f");

			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat("回転", &context_.viewportContext_.guizmo_.rotateSnap_, 0.5f, 0.1f, 90.0f, "%.0f\xc2\xb0");

			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat("拡大縮小", &context_.viewportContext_.guizmo_.scaleSnap_, 0.05f, 0.01f, 10.0f, "%.2f");

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(3);
	}

	void CanvasViewPanel::Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		if (ImGui::Begin("キャンバスビュー"))
		{
			DrawGizmoMenu();
			ImGui::Separator();

			ImVec2 regionSize = ImGui::GetContentRegionAvail();

			if (regionSize.x > 0.0f && regionSize.y > 0.0f)
			{
				constexpr Float aspectRatio = 16.0f / 9.0f;
				Float imageWidth = regionSize.x;
				Float imageHeight = regionSize.x / aspectRatio;

				if (imageHeight > regionSize.y)
				{
					imageHeight = regionSize.y;
					imageWidth = regionSize.y * aspectRatio;
				}

				imageWidth = ImFloor(imageWidth);
				imageHeight = ImFloor(imageHeight);
				Float offsetX = ImFloor((regionSize.x - imageWidth) * 0.5f);
				Float offsetY = ImFloor((regionSize.y - imageHeight) * 0.5f);

				ImVec2 cursorPosition = ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY);
				ImGui::SetCursorPos(cursorPosition);

				ImVec2 screenPosition = ImGui::GetCursorScreenPos();
				screenPosition.x = IM_ROUND(screenPosition.x);
				screenPosition.y = IM_ROUND(screenPosition.y);
				ImGui::SetCursorScreenPos(screenPosition);

				ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
				ImGui::Image(ImTextureID(frameBufferHandle.ptr), ImVec2(imageWidth, imageHeight));
				ImGui::PopStyleVar();

				Bool canvasHovered = ImGui::IsItemHovered();
				Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed);

				if (canvasHovered && panHeld && !isPanning_)
				{
					isPanning_ = true;
					isResettingView_ = false;
					InputSystem::BeginMouseCapture();
				}

				if (isPanning_ && panHeld && context_.cameraContext_.canvasCamera_)
				{
					CanvasCamera& canvasCamera = *context_.cameraContext_.canvasCamera_;

					Float worldPerPixel = canvasCamera.VisibleHeight() / imageHeight;
					Float deltaX = -InputSystem::MouseDeltaX() * worldPerPixel;
					Float deltaY = InputSystem::MouseDeltaY() * worldPerPixel;

					Vector3 eye = canvasCamera.Eye();
					Vector3 focus = canvasCamera.Focus();
					eye.x += deltaX;
					eye.y += deltaY;
					focus.x += deltaX;
					focus.y += deltaY;
					canvasCamera.Eye(eye);
					canvasCamera.Focus(focus);
				}

				if (isPanning_ && !panHeld)
				{
					isPanning_ = false;
					InputSystem::EndMouseCapture();
				}

				if (canvasHovered && InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::TriggerMode::RISING_EDGE))
				{
					isResettingView_ = true;
				}

				if (isResettingView_ && context_.cameraContext_.canvasCamera_)
				{
					CanvasCamera& canvasCamera = *context_.cameraContext_.canvasCamera_;

					Vector3 targetFocus = Vector3(100000.0f + ScResolution::SC_HD.Width * 0.5f, 100000.0f + ScResolution::SC_HD.Height * 0.5f, 100000.0f);
					Vector3 targetEye = Vector3(100000.0f + ScResolution::SC_HD.Width * 0.5f, 100000.0f + ScResolution::SC_HD.Height * 0.5f, 99990.0f);

					Float lerpAmount = Clamp(ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f);
					Vector3 newFocus = Vector3::Lerp(canvasCamera.Focus(), targetFocus, lerpAmount);
					Vector3 newEye = Vector3::Lerp(canvasCamera.Eye(), targetEye, lerpAmount);
					canvasCamera.Focus(newFocus);
					canvasCamera.Eye(newEye);

					if (Vector3::DistanceSquared(newFocus, targetFocus) < 0.01f)
					{
						canvasCamera.Focus(targetFocus);
						canvasCamera.Eye(targetEye);
						isResettingView_ = false;
					}
				}

				if (context_.cameraContext_.canvasCamera_)
				{
					CanvasCamera& canvasCamera = *context_.cameraContext_.canvasCamera_;

					Float worldPerPixel = canvasCamera.VisibleHeight() / imageHeight;
					Vector3 focus = canvasCamera.Focus();
					Float centerScreenX = screenPosition.x + imageWidth * 0.5f;
					Float centerScreenY = screenPosition.y + imageHeight * 0.5f;

					constexpr Float gridSpacing = 100.0f;
					Float worldMinX = focus.x - imageWidth * 0.5f * worldPerPixel;
					Float worldMaxX = focus.x + imageWidth * 0.5f * worldPerPixel;
					Float worldMinY = focus.y - imageHeight * 0.5f * worldPerPixel;
					Float worldMaxY = focus.y + imageHeight * 0.5f * worldPerPixel;

					ImGui::PushClipRect(ImVec2(screenPosition.x, screenPosition.y), ImVec2(screenPosition.x + imageWidth, screenPosition.y + imageHeight), true);

					Int gridStartX = static_cast<Int>(std::floor(worldMinX / gridSpacing));
					Int gridEndX = static_cast<Int>(std::ceil(worldMaxX / gridSpacing));
					for (Int gridIndex = gridStartX; gridIndex <= gridEndX; gridIndex++)
					{
						Float worldX = static_cast<Float>(gridIndex) * gridSpacing;
						Float lineScreenX = centerScreenX + (worldX - focus.x) / worldPerPixel;
						ImGui::GetWindowDrawList()->AddLine(ImVec2(lineScreenX, screenPosition.y), ImVec2(lineScreenX, screenPosition.y + imageHeight), IM_COL32(255, 255, 255, 30));
					}

					Int gridStartY = static_cast<Int>(std::floor(worldMinY / gridSpacing));
					Int gridEndY = static_cast<Int>(std::ceil(worldMaxY / gridSpacing));
					for (Int gridIndex = gridStartY; gridIndex <= gridEndY; gridIndex++)
					{
						Float worldY = static_cast<Float>(gridIndex) * gridSpacing;
						Float lineScreenY = centerScreenY - (worldY - focus.y) / worldPerPixel;
						ImGui::GetWindowDrawList()->AddLine(ImVec2(screenPosition.x, lineScreenY), ImVec2(screenPosition.x + imageWidth, lineScreenY), IM_COL32(255, 255, 255, 30));
					}

					ImGui::PopClipRect();

					Float landmarkMinX = centerScreenX + (100000.0f - focus.x) / worldPerPixel;
					Float landmarkMaxX = centerScreenX + (100000.0f + ScResolution::SC_HD.Width - focus.x) / worldPerPixel;
					Float landmarkMinY = centerScreenY - (100000.0f + ScResolution::SC_HD.Height - focus.y) / worldPerPixel;
					Float landmarkMaxY = centerScreenY - (100000.0f - focus.y) / worldPerPixel;

					ImVec2 landmarkMin = ImVec2(landmarkMinX, landmarkMinY);
					ImVec2 landmarkMax = ImVec2(landmarkMaxX, landmarkMaxY);

					ImGui::PushClipRect(ImVec2(screenPosition.x, screenPosition.y), ImVec2(screenPosition.x + imageWidth, screenPosition.y + imageHeight), true);
					ImGui::GetWindowDrawList()->AddRect(landmarkMin, landmarkMax, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
					ImGui::PopClipRect();
				}

				Vector2 cachePosition = { screenPosition.x, screenPosition.y };
				Vector2 cacheSize = { imageWidth, imageHeight };

				ImGui::PushClipRect(ImVec2(screenPosition.x, screenPosition.y), ImVec2(screenPosition.x + imageWidth, screenPosition.y + imageHeight), true);
				guizmoPanel_.Draw(cachePosition, cacheSize);
				ImGui::PopClipRect();
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}
}
