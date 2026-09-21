#include <Editor/Editor/Panel/GuizmoPanel2D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
#include <FoundationEngine/ECS/CompoundCommand.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

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
				context_.viewportContext_.guizmo_.rectTool_ = false;
				context_.viewportContext_.guizmo_.guizmoOperation_ = (ImGuizmo::OPERATION)0;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_W))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.rectTool_ = false;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.rectTool_ = false;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.rectTool_ = false;
				context_.viewportContext_.guizmo_.guizmoOperation_ = ImGuizmo::SCALE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_T))
			{
				context_.viewportContext_.guizmo_.showGuizmo_ = true;
				context_.viewportContext_.guizmo_.rectTool_ = true;
			}
		}

		if (!context_.viewportContext_.guizmo_.rectTool_ || !wasDragging_)
		{
			rectHandle_ = 0;
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
			if (context_.viewportContext_.guizmo_.rectTool_)
			{
				ImGuiIO& io = ImGui::GetIO();
				World& world = *context_.worldContext_.world_;
				CanvasCamera& camera = *context_.cameraContext_.canvasCamera_;

				static const String positionString("Position");
				static const String scaleString("Scale");
				ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
				ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

				Float canvasHeight = ScResolution::SC_CANVAS.Height;
				Float worldPerPixel = camera.VisibleHeight() / size.y;
				Vector3 focus = camera.Focus();
				Vector2 screenCenter = Vector2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
				Vector2 mouse = Vector2(io.MousePos.x, io.MousePos.y);
				Vector2 mouseCanvas = Vector2(focus.x + (mouse.x - screenCenter.x) * worldPerPixel, focus.y - (mouse.y - screenCenter.y) * worldPerPixel);

				Vector2 origin = Vector2(0.0f, 0.0f);
				Vector2 axisU = Vector2(1.0f, 0.0f);
				Vector2 axisV = Vector2(0.0f, 1.0f);
				Vector4 bounds = rectDragStart_;

				if (wasDragging_)
				{
					origin = Vector2(dragStartPivotMatrix_._41, dragStartPivotMatrix_._42);
					axisU = Vector2(dragStartPivotMatrix_._11, dragStartPivotMatrix_._12);
					axisV = Vector2(dragStartPivotMatrix_._21, dragStartPivotMatrix_._22);
				}
				else
				{
					Int32 canvasActorCount = 0;
					Vector4 canvasBounds = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

					for (Actor actor : selectedActors)
					{
						const Bounds* actorBounds = actor.GetComponent<Bounds>();
						const Image* image = actor.GetComponent<Image>();
						const Text* text = actor.GetComponent<Text>();
						const Movie* movie = actor.GetComponent<Movie>();
						Bool isImage = image && image->viewType_ == Image::ViewType::Sprite;
						Bool isCanvasActor = isImage || (text && text->viewType_ == Text::ViewType::Sprite) || (movie && movie->displayMode_ == Movie::DisplayMode::Sprite);
						if (!actorBounds || !isCanvasActor)
						{
							continue;
						}

						Vector3 worldScale;
						Quaternion worldRotation;
						Vector3 worldTranslation;
						Matrix worldMatrix = actor.GetWorldMatrix();
						worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

						Float rotation = isImage ? worldRotation.ToEuler().x : 0.0f;

						origin = Vector2(100000.0f + worldTranslation.x, 100000.0f + canvasHeight - worldTranslation.y);
						axisU = Vector2(std::cos(rotation), -std::sin(rotation));
						axisV = Vector2(std::sin(rotation), std::cos(rotation));
						bounds = Vector4((actorBounds->center_.x - actorBounds->extent_.x) * worldScale.x, (actorBounds->center_.y - actorBounds->extent_.y) * worldScale.y, (actorBounds->center_.x + actorBounds->extent_.x) * worldScale.x, (actorBounds->center_.y + actorBounds->extent_.y) * worldScale.y);

						for (Int32 cornerIndex = 0; cornerIndex < 4; cornerIndex++)
						{
							Vector2 corner = origin + axisU * (cornerIndex % 2 == 0 ? bounds.x : bounds.z) + axisV * (cornerIndex / 2 == 0 ? bounds.y : bounds.w);
							canvasBounds = Vector4(Min(canvasBounds.x, corner.x), Min(canvasBounds.y, corner.y), Max(canvasBounds.z, corner.x), Max(canvasBounds.w, corner.y));
						}

						canvasActorCount++;
					}

					if (canvasActorCount == 0)
					{
						return;
					}

					if (canvasActorCount > 1)
					{
						origin = Vector2(0.0f, 0.0f);
						axisU = Vector2(1.0f, 0.0f);
						axisV = Vector2(0.0f, 1.0f);
						bounds = canvasBounds;
					}
				}

				Vector2 mouseOffset = mouseCanvas - origin;
				Vector2 mouseFrame = Vector2(mouseOffset.Dot(axisU), mouseOffset.Dot(axisV));

				Vector2 fixedPoint = Vector2((bounds.x + bounds.z) * 0.5f, (bounds.y + bounds.w) * 0.5f);
				Vector2 rectScale = Vector2(1.0f, 1.0f);
				Vector2 rectMove = Vector2(0.0f, 0.0f);

				if (wasDragging_)
				{
					Int32 sideX = (rectHandle_ - 1) % 3 - 1;
					Int32 sideY = (rectHandle_ - 1) / 3 - 1;
					Vector2 dragDelta = mouseFrame - rectDragMouse_;

					if (sideX == 0 && sideY == 0)
					{
						rectMove = dragDelta;
					}
					else
					{
						if (sideX != 0 && !io.KeyAlt)
						{
							fixedPoint.x = sideX > 0 ? bounds.x : bounds.z;
						}
						if (sideY != 0 && !io.KeyAlt)
						{
							fixedPoint.y = sideY > 0 ? bounds.y : bounds.w;
						}

						Float handleX = sideX > 0 ? bounds.z : bounds.x;
						Float handleY = sideY > 0 ? bounds.w : bounds.y;
						if (sideX != 0 && std::abs(handleX - fixedPoint.x) > 0.0001f)
						{
							rectScale.x = (handleX + dragDelta.x - fixedPoint.x) / (handleX - fixedPoint.x);
						}
						if (sideY != 0 && std::abs(handleY - fixedPoint.y) > 0.0001f)
						{
							rectScale.y = (handleY + dragDelta.y - fixedPoint.y) / (handleY - fixedPoint.y);
						}

						if (io.KeyShift)
						{
							if (sideX != 0 && sideY != 0)
							{
								Float uniformScale = std::abs(rectScale.x - 1.0f) >= std::abs(rectScale.y - 1.0f) ? rectScale.x : rectScale.y;
								rectScale = Vector2(uniformScale, uniformScale);
							}
							else if (sideX != 0)
							{
								rectScale.y = rectScale.x;
							}
							else
							{
								rectScale.x = rectScale.y;
							}
						}

						rectScale = Vector2(Max(rectScale.x, 0.01f), Max(rectScale.y, 0.01f));
					}

					for (Size index = 0; index < dragEntities_.size(); ++index)
					{
						Entity entity = dragEntities_[index];
						Actor actor = world.GetActor(entity);

						Vector3 startTranslation = dragStartWorldMatrices_[index].Translation();
						Vector2 startOffset = Vector2(100000.0f + startTranslation.x, 100000.0f + canvasHeight - startTranslation.y) - origin;
						Vector2 startFrame = Vector2(startOffset.Dot(axisU), startOffset.Dot(axisV));
						Vector2 newFrame = fixedPoint + (startFrame - fixedPoint) * rectScale + rectMove;
						Vector2 newCanvas = origin + axisU * newFrame.x + axisV * newFrame.y;
						Vector3 newTranslation = Vector3(newCanvas.x - 100000.0f, 100000.0f + canvasHeight - newCanvas.y, startTranslation.z);

						Actor parentActor = actor ? actor.GetParent() : Actor();
						Vector3 localTranslation = parentActor ? Vector3::Transform(newTranslation, parentActor.GetWorldMatrix().Invert()) : newTranslation;

						Float* positionData = static_cast<Float*>(world.GetComponent(entity, positionID));
						Float* scaleData = static_cast<Float*>(world.GetComponent(entity, scaleID));
						if (positionData)
						{
							positionData[0] = localTranslation.x;
							positionData[1] = localTranslation.y;
						}
						if (scaleData)
						{
							scaleData[0] = dragStartScales_[index].x * rectScale.x;
							scaleData[1] = dragStartScales_[index].y * rectScale.y;
						}
					}

					if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						ResourcePtr<CompoundCommand> dragCommand = MakePtr<CompoundCommand>();

						for (Size index = 0; index < dragEntities_.size(); ++index)
						{
							Entity entity = dragEntities_[index];

							Float* positionData = static_cast<Float*>(world.GetComponent(entity, positionID));
							Float* scaleData = static_cast<Float*>(world.GetComponent(entity, scaleID));

							if (positionData)
							{
								Vector3 newPosition(positionData[0], positionData[1], positionData[2]);
								if (newPosition != dragStartPositions_[index])
								{
									dragCommand->Add(MakePtr<ComponentCommand<Vector3>>(world, entity, positionID, 0, dragStartPositions_[index], newPosition));
								}
							}
							if (scaleData)
							{
								Vector3 newScale(scaleData[0], scaleData[1], scaleData[2]);
								if (newScale != dragStartScales_[index])
								{
									dragCommand->Add(MakePtr<ComponentCommand<Vector3>>(world, entity, scaleID, 0, dragStartScales_[index], newScale));
								}
							}
						}

						if (!dragCommand->Empty())
						{
							context_.sceneContext_.history_.Push(std::move(dragCommand));
						}

						wasDragging_ = false;
					}
				}

				Vector4 displayBounds = Vector4(fixedPoint.x + (bounds.x - fixedPoint.x) * rectScale.x + rectMove.x, fixedPoint.y + (bounds.y - fixedPoint.y) * rectScale.y + rectMove.y, fixedPoint.x + (bounds.z - fixedPoint.x) * rectScale.x + rectMove.x, fixedPoint.y + (bounds.w - fixedPoint.y) * rectScale.y + rectMove.y);

				Vector2 handleScreens[9];
				for (Int32 handleIndex = 0; handleIndex < 9; handleIndex++)
				{
					Int32 handleSideX = handleIndex % 3 - 1;
					Int32 handleSideY = handleIndex / 3 - 1;
					Float frameX = handleSideX < 0 ? displayBounds.x : (handleSideX > 0 ? displayBounds.z : (displayBounds.x + displayBounds.z) * 0.5f);
					Float frameY = handleSideY < 0 ? displayBounds.y : (handleSideY > 0 ? displayBounds.w : (displayBounds.y + displayBounds.w) * 0.5f);
					Vector2 handleCanvas = origin + axisU * frameX + axisV * frameY;
					handleScreens[handleIndex] = Vector2(screenCenter.x + (handleCanvas.x - focus.x) / worldPerPixel, screenCenter.y - (handleCanvas.y - focus.y) / worldPerPixel);
				}

				Bool mouseInImage = ImGui::IsWindowHovered() && mouse.x >= position.x && mouse.x <= position.x + size.x && mouse.y >= position.y && mouse.y <= position.y + size.y;
				if (!wasDragging_)
				{
					rectHandle_ = 0;

					if (mouseInImage)
					{
						constexpr Int32 cornerHandles[4] = { 0, 2, 6, 8 };
						for (Int32 cornerHandle : cornerHandles)
						{
							if ((mouse - handleScreens[cornerHandle]).Length() <= 8.0f)
							{
								rectHandle_ = cornerHandle + 1;
								break;
							}
						}

						constexpr Int32 edgeHandles[4][3] = { { 1, 0, 2 }, { 3, 0, 6 }, { 5, 2, 8 }, { 7, 6, 8 } };
						for (Int32 edgeIndex = 0; edgeIndex < 4 && rectHandle_ == 0; edgeIndex++)
						{
							Vector2 edgeStart = handleScreens[edgeHandles[edgeIndex][1]];
							Vector2 edgeSegment = handleScreens[edgeHandles[edgeIndex][2]] - edgeStart;
							Float along = std::clamp((mouse - edgeStart).Dot(edgeSegment) / Max(edgeSegment.LengthSquared(), 0.0001f), 0.0f, 1.0f);
							if ((mouse - (edgeStart + edgeSegment * along)).Length() <= 6.0f)
							{
								rectHandle_ = edgeHandles[edgeIndex][0] + 1;
							}
						}

						if (rectHandle_ == 0 && mouseFrame.x >= displayBounds.x && mouseFrame.x <= displayBounds.z && mouseFrame.y >= displayBounds.y && mouseFrame.y <= displayBounds.w)
						{
							rectHandle_ = 5;
						}
					}

					if (rectHandle_ != 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						wasDragging_ = true;
						rectDragStart_ = bounds;
						rectDragMouse_ = mouseFrame;
						dragStartPivotMatrix_ = Matrix(axisU.x, axisU.y, 0.0f, 0.0f, axisV.x, axisV.y, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, origin.x, origin.y, 0.0f, 1.0f);

						dragEntities_.clear();
						dragStartWorldMatrices_.clear();
						dragStartPositions_.clear();
						dragStartRotations_.clear();
						dragStartScales_.clear();

						for (Actor actor : selectedActors)
						{
							Entity entity = actor.GetEntity();
							dragEntities_.push_back(entity);
							dragStartWorldMatrices_.push_back(actor.GetWorldMatrix());

							Float* positionData = static_cast<Float*>(world.GetComponent(entity, positionID));
							Float* scaleData = static_cast<Float*>(world.GetComponent(entity, scaleID));

							dragStartPositions_.push_back(positionData ? Vector3(positionData[0], positionData[1], positionData[2]) : Vector3::Zero);
							dragStartScales_.push_back(scaleData ? Vector3(scaleData[0], scaleData[1], scaleData[2]) : Vector3::One);
						}
					}
				}

				if (rectHandle_ != 0)
				{
					Int32 cursorSideX = (rectHandle_ - 1) % 3 - 1;
					Int32 cursorSideY = (rectHandle_ - 1) / 3 - 1;
					if (cursorSideX == 0 && cursorSideY == 0)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
					}
					else if (cursorSideY == 0)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					}
					else if (cursorSideX == 0)
					{
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
					else
					{
						ImGui::SetMouseCursor(cursorSideX == cursorSideY ? ImGuiMouseCursor_ResizeNESW : ImGuiMouseCursor_ResizeNWSE);
					}
				}

				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImU32 rectColor = IM_COL32(60, 150, 255, 255);
				ImU32 hotColor = IM_COL32(255, 200, 60, 255);

				drawList->AddQuad(ImVec2(handleScreens[0].x, handleScreens[0].y), ImVec2(handleScreens[2].x, handleScreens[2].y), ImVec2(handleScreens[8].x, handleScreens[8].y), ImVec2(handleScreens[6].x, handleScreens[6].y), rectColor, 1.5f);
				for (Int32 handleIndex = 0; handleIndex < 9; handleIndex++)
				{
					ImVec2 handlePoint = ImVec2(handleScreens[handleIndex].x, handleScreens[handleIndex].y);
					ImU32 handleColor = rectHandle_ == handleIndex + 1 ? hotColor : rectColor;
					if (handleIndex == 4)
					{
						drawList->AddCircle(handlePoint, 5.0f, handleColor, 0, 1.5f);
					}
					else
					{
						drawList->AddCircleFilled(handlePoint, handleIndex % 2 == 0 ? 5.0f : 3.5f, handleColor);
					}
				}

				return;
			}

			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(position.x, position.y, size.x, size.y);
			ImGuizmo::SetOrthographic(true);
			ImGuizmo::AllowAxisFlip(false);
			ImGuizmo::SetGizmoSizeClipSpace(0.2f);

			Float renderWidth = ScResolution::SC_CANVAS.Width;
			Float renderHeight = ScResolution::SC_CANVAS.Height;

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

	Bool GuizmoPanel2D::RectToolActive()const
	{
		return rectHandle_ != 0;
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
