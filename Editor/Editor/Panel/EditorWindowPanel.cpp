#include <Editor/Editor/Panel/EditorWindowPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/ViewportPicking.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/EditorCameraController.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	EditorWindowPanel::EditorWindowPanel(EditorContext& context, ImGuiTexture& imguiTexture) : context_(context), imguiTexture_(imguiTexture), guizmoPanel_(context)
	{
		/// No Code
	}

	void EditorWindowPanel::DrawGizmoMenu()
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
		if (ImGui::ImageButton("##GizmoIcon", imguiTexture_.Icon(IconType::Guizmo), iconSize))
		{
			ImGui::OpenPopup("##GizmoSettings");
		}
		ImGui::PopStyleColor(3);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		if (ImGui::BeginPopup("##GizmoSettings"))
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

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, transparent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		if (ImGui::ImageButton("##CameraIcon", imguiTexture_.Icon(IconType::Camera), iconSize))
		{
			ImGui::OpenPopup("##CameraSettings");
		}
		ImGui::PopStyleColor(3);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		if (ImGui::BeginPopup("##CameraSettings"))
		{
			if (context_.cameraContext_.editorCamera_)
			{
				EditorCamera& camera = *context_.cameraContext_.editorCamera_;
				ImGui::SeparatorText("投影");

				Float nearPlane = camera.Near();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("Near", &nearPlane, 0.001f, 0.0001f, 10.0f, "%.4f"))
				{
					camera.Near(nearPlane);
				}

				Float farPlane = camera.Far();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("Far", &farPlane, 1.0f, 1.0f, 100000.0f, "%.0f"))
				{
					camera.Far(farPlane);
				}

				Float fov = camera.Fov();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("FOV", &fov, 0.5f, 1.0f, 179.0f, "%.1f"))
				{
					camera.Fov(fov);
				}
			}

			if (context_.cameraContext_.editorCameraController_)
			{
				EditorCameraController& controller = *context_.cameraContext_.editorCameraController_;
				ImGui::SeparatorText("操作速度");

				Float moveSpeed = controller.MoveSpeed();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("移動", &moveSpeed, 0.1f, 0.1f, 200.0f, "%.1f"))
				{
					controller.MoveSpeed(moveSpeed);
				}

				Float rotateSpeed = controller.RotateSpeed();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("回転", &rotateSpeed, 0.01f, 0.01f, 2.0f, "%.2f"))
				{
					controller.RotateSpeed(rotateSpeed);
				}

				Float scrollSpeed = controller.ScrollSpeed();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("ズーム", &scrollSpeed, 0.1f, 0.1f, 100.0f, "%.1f"))
				{
					controller.ScrollSpeed(scrollSpeed);
				}

				Float panSpeed = controller.PanSpeed();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("パン", &panSpeed, 0.001f, 0.001f, 1.0f, "%.3f"))
				{
					controller.PanSpeed(panSpeed);
				}

				Float shiftMultiplier = controller.ShiftSpeedMultiplier();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::DragFloat("Shift倍率", &shiftMultiplier, 0.1f, 1.0f, 20.0f, "x%.1f"))
				{
					controller.ShiftSpeedMultiplier(shiftMultiplier);
				}
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(3);

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, transparent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
		if (ImGui::ImageButton("##ViewModeIcon", imguiTexture_.Icon(IconType::ViewMode), iconSize))
		{
			ImGui::OpenPopup("##ViewModeSettings");
		}
		ImGui::PopStyleColor(3);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		if (ImGui::BeginPopup("##ViewModeSettings"))
		{
			auto item = [&](const Char* label, ViewMode mode)
			{
				if (ImGui::MenuItem(label, nullptr, context_.viewportContext_.viewMode_ == mode))
				{
					context_.viewportContext_.viewMode_ = mode;
				}
			};

			item("ライティングあり", ViewMode::Lit);
			item("ライティングなし", ViewMode::Unlit);
			item("ワイヤーフレーム", ViewMode::Wireframe);
			item("深度", ViewMode::Depth);
			item("メッシュレット", ViewMode::Meshlet);

			if (ImGui::BeginMenu("バッファービジュアライゼーション"))
			{
				item("法線", ViewMode::Normal);
				item("ラフネス", ViewMode::Roughness);
				item("メタルネス", ViewMode::Metallic);
				item("エミッシブ", ViewMode::Emissive);
				item("モーションベクター", ViewMode::Velocity);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("レイトレーシング"))
			{
				item("反射（生）", ViewMode::ReflectionRaw);
				item("反射（デノイズ後）", ViewMode::ReflectionDenoised);
				ImGui::Separator();
				item("グローバルイルミネーション（生）", ViewMode::GlobalIlluminationRaw);
				item("グローバルイルミネーション（デノイズ後）", ViewMode::GlobalIlluminationDenoised);
				ImGui::Separator();
				item("シャドウ（生）", ViewMode::ShadowRaw);
				item("シャドウ（デノイズ後）", ViewMode::ShadowDenoised);
				ImGui::Separator();
				item("アンビエントオクルージョン（生）", ViewMode::AmbientOcclusionRaw);
				item("アンビエントオクルージョン（デノイズ後）", ViewMode::AmbientOcclusionDenoised);
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(3);
	}

	void EditorWindowPanel::Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle)
	{
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		/// [EN] Moved to Editor::DrawToolbar() (runs before this every frame)
		///      so TimelinePanel's preview grid — also drawn from
		///      DrawToolbar(), before this function runs — doesn't use last
		///      frame's stale ImGuizmo state.
		/// [JP] Editor::DrawToolbar()へ移動(毎フレームこの関数より先に実行
		///      される) — 同じくDrawToolbar()内で(この関数より前に)描画される
		///      TimelinePanelのプレビューグリッドが前フレームの古いImGuizmo
		///      状態を使わないようにするため。

		if (!focusedOnce_)
		{
			ImGui::SetNextWindowFocus();
			focusedOnce_ = true;
		}

		if (ImGui::Begin("エディタービュー"))
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

				ImVec2 borderMin = ImVec2(screenPosition.x - 1.0f, screenPosition.y - 1.0f);
				ImVec2 borderMax = ImVec2(screenPosition.x + imageWidth + 1.0f, screenPosition.y + imageHeight + 1.0f);
				ImGui::GetWindowDrawList()->AddRect(borderMin, borderMax, ImGui::GetColorU32(ImGuiCol_Border));

				/// [EN] Click-to-select: a plain left click (not a drag,
				///      not over a gizmo handle) on the viewport image
				///      unprojects the mouse into a world-space ray and
				///      picks the closest actor whose Bounds it hits (see
				///      ViewportPicking). Clicking empty space clears
				///      selection, matching Unreal's click-to-deselect.
				/// [JP] クリックで選択: ビューポート画像上での単純な左クリック
				///      （ドラッグでもギズモハンドル上でもない）を、ワールド
				///      空間のレイへ逆投影し、その Bounds に当たる最も近い
				///      アクターを選択する（ViewportPicking 参照）。何もない場所を
				///      クリックすると選択解除する（Unrealの挙動と同じ）。
				if (ImGui::IsWindowHovered() && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && context_.cameraContext_.editorCamera_ && context_.worldContext_.world_)
				{
					ImVec2 mousePosition = ImGui::GetMousePos();
					if (mousePosition.x >= screenPosition.x && mousePosition.x <= screenPosition.x + imageWidth &&
						mousePosition.y >= screenPosition.y && mousePosition.y <= screenPosition.y + imageHeight)
					{
						EditorCamera& camera = *context_.cameraContext_.editorCamera_;

						Float ndcX = ((mousePosition.x - screenPosition.x) / imageWidth) * 2.0f - 1.0f;
						Float ndcY = 1.0f - ((mousePosition.y - screenPosition.y) / imageHeight) * 2.0f;

						Vector3 rayOrigin = camera.Eye();
						Vector4 farPointH = Vector4::Transform(Vector4(ndcX, ndcY, 1.0f, 1.0f), camera.InverseViewProjection());
						Vector3 farPoint = Vector3(farPointH.x, farPointH.y, farPointH.z) / farPointH.w;
						Vector3 rayDirection = farPoint - rayOrigin;
						rayDirection.Normalize();

						Actor* hitActor = ViewportPicking::Pick(*context_.worldContext_.world_, rayOrigin, rayDirection);

						context_.selectionContext_.selectedActors_.clear();
						if (hitActor)
						{
							context_.selectionContext_.selectedActors_.push_back(hitActor);
						}
						context_.selectionContext_.selectedActor_ = hitActor;
						context_.selectionContext_.selectedEntity_ = hitActor ? hitActor->GetEntity() : Entity::Null();
					}
				}

				if (context_.cameraContext_.editorCamera_)
				{
					constexpr Float gizmoSize = 80.0f;
					ImVec2 gizmoPosition = ImVec2(screenPosition.x, screenPosition.y + imageHeight - gizmoSize);

					Matrix view = context_.cameraContext_.editorCamera_->View();

					Float orbitDistance = Vector3::Distance(context_.cameraContext_.editorCamera_->Eye(), context_.cameraContext_.editorCamera_->Focus());
					if (orbitDistance < 0.01f)
					{
						orbitDistance = 8.0f;
					}

					ImGuizmo::SetDrawlist();
					ImGuizmo::SetRect(screenPosition.x, screenPosition.y, imageWidth, imageHeight);
					ImGuizmo::ViewManipulate(reinterpret_cast<Float*>(&view), orbitDistance, gizmoPosition, ImVec2(gizmoSize, gizmoSize), 0x00000000);

					if (ImGuizmo::IsUsingViewManipulate())
					{
						Matrix cameraWorld = view.Invert();

						Vector3 newEye = cameraWorld.Translation();
						Vector3 newForward = Vector3::TransformNormal(Vector3::Forward, cameraWorld);
						newForward.Normalize();

						Vector3 worldUp = Vector3::Up;
						Vector3 newUp = worldUp - newForward * newForward.Dot(worldUp);
						if (newUp.LengthSquared() < 1e-6f)
						{
							newUp = context_.cameraContext_.editorCamera_->Up();
						}
						newUp.Normalize();

						context_.cameraContext_.editorCamera_->Eye(newEye);
						context_.cameraContext_.editorCamera_->Focus(newEye + newForward * orbitDistance);
						context_.cameraContext_.editorCamera_->Up(newUp);
					}

					Vector2 cachePosition = { screenPosition.x, screenPosition.y };
					Vector2 cacheSize = { imageWidth, imageHeight };
					guizmoPanel_.Draw(cachePosition, cacheSize);
				}
			}

			/// [EN] Right-click (rotate) or middle-click (pan) held over this
			///      viewport locks the OS cursor in place (hidden, re-anchored
			///      every frame) so a long drag never hits a monitor edge and
			///      stalls input. Begin only while hovering (so a drag that
			///      didn't start here can't capture); the release check runs
			///      unconditionally (not hover-gated) so capture always ends
			///      even if hover state changed mid-drag.
			/// [JP] このビューポート上での右クリック（回転）/中クリック（パン）
			///      押下中は OS カーソルを固定する（非表示にし、毎フレーム
			///      再アンカー）。これにより長いドラッグでもモニタ端に到達して
			///      入力が止まらない。開始は hover 中のみ（ここで始まって
			///      いないドラッグがキャプチャしないように）。解除判定は
			///      hover 条件の外（無条件）で行い、ドラッグ中に hover 状態が
			///      変わってもキャプチャが必ず解除されるようにする。
			/// [EN] Ctrl+wheel over the viewport adjusts the current gizmo operation's
			///      snap value instead of dollying the camera - EditorCameraController
			///      skips its own wheel dolly while Ctrl is held (see its Update()).
			/// [JP] ビューポート上での Ctrl+ホイールは、カメラのドリーではなく
			///      現在のギズモ操作のスナップ値を変更する - EditorCameraController は
			///      Ctrl 押下中、自身のホイールドリーをスキップする（Update() 参照）。
			if (ImGui::IsWindowHovered() && InputSystem::KeyState(InputSystem::Key::Control) && context_.viewportContext_.guizmo_.guizmoOperation_ != 0)
			{
				auto& guizmo = context_.viewportContext_.guizmo_;

				Float wheel = InputSystem::MouseWheelDelta();
				if (Abs(wheel) > 0.0f)
				{
					if (guizmo.guizmoOperation_ == ImGuizmo::TRANSLATE)
					{
						guizmo.translateSnap_ = Clamp(guizmo.translateSnap_ + wheel * 0.1f, 0.01f, 100.0f);
					}
					else if (guizmo.guizmoOperation_ == ImGuizmo::ROTATE)
					{
						guizmo.rotateSnap_ = Clamp(guizmo.rotateSnap_ + wheel * 0.5f, 0.1f, 90.0f);
					}
					else if (guizmo.guizmoOperation_ == ImGuizmo::SCALE)
					{
						guizmo.scaleSnap_ = Clamp(guizmo.scaleSnap_ + wheel * 0.05f, 0.01f, 10.0f);
					}
				}

				/// [EN] Ctrl alone (even without scrolling yet) shows the current
				///      operation's snap value as a tooltip, so the value is visible
				///      before/while adjusting it - it's otherwise only shown inside
				///      the toolbar's gizmo settings popup.
				/// [JP] Ctrl を押しているだけ（まだスクロールしていなくても）で
				///      現在の操作のスナップ値をツールチップ表示する。ホイールで
				///      変更する前/最中に値が見えるように - 通常はツールバーの
				///      ギズモ設定ポップアップの中でしか見えない。
				if (guizmo.guizmoOperation_ == ImGuizmo::TRANSLATE)
				{
					ImGui::SetTooltip("移動スナップ: %.2f", guizmo.translateSnap_);
				}
				else if (guizmo.guizmoOperation_ == ImGuizmo::ROTATE)
				{
					ImGui::SetTooltip("回転スナップ: %.0f\xc2\xb0", guizmo.rotateSnap_);
				}
				else if (guizmo.guizmoOperation_ == ImGuizmo::SCALE)
				{
					ImGui::SetTooltip("拡大縮小スナップ: %.2f", guizmo.scaleSnap_);
				}
			}

			Bool editorRotateHeld = InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed);
			Bool editorPanHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

			/// [EN] While right-click flying, the wheel changes move speed (see
			///      EditorCameraController::Update). Only pop the tooltip up when
			///      an actual wheel tick happens (not just because right-click is
			///      held), then keep it visible for a short window afterward via
			///      moveSpeedTooltipTimer_ so it's readable instead of a 1-frame
			///      flash.
			/// [JP] 右クリックでの視点移動中はホイールで移動速度が変わる
			///      （EditorCameraController::Update 参照）。右クリック中という
			///      だけでは出さず、実際にホイールが動いたフレームだけツール
			///      チップを出し、その後は moveSpeedTooltipTimer_ で短時間表示を
			///      維持する（1フレームだけの点滅で読めなくなるのを防ぐ）。
			if (editorRotateHeld && ImGui::IsWindowHovered() && Abs(InputSystem::MouseWheelDelta()) > 0.0f)
			{
				moveSpeedTooltipTimer_ = 1.0f;
			}
			else
			{
				moveSpeedTooltipTimer_ = Max(0.0f, moveSpeedTooltipTimer_ - ImGui::GetIO().DeltaTime);
			}

			if (moveSpeedTooltipTimer_ > 0.0f && context_.cameraContext_.editorCameraController_)
			{
				ImGui::SetTooltip("移動速度: %.2f", context_.cameraContext_.editorCameraController_->MoveSpeed());
			}

			if (!ImGuizmo::IsUsing() && ImGui::IsWindowHovered() && context_.cameraContext_.editorCamera_ && context_.cameraContext_.editorCameraController_)
			{
				if ((editorRotateHeld || editorPanHeld) && !InputSystem::IsMouseCaptured())
				{
					InputSystem::BeginMouseCapture();
				}

				Float deltaTime = ImGui::GetIO().DeltaTime;
				context_.cameraContext_.editorCameraController_->Update(*context_.cameraContext_.editorCamera_, deltaTime);
			}

			if (!editorRotateHeld && !editorPanHeld && InputSystem::IsMouseCaptured())
			{
				InputSystem::EndMouseCapture();
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}
}
