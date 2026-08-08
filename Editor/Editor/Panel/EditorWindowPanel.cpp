#include <Editor/Editor/Panel/EditorWindowPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/EditorCameraController.h>
#include <FoundationEngine/ECS/Component/Bounds.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <cfloat>

namespace SeedCore
{
	namespace
	{
		/// [EN] Slab-method ray/AABB test, run entirely in the box's own
		///      local space (rayOriginLocal/rayDirectionLocal already
		///      transformed there by the caller) — this doubles as an OBB
		///      test for a rotated/scaled actor without needing dedicated
		///      OBB math (see PickActor). outT is the local-space ray
		///      parameter to report as this box's pick distance: the entry
		///      point when the ray starts outside the box (the common
		///      case), or the EXIT point when the ray origin starts inside
		///      it — e.g. the camera standing inside a large stage/room
		///      model. Reporting entry (which would be negative/clamped to
		///      0) for the inside-the-box case would make that enclosing
		///      box register as an always-wins, distance-0 hit, so nothing
		///      inside it (smaller props) could ever be picked instead.
		/// [JP] スラブ法によるレイ/AABB判定。箱自身のローカル空間で完全に
		///      行う（rayOriginLocal/rayDirectionLocal は呼び出し側で
		///      あらかじめそこへ変換済み）— これにより、回転/スケールした
		///      アクターに対しても専用の OBB 演算を用意せずに OBB 判定として
		///      機能する（PickActor 参照）。outT はこの箱のピック距離として
		///      報告するローカル空間レイパラメータ: レイが箱の外から
		///      始まる通常のケースでは侵入点、レイの起点が箱の内側にある
		///      場合（例: カメラが巨大なステージ/部屋モデルの内側にいる）は
		///      脱出点を使う。内側ケースで侵入点（負になり0にクランプされる）
		///      を報告すると、その箱が常に距離0で勝ってしまい、内部にある
		///      小物が絶対に選択できなくなるため。
		Bool RayIntersectsLocalAABB(const Vector3& rayOriginLocal, const Vector3& rayDirectionLocal, const Vector3& boundsMin, const Vector3& boundsMax, Float& outT)
		{
			Float origin[3] = { rayOriginLocal.x, rayOriginLocal.y, rayOriginLocal.z };
			Float direction[3] = { rayDirectionLocal.x, rayDirectionLocal.y, rayDirectionLocal.z };
			Float boundsMinArray[3] = { boundsMin.x, boundsMin.y, boundsMin.z };
			Float boundsMaxArray[3] = { boundsMax.x, boundsMax.y, boundsMax.z };

			Float tMin = -FLT_MAX;
			Float tMax = FLT_MAX;

			for (Int axis = 0; axis < 3; axis++)
			{
				if (std::abs(direction[axis]) < 1e-8f)
				{
					if (origin[axis] < boundsMinArray[axis] || origin[axis] > boundsMaxArray[axis])
					{
						return false;
					}
					continue;
				}

				Float inverseDirection = 1.0f / direction[axis];
				Float t0 = (boundsMinArray[axis] - origin[axis]) * inverseDirection;
				Float t1 = (boundsMaxArray[axis] - origin[axis]) * inverseDirection;
				if (t0 > t1)
				{
					std::swap(t0, t1);
				}

				tMin = Max(tMin, t0);
				tMax = Min(tMax, t1);
				if (tMin > tMax)
				{
					return false;
				}
			}

			if (tMax < 0.0f)
			{
				/// [EN] Whole box is behind the ray origin.
				/// [JP] 箱全体がレイの起点より後ろにある。
				return false;
			}

			outT = (tMin >= 0.0f) ? tMin : tMax;
			return true;
		}

		/// [EN] Finds the actor whose Bounds (see Bounds's class comment)
		///      the ray hits closest to rayOrigin, or nullptr if none.
		///      Transforms the ray into each candidate's local space via
		///      its inverse world matrix so Bounds (stored local-space) acts
		///      as an OBB, then converts the local hit back to world space
		///      to rank candidates by true world-space distance (raw local
		///      ray parameters aren't comparable across actors with
		///      different scales).
		/// [JP] レイが最も近くで当たる Bounds（Bounds のクラスコメント参照）
		///      を持つアクターを探す。無ければ nullptr。各候補の逆ワールド
		///      行列でレイをローカル空間へ変換し、Bounds（ローカル空間で
		///      保持）を OBB として機能させる。ローカルのヒット点はワールド
		///      空間へ変換し直してから比較する（スケールの異なるアクター間
		///      では、生のローカルレイパラメータは比較できないため）。
		Actor* PickActor(World& world, const Vector3& rayOrigin, const Vector3& rayDirection)
		{
			ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();
			if (!boundsComponentID)
			{
				return nullptr;
			}

			Actor* closestActor = nullptr;
			Float closestDistance = FLT_MAX;

			for (EntityID entityID : world.GetComponents<Bounds>())
			{
				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					continue;
				}

				const Bounds* bounds = actor->GetComponent<Bounds>();
				if (!bounds)
				{
					continue;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();
				Matrix inverseWorld = worldMatrix.Invert();

				Vector3 localOrigin = Vector3::Transform(rayOrigin, inverseWorld);
				Vector3 localDirection = Vector3::TransformNormal(rayDirection, inverseWorld);

				Vector3 boundsMin = bounds->center_ - bounds->extent_;
				Vector3 boundsMax = bounds->center_ + bounds->extent_;

				Float localT;
				if (!RayIntersectsLocalAABB(localOrigin, localDirection, boundsMin, boundsMax, localT))
				{
					continue;
				}

				Vector3 localHitPoint = localOrigin + localDirection * localT;
				Vector3 worldHitPoint = Vector3::Transform(localHitPoint, worldMatrix);
				Float worldDistance = Vector3::Distance(rayOrigin, worldHitPoint);

				if (worldDistance < closestDistance)
				{
					closestDistance = worldDistance;
					closestActor = actor;
				}
			}

			return closestActor;
		}
	}

	EditorWindowPanel::EditorWindowPanel(EditorContext& context, ImGuiTexture& imguiTexture) :context_(context), imguiTexture_(imguiTexture), guizmoPanel_(context)
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
				///      PickActor). Clicking empty space clears selection,
				///      matching Unreal's click-to-deselect.
				/// [JP] クリックで選択: ビューポート画像上での単純な左クリック
				///      （ドラッグでもギズモハンドル上でもない）を、ワールド
				///      空間のレイへ逆投影し、その Bounds に当たる最も近い
				///      アクターを選択する（PickActor 参照）。何もない場所を
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

						Actor* hitActor = PickActor(*context_.worldContext_.world_, rayOrigin, rayDirection);

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
			Bool editorRotateHeld = InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed);
			Bool editorPanHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

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
