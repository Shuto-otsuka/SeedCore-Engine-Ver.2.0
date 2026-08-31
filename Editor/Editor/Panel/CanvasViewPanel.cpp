#include <Editor/Editor/Panel/CanvasViewPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

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

	void CanvasViewPanel::HandlePicking(const ImVec2& imageScreenPos, Float imageWidth, Float imageHeight)
	{
		if (!context_.cameraContext_.canvasCamera_ || !context_.worldContext_.world_)
		{
			return;
		}

		ImVec2 mouse = ImGui::GetMousePos();
		Bool mouseInImage = mouse.x >= imageScreenPos.x && mouse.x <= imageScreenPos.x + imageWidth && mouse.y >= imageScreenPos.y && mouse.y <= imageScreenPos.y + imageHeight;

		/// [EN] Gesture start: a left press inside the image that is not on a gizmo handle. Whether it turns out to be a click or a rubber-band box is decided later by how far the mouse travels.
		/// [JP] ジェスチャ開始: 画像内の、ギズモハンドル上でない左押下。クリックかラバーバンドボックスかは、この後マウスの移動距離で決まる。
		if (!isBoxSelectPending_ && !isBoxSelecting_)
		{
			if (mouseInImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver())
			{
				isBoxSelectPending_ = true;
				boxSelectStartScreen_ = mouse;
			}
			return;
		}

		/// [EN] Left button was released outside this handler's sight (focus loss, etc.) - drop the gesture.
		/// [JP] 左ボタンがこのハンドラの見えないところで離された(フォーカス喪失など) - ジェスチャを破棄。
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			isBoxSelectPending_ = false;
			isBoxSelecting_ = false;
			return;
		}

		/// [EN] Promote the pending press to a box select once it has dragged past a few pixels.
		/// [JP] 押下が数ピクセル動いたら、ラバーバンドボックス選択へ昇格させる。
		if (isBoxSelectPending_ && !isBoxSelecting_)
		{
			if (std::abs(mouse.x - boxSelectStartScreen_.x) + std::abs(mouse.y - boxSelectStartScreen_.y) > 4.0f)
			{
				isBoxSelecting_ = true;
			}
		}

		CanvasCamera& camera = *context_.cameraContext_.canvasCamera_;
		World& world = *context_.worldContext_.world_;

		/// [EN] Screen -> canvas world: the inverse of the grid-line placement in Draw(). A pixel offset from the image centre maps to a world offset from the camera focus; screen Y grows downward so it inverts.
		/// [JP] スクリーン -> キャンバスワールド: Draw() のグリッド線配置の逆。画像中心からのピクセルオフセットを、カメラ focus からのワールドオフセットへ写す。スクリーン Y は下方向に増えるので反転する。
		Float worldPerPixel = camera.VisibleHeight() / imageHeight;
		Vector3 focus = camera.Focus();
		Float centerScreenX = imageScreenPos.x + imageWidth * 0.5f;
		Float centerScreenY = imageScreenPos.y + imageHeight * 0.5f;

		if (isBoxSelecting_)
		{
			/// [EN] Draw the rubber-band rectangle over the canvas image.
			/// [JP] ラバーバンド矩形をキャンバス画像上に描く。
			ImVec2 rectMin = ImVec2(Min(boxSelectStartScreen_.x, mouse.x), Min(boxSelectStartScreen_.y, mouse.y));
			ImVec2 rectMax = ImVec2(Max(boxSelectStartScreen_.x, mouse.x), Max(boxSelectStartScreen_.y, mouse.y));
			ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, IM_COL32(80, 140, 255, 40));
			ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, IM_COL32(80, 140, 255, 200));
		}

		if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			return;
		}

		SelectionContext& selection = context_.selectionContext_;
		Bool ctrl = ImGui::GetIO().KeyCtrl;

		if (isBoxSelecting_)
		{
			/// [EN] Selection rectangle in canvas world space (both screen corners unprojected).
			/// [JP] キャンバスワールド空間での選択矩形(スクリーンの両隅を逆投影)。
			Float startWorldX = focus.x + (boxSelectStartScreen_.x - centerScreenX) * worldPerPixel;
			Float startWorldY = focus.y - (boxSelectStartScreen_.y - centerScreenY) * worldPerPixel;
			Float endWorldX = focus.x + (mouse.x - centerScreenX) * worldPerPixel;
			Float endWorldY = focus.y - (mouse.y - centerScreenY) * worldPerPixel;
			Float rectMinX = Min(startWorldX, endWorldX);
			Float rectMaxX = Max(startWorldX, endWorldX);
			Float rectMinY = Min(startWorldY, endWorldY);
			Float rectMaxY = Max(startWorldY, endWorldY);

			DynamicArray<Actor> hits;
			Query<Read<Active>, Read<Image>, Read<Bounds>> query(world);
			query.ForEach([&](EntityID entityID, const Active& active, const Image& image, const Bounds& bounds)
				{
					if (!active.active_ || image.viewType_ != Image::ViewType::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float rotation = worldRotation.ToEuler().x;
					Float cosRotation = std::cos(rotation);
					Float sinRotation = std::sin(rotation);

					Float centerX = 100000.0f + worldTranslation.x;
					Float centerY = 100000.0f + ScResolution::SC_HD.Height - worldTranslation.y;
					Float halfWidth = bounds.extent_.x * worldScale.x;
					Float halfHeight = bounds.extent_.y * worldScale.y;
					Float pivotX = bounds.center_.x * worldScale.x;
					Float pivotY = bounds.center_.y * worldScale.y;

					/// [EN] Axis-aligned bounds of the sprite's 4 rotated canvas-space corners (the shader rotates the quad by Rz(-rotation), so a local offset maps to world by Rz(-rotation)). Overlap-test that against the selection rectangle.
					/// [JP] スプライトの回転済みキャンバス空間の4隅の軸平行境界(シェーダーはクアッドを Rz(-rotation) で回すので、ローカルオフセットは Rz(-rotation) でワールドへ写る)。それを選択矩形と重なり判定する。
					Float quadMinX = FLT_MAX;
					Float quadMaxX = -FLT_MAX;
					Float quadMinY = FLT_MAX;
					Float quadMaxY = -FLT_MAX;
					for (Int cornerX = -1; cornerX <= 1; cornerX += 2)
					{
						for (Int cornerY = -1; cornerY <= 1; cornerY += 2)
						{
							Float localX = pivotX + static_cast<Float>(cornerX) * halfWidth;
							Float localY = pivotY + static_cast<Float>(cornerY) * halfHeight;
							Float worldX = centerX + localX * cosRotation + localY * sinRotation;
							Float worldY = centerY - localX * sinRotation + localY * cosRotation;
							quadMinX = Min(quadMinX, worldX);
							quadMaxX = Max(quadMaxX, worldX);
							quadMinY = Min(quadMinY, worldY);
							quadMaxY = Max(quadMaxY, worldY);
						}
					}

					if (quadMaxX >= rectMinX && quadMinX <= rectMaxX && quadMaxY >= rectMinY && quadMinY <= rectMaxY)
					{
						hits.push_back(actor);
					}
				});

			/// [EN] Sprite-view Text draws on the canvas with no rotation (FontRenderer sets the canvas instance rotation to zero), so its box is axis-aligned: centre ± half-extent, scaled by the actor's world scale. Bounds is synced by FontRenderer the same way ImageRenderer syncs Image.
			/// [JP] スプライトビューの Text はキャンバスに回転なしで描かれる(FontRenderer がキャンバスインスタンスの回転を 0 にする)ので、そのボックスは軸平行: 中心 ± 半径、アクターのワールドスケール倍。Bounds は ImageRenderer が Image を同期するのと同じ形で FontRenderer が同期する。
			Query<Read<Active>, Read<Text>, Read<Bounds>> textQuery(world);
			textQuery.ForEach([&](EntityID entityID, const Active& active, const Text& text, const Bounds& bounds)
				{
					if (!active.active_ || text.viewType_ != Text::ViewType::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float centerX = 100000.0f + worldTranslation.x + bounds.center_.x * worldScale.x;
					Float centerY = 100000.0f + ScResolution::SC_HD.Height - worldTranslation.y + bounds.center_.y * worldScale.y;
					Float halfWidth = bounds.extent_.x * worldScale.x;
					Float halfHeight = bounds.extent_.y * worldScale.y;

					if (centerX + halfWidth >= rectMinX && centerX - halfWidth <= rectMaxX && centerY + halfHeight >= rectMinY && centerY - halfHeight <= rectMaxY)
					{
						hits.push_back(actor);
					}
				});

			/// [EN] Sprite-mode Movie draws on the canvas unrotated (MovieRenderer zeroes the canvas instance rotation), Bounds synced by MovieRenderer the same way. Same axis-aligned box test as Text.
			/// [JP] スプライトモードの Movie はキャンバスに回転なしで描かれ(MovieRenderer がキャンバスインスタンスの回転を 0 にする)、Bounds も同じ形で同期される。Text と同じ軸平行ボックス判定。
			Query<Read<Active>, Read<Movie>, Read<Bounds>> movieQuery(world);
			movieQuery.ForEach([&](EntityID entityID, const Active& active, const Movie& movie, const Bounds& bounds)
				{
					if (!active.active_ || movie.displayMode_ != Movie::DisplayMode::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float centerX = 100000.0f + worldTranslation.x + bounds.center_.x * worldScale.x;
					Float centerY = 100000.0f + ScResolution::SC_HD.Height - worldTranslation.y + bounds.center_.y * worldScale.y;
					Float halfWidth = bounds.extent_.x * worldScale.x;
					Float halfHeight = bounds.extent_.y * worldScale.y;

					if (centerX + halfWidth >= rectMinX && centerX - halfWidth <= rectMaxX && centerY + halfHeight >= rectMinY && centerY - halfHeight <= rectMaxY)
					{
						hits.push_back(actor);
					}
				});

			if (!ctrl)
			{
				selection.selectedActors_.clear();
			}
			for (Actor actor : hits)
			{
				if (std::ranges::find(selection.selectedActors_, actor) == selection.selectedActors_.end())
				{
					selection.selectedActors_.push_back(actor);
				}
			}
		}
		else if (mouseInImage)
		{
			/// [EN] Click pick: mouse point against each Image's canvas-space quad, rebuilt exactly as ImageRenderer/ImageBillboardMS.hlsl draws it. Bounds (synced by ImageRenderer) supplies textureSize/2 in extent_ and the pivot shift in center_. The last hit in query order wins - that is the sprite drawn on top.
			/// [JP] クリックピック: マウス点を各 Image のキャンバス空間クアッドに対して判定する。クアッドは ImageRenderer/ImageBillboardMS.hlsl の描画と全く同じに再構成する。Bounds(ImageRenderer が同期)が extent_ に textureSize/2、center_ に pivot ずれを供給する。クエリ順で最後に当たったものが勝つ = 最前面に描かれたスプライト。
			Float mouseWorldX = focus.x + (mouse.x - centerScreenX) * worldPerPixel;
			Float mouseWorldY = focus.y - (mouse.y - centerScreenY) * worldPerPixel;

			Actor hit;
			Query<Read<Active>, Read<Image>, Read<Bounds>> query(world);
			query.ForEach([&](EntityID entityID, const Active& active, const Image& image, const Bounds& bounds)
				{
					if (!active.active_ || image.viewType_ != Image::ViewType::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float rotation = worldRotation.ToEuler().x;
					Float cosRotation = std::cos(rotation);
					Float sinRotation = std::sin(rotation);

					/// [EN] Mouse offset from the quad centre, rotated back into the quad's local frame (the shader applies Rz(-rotation), so undo with Rz(+rotation)).
					/// [JP] クアッド中心からのマウスオフセットを、クアッドのローカルフレームへ回転で戻す(シェーダーは Rz(-rotation) を掛けるので Rz(+rotation) で戻す)。
					Float deltaX = mouseWorldX - (100000.0f + worldTranslation.x);
					Float deltaY = mouseWorldY - (100000.0f + ScResolution::SC_HD.Height - worldTranslation.y);
					Float localX = deltaX * cosRotation - deltaY * sinRotation;
					Float localY = deltaX * sinRotation + deltaY * cosRotation;

					Float halfWidth = bounds.extent_.x * worldScale.x;
					Float halfHeight = bounds.extent_.y * worldScale.y;
					Float pivotX = bounds.center_.x * worldScale.x;
					Float pivotY = bounds.center_.y * worldScale.y;

					if (std::abs(localX - pivotX) <= halfWidth && std::abs(localY - pivotY) <= halfHeight)
					{
						hit = actor;
					}
				});

			/// [EN] Sprite-view Text: same canvas quad test but axis-aligned (canvas text is drawn unrotated). Runs after the Image query so text on top of an image wins the click.
			/// [JP] スプライトビューの Text: 同じキャンバスクアッド判定だが軸平行(キャンバステキストは回転なしで描かれる)。Image クエリの後に実行し、画像の上のテキストがクリックで勝つようにする。
			Query<Read<Active>, Read<Text>, Read<Bounds>> textQuery(world);
			textQuery.ForEach([&](EntityID entityID, const Active& active, const Text& text, const Bounds& bounds)
				{
					if (!active.active_ || text.viewType_ != Text::ViewType::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float localX = mouseWorldX - (100000.0f + worldTranslation.x) - bounds.center_.x * worldScale.x;
					Float localY = mouseWorldY - (100000.0f + ScResolution::SC_HD.Height - worldTranslation.y) - bounds.center_.y * worldScale.y;

					if (std::abs(localX) <= bounds.extent_.x * worldScale.x && std::abs(localY) <= bounds.extent_.y * worldScale.y)
					{
						hit = actor;
					}
				});

			/// [EN] Sprite-mode Movie: same axis-aligned canvas box test as Text, run last so a movie on top wins the click.
			/// [JP] スプライトモードの Movie: Text と同じ軸平行キャンバスボックス判定。最後に実行し、上にある動画がクリックで勝つようにする。
			Query<Read<Active>, Read<Movie>, Read<Bounds>> movieQuery(world);
			movieQuery.ForEach([&](EntityID entityID, const Active& active, const Movie& movie, const Bounds& bounds)
				{
					if (!active.active_ || movie.displayMode_ != Movie::DisplayMode::Sprite)
					{
						return;
					}

					Actor actor = world.GetActor(entityID);
					if (!actor)
					{
						return;
					}

					Vector3 worldScale;
					Quaternion worldRotation;
					Vector3 worldTranslation;
					Matrix worldMatrix = actor.GetWorldMatrix();
					worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

					Float localX = mouseWorldX - (100000.0f + worldTranslation.x) - bounds.center_.x * worldScale.x;
					Float localY = mouseWorldY - (100000.0f + ScResolution::SC_HD.Height - worldTranslation.y) - bounds.center_.y * worldScale.y;

					if (std::abs(localX) <= bounds.extent_.x * worldScale.x && std::abs(localY) <= bounds.extent_.y * worldScale.y)
					{
						hit = actor;
					}
				});

			if (ctrl && hit)
			{
				auto it = std::ranges::find(selection.selectedActors_, hit);
				if (it != selection.selectedActors_.end())
				{
					selection.selectedActors_.erase(it);
				}
				else
				{
					selection.selectedActors_.push_back(hit);
				}
			}
			else
			{
				selection.selectedActors_.clear();
				if (hit)
				{
					selection.selectedActors_.push_back(hit);
				}
			}
		}

		selection.selectedActor_ = selection.selectedActors_.empty() ? Actor() : selection.selectedActors_.back();
		selection.selectedEntity_ = selection.selectedActor_ ? selection.selectedActor_.GetEntity() : Entity::Null();

		isBoxSelectPending_ = false;
		isBoxSelecting_ = false;
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

				HandlePicking(screenPosition, imageWidth, imageHeight);

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
