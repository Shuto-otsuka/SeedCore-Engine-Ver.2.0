#include <Editor/Editor/Panel/GuizmoPanel3D.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
#include <FoundationEngine/ECS/CompoundCommand.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

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

		if (context_.viewportContext_.guizmo_.showGuizmo_)
		{
			if (context_.viewportContext_.guizmo_.rectTool_)
			{
				ImGuiIO& io = ImGui::GetIO();
				World& world = *context_.worldContext_.world_;
				EditorCamera& camera = *context_.cameraContext_.editorCamera_;

				static const String positionString("Position");
				static const String scaleString("Scale");
				ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
				ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

				Matrix viewProjection = camera.View() * Matrix::CreatePerspectiveFieldOfView(ToRadians(camera.Fov()), camera.AspectRatio(), camera.Near(), camera.Far());

				Vector2 mouse = Vector2(io.MousePos.x, io.MousePos.y);
				Float ndcX = ((mouse.x - position.x) / size.x) * 2.0f - 1.0f;
				Float ndcY = 1.0f - ((mouse.y - position.y) / size.y) * 2.0f;
				Vector4 farPoint = Vector4::Transform(Vector4(ndcX, ndcY, 1.0f, 1.0f), viewProjection.Invert());
				Vector3 rayOrigin = camera.Eye();
				Vector3 rayDirection = Vector3(farPoint.x, farPoint.y, farPoint.z) / farPoint.w - rayOrigin;
				rayDirection.Normalize();

				Vector3 origin = Vector3::Zero;
				Vector3 axisU = Vector3::UnitX;
				Vector3 axisV = Vector3::UnitY;
				Vector3 axisN = Vector3::UnitZ;
				Vector4 bounds = rectDragStart_;

				if (wasDragging_)
				{
					axisU = Vector3(dragStartPivotMatrix_._11, dragStartPivotMatrix_._12, dragStartPivotMatrix_._13);
					axisV = Vector3(dragStartPivotMatrix_._21, dragStartPivotMatrix_._22, dragStartPivotMatrix_._23);
					axisN = Vector3(dragStartPivotMatrix_._31, dragStartPivotMatrix_._32, dragStartPivotMatrix_._33);
					origin = Vector3(dragStartPivotMatrix_._41, dragStartPivotMatrix_._42, dragStartPivotMatrix_._43);
				}
				else
				{
					Int32 boundedActorCount = 0;
					Vector3 axes[3] = { Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ };
					Vector3 localMin = Vector3::Zero;
					Vector3 localMax = Vector3::Zero;
					Vector3 worldMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
					Vector3 worldMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

					for (Actor actor : selectedActors)
					{
						const Bounds* actorBounds = actor.GetComponent<Bounds>();
						if (!actorBounds)
						{
							continue;
						}

						Matrix worldMatrix = actor.GetWorldMatrix();
						Vector3 worldScale;
						Quaternion worldRotation;
						Vector3 worldTranslation;
						worldMatrix.Decompose(worldScale, worldRotation, worldTranslation);

						origin = worldTranslation;
						axes[0] = Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13);
						axes[1] = Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23);
						axes[2] = Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33);
						axes[0].Normalize();
						axes[1].Normalize();
						axes[2].Normalize();
						localMin = (actorBounds->center_ - actorBounds->extent_) * worldScale;
						localMax = (actorBounds->center_ + actorBounds->extent_) * worldScale;

						for (Int32 cornerIndex = 0; cornerIndex < 8; cornerIndex++)
						{
							Vector3 localCorner = actorBounds->center_ + Vector3((cornerIndex & 1) ? actorBounds->extent_.x : -actorBounds->extent_.x, (cornerIndex & 2) ? actorBounds->extent_.y : -actorBounds->extent_.y, (cornerIndex & 4) ? actorBounds->extent_.z : -actorBounds->extent_.z);
							Vector3 worldCorner = Vector3::Transform(localCorner, worldMatrix);
							worldMin = Vector3::Min(worldMin, worldCorner);
							worldMax = Vector3::Max(worldMax, worldCorner);
						}

						boundedActorCount++;
					}

					if (boundedActorCount == 0)
					{
						return;
					}

					if (boundedActorCount > 1)
					{
						origin = Vector3::Zero;
						axes[0] = Vector3::UnitX;
						axes[1] = Vector3::UnitY;
						axes[2] = Vector3::UnitZ;
						localMin = worldMin;
						localMax = worldMax;
					}

					Vector3 viewDirection = camera.Focus() - camera.Eye();
					viewDirection.Normalize();

					Int32 normalIndex = 0;
					Float bestFacing = -1.0f;
					for (Int32 axisIndex = 0; axisIndex < 3; axisIndex++)
					{
						Float facing = std::abs(axes[axisIndex].Dot(viewDirection));
						if (facing > bestFacing)
						{
							bestFacing = facing;
							normalIndex = axisIndex;
						}
					}

					Int32 uIndex = normalIndex == 0 ? 1 : 0;
					Int32 vIndex = normalIndex == 2 ? 1 : 2;
					Float minValues[3] = { localMin.x, localMin.y, localMin.z };
					Float maxValues[3] = { localMax.x, localMax.y, localMax.z };

					axisU = axes[uIndex];
					axisV = axes[vIndex];
					axisN = axes[normalIndex];
					origin += axisN * ((minValues[normalIndex] + maxValues[normalIndex]) * 0.5f);
					bounds = Vector4(minValues[uIndex], minValues[vIndex], maxValues[uIndex], maxValues[vIndex]);
				}

				Vector2 mouseFrame = Vector2(0.0f, 0.0f);
				Bool mouseOnPlane = false;
				Float rayFacing = rayDirection.Dot(axisN);
				if (std::abs(rayFacing) > 0.0001f)
				{
					Float rayDistance = (origin - rayOrigin).Dot(axisN) / rayFacing;
					if (rayDistance > 0.0f)
					{
						Vector3 planeHit = rayOrigin + rayDirection * rayDistance - origin;
						mouseFrame = Vector2(planeHit.Dot(axisU), planeHit.Dot(axisV));
						mouseOnPlane = true;
					}
				}

				Vector2 fixedPoint = Vector2((bounds.x + bounds.z) * 0.5f, (bounds.y + bounds.w) * 0.5f);
				Vector2 rectScale = Vector2(1.0f, 1.0f);
				Vector2 rectMove = Vector2(0.0f, 0.0f);

				if (wasDragging_)
				{
					Int32 sideX = (rectHandle_ - 1) % 3 - 1;
					Int32 sideY = (rectHandle_ - 1) / 3 - 1;
					Vector2 dragDelta = mouseOnPlane ? mouseFrame - rectDragMouse_ : Vector2(0.0f, 0.0f);

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
						const Matrix& startWorldMatrix = dragStartWorldMatrices_[index];

						Vector3 startTranslation = startWorldMatrix.Translation();
						Vector3 startOffset = startTranslation - origin;
						Vector2 startFrame = Vector2(startOffset.Dot(axisU), startOffset.Dot(axisV));
						Vector2 newFrame = fixedPoint + (startFrame - fixedPoint) * rectScale + rectMove;
						Vector3 newTranslation = startTranslation + axisU * (newFrame.x - startFrame.x) + axisV * (newFrame.y - startFrame.y);

						Actor parentActor = actor ? actor.GetParent() : Actor();
						Vector3 localTranslation = parentActor ? Vector3::Transform(newTranslation, parentActor.GetWorldMatrix().Invert()) : newTranslation;

						Vector3 actorAxes[3] = { Vector3(startWorldMatrix._11, startWorldMatrix._12, startWorldMatrix._13), Vector3(startWorldMatrix._21, startWorldMatrix._22, startWorldMatrix._23), Vector3(startWorldMatrix._31, startWorldMatrix._32, startWorldMatrix._33) };
						Int32 uScaleIndex = 0;
						Int32 vScaleIndex = 0;
						Float bestUFacing = -1.0f;
						Float bestVFacing = -1.0f;
						for (Int32 axisIndex = 0; axisIndex < 3; axisIndex++)
						{
							actorAxes[axisIndex].Normalize();
							Float uFacing = std::abs(actorAxes[axisIndex].Dot(axisU));
							Float vFacing = std::abs(actorAxes[axisIndex].Dot(axisV));
							if (uFacing > bestUFacing)
							{
								bestUFacing = uFacing;
								uScaleIndex = axisIndex;
							}
							if (vFacing > bestVFacing)
							{
								bestVFacing = vFacing;
								vScaleIndex = axisIndex;
							}
						}

						Float startScales[3] = { dragStartScales_[index].x, dragStartScales_[index].y, dragStartScales_[index].z };

						Float* positionData = static_cast<Float*>(world.GetComponent(entity, positionID));
						Float* scaleData = static_cast<Float*>(world.GetComponent(entity, scaleID));
						if (positionData)
						{
							positionData[0] = localTranslation.x;
							positionData[1] = localTranslation.y;
							positionData[2] = localTranslation.z;
						}
						if (scaleData)
						{
							scaleData[uScaleIndex] = startScales[uScaleIndex] * rectScale.x;
							if (vScaleIndex != uScaleIndex)
							{
								scaleData[vScaleIndex] = startScales[vScaleIndex] * rectScale.y;
							}
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
					Vector3 handleWorld = origin + axisU * frameX + axisV * frameY;
					Vector4 handleClip = Vector4::Transform(Vector4(handleWorld.x, handleWorld.y, handleWorld.z, 1.0f), viewProjection);
					if (handleClip.w <= 0.0001f)
					{
						rectHandle_ = wasDragging_ ? rectHandle_ : 0;
						return;
					}
					handleScreens[handleIndex] = Vector2(position.x + (handleClip.x / handleClip.w * 0.5f + 0.5f) * size.x, position.y + (0.5f - handleClip.y / handleClip.w * 0.5f) * size.y);
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

						if (rectHandle_ == 0 && mouseOnPlane && mouseFrame.x >= displayBounds.x && mouseFrame.x <= displayBounds.z && mouseFrame.y >= displayBounds.y && mouseFrame.y <= displayBounds.w)
						{
							rectHandle_ = 5;
						}
					}

					if (rectHandle_ != 0 && mouseOnPlane && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						wasDragging_ = true;
						rectDragStart_ = bounds;
						rectDragMouse_ = mouseFrame;
						dragStartPivotMatrix_ = Matrix(axisU.x, axisU.y, axisU.z, 0.0f, axisV.x, axisV.y, axisV.z, 0.0f, axisN.x, axisN.y, axisN.z, 0.0f, origin.x, origin.y, origin.z, 1.0f);

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

	Bool GuizmoPanel3D::RectToolActive()const
	{
		return rectHandle_ != 0;
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
