#include <Editor/Editor/Panel/AnimatorControllerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	namespace
	{
		constexpr uintptr_t nodeIdBase = 0;
		constexpr uintptr_t pinIdBase = 100000;
		constexpr uintptr_t linkIdBase = 300000;

		ed::NodeId StateNodeId(Size index)
		{
			return ed::NodeId(static_cast<uintptr_t>(nodeIdBase + index + 1));
		}

		ed::PinId StatePinId(Size index)
		{
			return ed::PinId(static_cast<uintptr_t>(pinIdBase + index + 1));
		}

		ed::LinkId TransitionLinkId(Size index)
		{
			return ed::LinkId(static_cast<uintptr_t>(linkIdBase + index + 1));
		}

		ed::NodeId EntryNodeId()
		{
			return ed::NodeId(static_cast<uintptr_t>(900001));
		}

		ed::NodeId ExitNodeId()
		{
			return ed::NodeId(static_cast<uintptr_t>(900002));
		}

		ed::PinId EntryPinId()
		{
			return ed::PinId(static_cast<uintptr_t>(900101));
		}

		ed::PinId ExitPinId()
		{
			return ed::PinId(static_cast<uintptr_t>(900102));
		}

		void DrawTransitionArrow(ImDrawList* drawList, const ImVec2& screenFrom, const ImVec2& screenTo, ImU32 color, Float thickness)
		{
			ImVec2 direction(screenTo.x - screenFrom.x, screenTo.y - screenFrom.y);
			Float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
			if (length < 1.0f)
			{
				return;
			}
			direction.x /= length;
			direction.y /= length;

			drawList->AddLine(screenFrom, screenTo, color, thickness);

			ImVec2 perpendicular(-direction.y, direction.x);
			ImVec2 midpoint((screenFrom.x + screenTo.x) * 0.5f, (screenFrom.y + screenTo.y) * 0.5f);

			constexpr Float arrowLength = 12.0f;
			constexpr Float arrowWidth = 8.0f;

			ImVec2 tip(midpoint.x + direction.x * arrowLength * 0.5f, midpoint.y + direction.y * arrowLength * 0.5f);
			ImVec2 back(midpoint.x - direction.x * arrowLength * 0.5f, midpoint.y - direction.y * arrowLength * 0.5f);
			ImVec2 left(back.x + perpendicular.x * arrowWidth * 0.5f, back.y + perpendicular.y * arrowWidth * 0.5f);
			ImVec2 right(back.x - perpendicular.x * arrowWidth * 0.5f, back.y - perpendicular.y * arrowWidth * 0.5f);

			drawList->AddTriangleFilled(tip, left, right, color);
		}

		Float DistanceToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b)
		{
			ImVec2 ab(b.x - a.x, b.y - a.y);
			Float lengthSquared = ab.x * ab.x + ab.y * ab.y;
			Float t = lengthSquared > 0.0f ? ((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / lengthSquared : 0.0f;
			t = std::clamp(t, 0.0f, 1.0f);
			ImVec2 closest(a.x + ab.x * t, a.y + ab.y * t);
			Float dx = point.x - closest.x;
			Float dy = point.y - closest.y;
			return std::sqrt(dx * dx + dy * dy);
		}

		Bool ClipSegmentToRect(const ImVec2& p0, const ImVec2& p1, const ImVec2& rectMin, const ImVec2& rectMax, Float& tEnter, Float& tExit)
		{
			Float dx = p1.x - p0.x;
			Float dy = p1.y - p0.y;
			Float t0 = 0.0f;
			Float t1 = 1.0f;
			Float p[4] = { -dx, dx, -dy, dy };
			Float q[4] = { p0.x - rectMin.x, rectMax.x - p0.x, p0.y - rectMin.y, rectMax.y - p0.y };

			for (Int edgeIndex = 0; edgeIndex < 4; ++edgeIndex)
			{
				if (std::abs(p[edgeIndex]) < 0.0001f)
				{
					if (q[edgeIndex] < 0.0f)
					{
						return false;
					}
				}
				else
				{
					Float r = q[edgeIndex] / p[edgeIndex];
					if (p[edgeIndex] < 0.0f)
					{
						if (r > t1)
						{
							return false;
						}
						if (r > t0)
						{
							t0 = r;
						}
					}
					else
					{
						if (r < t0)
						{
							return false;
						}
						if (r < t1)
						{
							t1 = r;
						}
					}
				}
			}

			tEnter = t0;
			tExit = t1;
			return true;
		}

		ImVec2 ExitPointFromRect(const ImVec2& from, const ImVec2& to, const ImVec2& rectMin, const ImVec2& rectMax)
		{
			Float tEnter, tExit;
			if (ClipSegmentToRect(from, to, rectMin, rectMax, tEnter, tExit))
			{
				return ImVec2(from.x + (to.x - from.x) * tExit, from.y + (to.y - from.y) * tExit);
			}
			return from;
		}

		ImVec2 EntryPointToRect(const ImVec2& from, const ImVec2& to, const ImVec2& rectMin, const ImVec2& rectMax)
		{
			Float tEnter, tExit;
			if (ClipSegmentToRect(from, to, rectMin, rectMax, tEnter, tExit))
			{
				return ImVec2(from.x + (to.x - from.x) * tEnter, from.y + (to.y - from.y) * tEnter);
			}
			return to;
		}

		const Char* ComparisonLabel(AnimationConditionComparison comparison)
		{
			switch (comparison)
			{
			case AnimationConditionComparison::Equal:
				return "=";
			case AnimationConditionComparison::NotEqual:
				return "!=";
			case AnimationConditionComparison::Greater:
				return ">";
			case AnimationConditionComparison::Less:
				return "<";
			case AnimationConditionComparison::GreaterOrEqual:
				return ">=";
			case AnimationConditionComparison::LessOrEqual:
				return "<=";
			default:
				return "Unknown";
			}
		}

		const Char* ParameterTypeLabel(AnimationParameterType type)
		{
			switch (type)
			{
			case AnimationParameterType::Bool:
				return "Bool";
			case AnimationParameterType::Float:
				return "Float";
			case AnimationParameterType::Int:
				return "Int";
			case AnimationParameterType::Trigger:
				return "Trigger";
			default:
				return "Unknown";
			}
		}

		const AnimationParameter* FindParameter(const Animator& animator, const String& name)
		{
			for (const AnimationParameter& parameter : animator.parameters_)
			{
				if (parameter.name_ == name)
				{
					return &parameter;
				}
			}
			return nullptr;
		}

		String MakeUniqueParameterName(const Animator& animator, const String& base)
		{
			if (!FindParameter(animator, base))
			{
				return base;
			}

			for (Int suffix = 1; ; ++suffix)
			{
				std::string candidate = base.str() + std::to_string(suffix);
				if (!FindParameter(animator, String(std::string_view(candidate))))
				{
					return String(std::string_view(candidate));
				}
			}
		}

		std::string AssetLabel(ResourceCache* resource, Int assetId)
		{
			if (assetId == 0)
			{
				return "(未選択)";
			}

			Asset* asset = resource->GetAsset(static_cast<Uint32>(assetId));
			if (!asset)
			{
				return "";
			}

			return std::filesystem::path(asset->path_.c_str()).filename().string();
		}
	}

	AnimatorControllerPanel::AnimatorControllerPanel(EditorContext& context) : context_(context)
	{
		nodeEditorContext_ = ed::CreateEditor();
	}

	AnimatorControllerPanel::~AnimatorControllerPanel()
	{
		ed::DestroyEditor(nodeEditorContext_);
	}

	void AnimatorControllerPanel::Open(Animator* target)
	{
		show_ = true;

		if (target != target_)
		{
			needsPositionSync_ = true;
		}
		target_ = target;
	}

	void AnimatorControllerPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		Animator* selectedTarget = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_->GetComponent<Animator>()) : nullptr;
		if (selectedTarget != target_)
		{
			target_ = selectedTarget;
			needsPositionSync_ = true;
			selectedStateIndex_ = SIZE_MAX;
			selectedTransitionIndex_ = SIZE_MAX;
			selectedConditionIndex_ = SIZE_MAX;
		}

		ImGui::SetNextWindowSize(ImVec2(1100, 650), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("アニメーターコントローラー", &show_))
		{
			if (!target_)
			{
				ImGui::TextDisabled("対象のAnimatorがありません");
			}
			else
			{
				DrawNodeEditor();
			}
		}
		ImGui::End();
	}

	void AnimatorControllerPanel::DrawNodeEditor()
	{
		ed::SetCurrentEditor(nodeEditorContext_);

		ed::GetStyle().LinkStrength = 0.0f;
		ed::GetStyle().NodePadding = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

		Bool syncPositions = needsPositionSync_;
		needsPositionSync_ = false;

		if (ImGui::Button("ステートを追加"))
		{
			AnimationState& state = target_->states_.emplace_back();
			state.name_ = String("State");
			state.nodePositionX_ = 40.0f;
			state.nodePositionY_ = 40.0f + static_cast<Float>(target_->states_.size() - 1) * 160.0f;

			ed::SetNodePosition(StateNodeId(target_->states_.size() - 1), ImVec2(state.nodePositionX_, state.nodePositionY_));
		}

		Float availableHeight = ImGui::GetContentRegionAvail().y;

		ImGui::BeginChild("##graph", ImVec2(-300.0f, availableHeight), true);

		ed::Begin("AnimatorControllerEditor", ImVec2(0, 0));

		for (Size index = 0; index < target_->states_.size(); ++index)
		{
			AnimationState& state = target_->states_[index];

			ed::NodeId nodeId = StateNodeId(index);

			if (syncPositions)
			{
				ed::SetNodePosition(nodeId, ImVec2(state.nodePositionX_, state.nodePositionY_));
			}

			ed::BeginNode(nodeId);
			ImGui::PushID(static_cast<Int>(index));

			ed::BeginPin(StatePinId(index), ed::PinKind::Output);

			ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
			ed::PinPivotSize(ImVec2(-1.0f, -1.0f));

			constexpr Float nodeWidth = 200.0f;

			ImGui::Dummy(ImVec2(nodeWidth, 8.0f));

			Float textWidth = ImGui::CalcTextSize(state.name_.c_str()).x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (nodeWidth - textWidth) * 0.5f));
			ImGui::Text("%s", state.name_.c_str());

			ImGui::Dummy(ImVec2(nodeWidth, 8.0f));
			ed::EndPin();

			ImGui::PopID();
			ed::EndNode();

			ImVec2 currentPosition = ed::GetNodePosition(nodeId);
			state.nodePositionX_ = currentPosition.x;
			state.nodePositionY_ = currentPosition.y;
		}

		if (syncPositions)
		{
			ed::SetNodePosition(EntryNodeId(), ImVec2(target_->entryNodePositionX_, target_->entryNodePositionY_));
			ed::SetNodePosition(ExitNodeId(), ImVec2(target_->exitNodePositionX_, target_->exitNodePositionY_));
		}

		constexpr Float specialNodeWidth = 100.0f;

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.12f, 0.35f, 0.12f, 1.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.35f, 0.9f, 0.35f, 1.0f));
		ed::BeginNode(EntryNodeId());
		ed::BeginPin(EntryPinId(), ed::PinKind::Output);
		ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
		ed::PinPivotSize(ImVec2(-1.0f, -1.0f));
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		Float entryTextWidth = ImGui::CalcTextSize("Entry").x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (specialNodeWidth - entryTextWidth) * 0.5f));
		ImGui::Text("Entry");
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		ed::EndPin();
		ed::EndNode();
		ed::PopStyleColor(2);

		ImVec2 entryPosition = ed::GetNodePosition(EntryNodeId());
		target_->entryNodePositionX_ = entryPosition.x;
		target_->entryNodePositionY_ = entryPosition.y;

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.35f, 0.12f, 0.12f, 1.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ed::BeginNode(ExitNodeId());
		ed::BeginPin(ExitPinId(), ed::PinKind::Output);
		ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
		ed::PinPivotSize(ImVec2(-1.0f, -1.0f));
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		Float exitTextWidth = ImGui::CalcTextSize("Exit").x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (specialNodeWidth - exitTextWidth) * 0.5f));
		ImGui::Text("Exit");
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		ed::EndPin();
		ed::EndNode();
		ed::PopStyleColor(2);

		ImVec2 exitPosition = ed::GetNodePosition(ExitNodeId());
		target_->exitNodePositionX_ = exitPosition.x;
		target_->exitNodePositionY_ = exitPosition.y;

		Bool linkCreatedThisFrame = false;

		if (ed::BeginCreate())
		{
			ed::PinId startPinId, endPinId;
			if (ed::QueryNewLink(&startPinId, &endPinId))
			{
				if (ed::AcceptNewItem())
				{
					uintptr_t startValue = startPinId.Get();
					uintptr_t endValue = endPinId.Get();

					if (startValue == 900101 && endValue >= pinIdBase && endValue < linkIdBase)
					{
						Size toIndex = static_cast<Size>(endValue - pinIdBase - 1);
						if (toIndex < target_->states_.size())
						{
							target_->entryStateIndex_ = static_cast<Int>(toIndex);
							linkCreatedThisFrame = true;
						}
					}
					else if (endValue == 900102 && startValue >= pinIdBase && startValue < linkIdBase)
					{
						Size fromIndex = static_cast<Size>(startValue - pinIdBase - 1);
						if (fromIndex < target_->states_.size())
						{
							AnimationTransition& transition = target_->transitions_.emplace_back();
							transition.fromState_ = static_cast<Int>(fromIndex);
							transition.toState_ = Animator::ExitState;

							if (pendingFromStateIndex_ == fromIndex)
							{
								transition.fromOffsetX_ = pendingFromOffsetX_;
								transition.fromOffsetY_ = pendingFromOffsetY_;
							}

							ImVec2 toCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
							ImVec2 toNodePos = ed::GetNodePosition(ExitNodeId());
							transition.toOffsetX_ = toCanvasPos.x - toNodePos.x;
							transition.toOffsetY_ = toCanvasPos.y - toNodePos.y;

							linkCreatedThisFrame = true;
						}
					}
					else if (startValue >= pinIdBase && startValue < linkIdBase &&
						endValue >= pinIdBase && endValue < linkIdBase)
					{
						Size fromIndex = static_cast<Size>(startValue - pinIdBase - 1);
						Size toIndex = static_cast<Size>(endValue - pinIdBase - 1);

						AnimationTransition& transition = target_->transitions_.emplace_back();
						transition.fromState_ = static_cast<Int>(fromIndex);
						transition.toState_ = static_cast<Int>(toIndex);

						if (pendingFromStateIndex_ == fromIndex)
						{
							transition.fromOffsetX_ = pendingFromOffsetX_;
							transition.fromOffsetY_ = pendingFromOffsetY_;
						}

						ImVec2 toCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
						ImVec2 toNodePos = ed::GetNodePosition(StateNodeId(toIndex));
						transition.toOffsetX_ = toCanvasPos.x - toNodePos.x;
						transition.toOffsetY_ = toCanvasPos.y - toNodePos.y;

						linkCreatedThisFrame = true;
					}
				}
			}
		}
		ed::EndCreate();

		if (ed::BeginDelete())
		{
			ed::NodeId deletedNodeId;
			while (ed::QueryDeletedNode(&deletedNodeId))
			{
				if (deletedNodeId == EntryNodeId() || deletedNodeId == ExitNodeId())
				{
					ed::RejectDeletedItem();
					continue;
				}

				if (ed::AcceptDeletedItem())
				{
					uintptr_t value = deletedNodeId.Get();
					if (value >= nodeIdBase)
					{
						Size stateIndex = static_cast<Size>(value - nodeIdBase - 1);
						if (stateIndex < target_->states_.size())
						{
							target_->states_.erase(target_->states_.begin() + stateIndex);

							Bool removedTransition = false;
							for (Size transitionIndex = target_->transitions_.size(); transitionIndex > 0; --transitionIndex)
							{
								AnimationTransition& transition = target_->transitions_[transitionIndex - 1];
								if (transition.fromState_ == static_cast<Int>(stateIndex) || transition.toState_ == static_cast<Int>(stateIndex))
								{
									target_->transitions_.erase(target_->transitions_.begin() + (transitionIndex - 1));
									removedTransition = true;
								}
							}

							if (removedTransition)
							{
								selectedTransitionIndex_ = SIZE_MAX;
								selectedConditionIndex_ = SIZE_MAX;
							}

							if (selectedStateIndex_ == stateIndex)
							{
								selectedStateIndex_ = SIZE_MAX;
							}
							else if (selectedStateIndex_ != SIZE_MAX && selectedStateIndex_ > stateIndex)
							{
								--selectedStateIndex_;
							}

							if (middleDragStateIndex_ == stateIndex)
							{
								middleDragStateIndex_ = SIZE_MAX;
							}
							else if (middleDragStateIndex_ != SIZE_MAX && middleDragStateIndex_ > stateIndex)
							{
								--middleDragStateIndex_;
							}
						}
					}
				}
			}
		}
		ed::EndDelete();

		ed::End();

		ed::NodeId hoveredNodeId = ed::GetHoveredNode();
		ed::PinId hoveredPinId = ed::GetHoveredPin();

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			uintptr_t pinValue = hoveredPinId.Get();
			if (pinValue >= pinIdBase && pinValue < linkIdBase)
			{
				Size candidate = static_cast<Size>(pinValue - pinIdBase - 1);
				if (candidate < target_->states_.size())
				{
					ImVec2 canvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
					ImVec2 nodePos = ed::GetNodePosition(StateNodeId(candidate));
					pendingFromStateIndex_ = candidate;
					pendingFromOffsetX_ = canvasPos.x - nodePos.x;
					pendingFromOffsetY_ = canvasPos.y - nodePos.y;
				}
			}
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			if (hoveredNodeId == EntryNodeId())
			{
				middleDraggingEntry_ = true;
			}
			else if (hoveredNodeId == ExitNodeId())
			{
				middleDraggingExit_ = true;
			}
			else
			{
				uintptr_t value = hoveredNodeId.Get();
				if (value >= nodeIdBase + 1)
				{
					Size candidate = static_cast<Size>(value - nodeIdBase - 1);
					if (candidate < target_->states_.size())
					{
						middleDragStateIndex_ = candidate;
					}
				}
			}
		}

		if (middleDraggingEntry_ || middleDraggingExit_)
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
			{
				ed::NodeId dragNodeId = middleDraggingEntry_ ? EntryNodeId() : ExitNodeId();
				ImVec2 currentPos = ed::GetNodePosition(dragNodeId);
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				Float zoom = ed::GetCurrentZoom();
				ImVec2 newPos(currentPos.x + delta.x / zoom, currentPos.y + delta.y / zoom);

				ed::SetNodePosition(dragNodeId, newPos);

				if (middleDraggingEntry_)
				{
					target_->entryNodePositionX_ = newPos.x;
					target_->entryNodePositionY_ = newPos.y;
				}
				else
				{
					target_->exitNodePositionX_ = newPos.x;
					target_->exitNodePositionY_ = newPos.y;
				}
			}

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
			{
				middleDraggingEntry_ = false;
				middleDraggingExit_ = false;
			}
		}
		else if (middleDragStateIndex_ != SIZE_MAX)
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) && middleDragStateIndex_ < target_->states_.size())
			{
				ed::NodeId dragNodeId = StateNodeId(middleDragStateIndex_);
				ImVec2 currentPos = ed::GetNodePosition(dragNodeId);
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				Float zoom = ed::GetCurrentZoom();
				ImVec2 newPos(currentPos.x + delta.x / zoom, currentPos.y + delta.y / zoom);

				ed::SetNodePosition(dragNodeId, newPos);

				AnimationState& draggedState = target_->states_[middleDragStateIndex_];
				draggedState.nodePositionX_ = newPos.x;
				draggedState.nodePositionY_ = newPos.y;
			}

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
			{
				middleDragStateIndex_ = SIZE_MAX;
			}
		}

		Bool releasedOnEmpty = ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !linkCreatedThisFrame && ImGui::IsWindowHovered();
		ImVec2 releasePos = ImGui::GetMousePos();
		constexpr Float transitionHitDistance = 6.0f;
		Size hitTransitionIndex = SIZE_MAX;
		Float hitTransitionDistance = transitionHitDistance;

		ImDrawList* graphDrawList = ImGui::GetWindowDrawList();
		for (Size index = 0; index < target_->transitions_.size(); ++index)
		{
			const AnimationTransition& transition = target_->transitions_[index];
			if (transition.fromState_ < 0)
			{
				continue;
			}

			Bool toIsExit = (transition.toState_ == Animator::ExitState);

			Size fromIndex = static_cast<Size>(transition.fromState_);
			if (fromIndex >= target_->states_.size())
			{
				continue;
			}

			Size toIndex = 0;
			if (!toIsExit)
			{
				if (transition.toState_ < 0)
				{
					continue;
				}
				toIndex = static_cast<Size>(transition.toState_);
				if (toIndex >= target_->states_.size() || fromIndex == toIndex)
				{
					continue;
				}
			}

			ed::NodeId toNodeIdForDraw = toIsExit ? ExitNodeId() : StateNodeId(toIndex);

			ImVec2 fromNodePos = ed::GetNodePosition(StateNodeId(fromIndex));
			ImVec2 fromNodeSize = ed::GetNodeSize(StateNodeId(fromIndex));
			ImVec2 toNodePos = ed::GetNodePosition(toNodeIdForDraw);
			ImVec2 toNodeSize = ed::GetNodeSize(toNodeIdForDraw);

			ImVec2 fromRectMax(fromNodePos.x + fromNodeSize.x, fromNodePos.y + fromNodeSize.y);
			ImVec2 toRectMax(toNodePos.x + toNodeSize.x, toNodePos.y + toNodeSize.y);

			ImVec2 fromPoint(fromNodePos.x + transition.fromOffsetX_, fromNodePos.y + transition.fromOffsetY_);
			ImVec2 toPoint(toNodePos.x + transition.toOffsetX_, toNodePos.y + transition.toOffsetY_);

			ImVec2 clippedFrom = ExitPointFromRect(fromPoint, toPoint, fromNodePos, fromRectMax);
			ImVec2 clippedTo = EntryPointToRect(fromPoint, toPoint, toNodePos, toRectMax);

			ImVec2 screenFrom = ed::CanvasToScreen(clippedFrom);
			ImVec2 screenTo = ed::CanvasToScreen(clippedTo);

			ImVec2 direction(screenTo.x - screenFrom.x, screenTo.y - screenFrom.y);
			Float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
			if (length < 1.0f)
			{
				continue;
			}
			direction.x /= length;
			direction.y /= length;

			if (releasedOnEmpty)
			{
				Float distance = DistanceToSegment(releasePos, screenFrom, screenTo);
				if (distance < hitTransitionDistance)
				{
					hitTransitionDistance = distance;
					hitTransitionIndex = index;
				}
			}

			ImVec2 perpendicular(-direction.y, direction.x);
			ImVec2 midpoint((screenFrom.x + screenTo.x) * 0.5f, (screenFrom.y + screenTo.y) * 0.5f);

			Bool isSelected = (selectedTransitionIndex_ == index);
			ImU32 lineColor = isSelected ? IM_COL32(255, 176, 50, 255) : (toIsExit ? IM_COL32(230, 90, 90, 200) : IM_COL32(255, 255, 255, 180));

			graphDrawList->AddLine(screenFrom, screenTo, lineColor, isSelected ? 3.0f : 2.0f);

			constexpr Float arrowLength = 12.0f;
			constexpr Float arrowWidth = 8.0f;

			ImVec2 tip(midpoint.x + direction.x * arrowLength * 0.5f, midpoint.y + direction.y * arrowLength * 0.5f);
			ImVec2 back(midpoint.x - direction.x * arrowLength * 0.5f, midpoint.y - direction.y * arrowLength * 0.5f);
			ImVec2 left(back.x + perpendicular.x * arrowWidth * 0.5f, back.y + perpendicular.y * arrowWidth * 0.5f);
			ImVec2 right(back.x - perpendicular.x * arrowWidth * 0.5f, back.y - perpendicular.y * arrowWidth * 0.5f);

			graphDrawList->AddTriangleFilled(tip, left, right, lineColor);
		}

		if (target_->entryStateIndex_ >= 0 && static_cast<Size>(target_->entryStateIndex_) < target_->states_.size())
		{
			Size entryTargetIndex = static_cast<Size>(target_->entryStateIndex_);

			ImVec2 entryNodePos = ed::GetNodePosition(EntryNodeId());
			ImVec2 entryNodeSize = ed::GetNodeSize(EntryNodeId());
			ImVec2 targetNodePos = ed::GetNodePosition(StateNodeId(entryTargetIndex));
			ImVec2 targetNodeSize = ed::GetNodeSize(StateNodeId(entryTargetIndex));

			ImVec2 entryRectMax(entryNodePos.x + entryNodeSize.x, entryNodePos.y + entryNodeSize.y);
			ImVec2 targetRectMax(targetNodePos.x + targetNodeSize.x, targetNodePos.y + targetNodeSize.y);

			ImVec2 entryCenter(entryNodePos.x + entryNodeSize.x * 0.5f, entryNodePos.y + entryNodeSize.y * 0.5f);
			ImVec2 targetCenter(targetNodePos.x + targetNodeSize.x * 0.5f, targetNodePos.y + targetNodeSize.y * 0.5f);

			ImVec2 clippedFrom = ExitPointFromRect(entryCenter, targetCenter, entryNodePos, entryRectMax);
			ImVec2 clippedTo = EntryPointToRect(entryCenter, targetCenter, targetNodePos, targetRectMax);

			ImVec2 screenFrom = ed::CanvasToScreen(clippedFrom);
			ImVec2 screenTo = ed::CanvasToScreen(clippedTo);

			ImVec2 direction(screenTo.x - screenFrom.x, screenTo.y - screenFrom.y);
			Float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
			if (length >= 1.0f)
			{
				direction.x /= length;
				direction.y /= length;

				ImU32 entryLineColor = IM_COL32(100, 220, 100, 220);
				DrawTransitionArrow(graphDrawList, screenFrom, screenTo, entryLineColor, 2.0f);
			}
		}

		if (releasedOnEmpty)
		{
			if (hitTransitionIndex != SIZE_MAX)
			{
				if (selectedTransitionIndex_ != hitTransitionIndex)
				{
					selectedConditionIndex_ = SIZE_MAX;
				}
				selectedTransitionIndex_ = hitTransitionIndex;
				selectedStateIndex_ = SIZE_MAX;
			}
			else
			{
				selectedTransitionIndex_ = SIZE_MAX;
				selectedConditionIndex_ = SIZE_MAX;

				uintptr_t value = hoveredNodeId.Get();
				if (value >= nodeIdBase + 1)
				{
					Size candidate = static_cast<Size>(value - nodeIdBase - 1);
					if (candidate < target_->states_.size())
					{
						selectedStateIndex_ = candidate;
					}
				}
			}
		}

		if (selectedTransitionIndex_ != SIZE_MAX && selectedTransitionIndex_ < target_->transitions_.size() &&
			ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			target_->transitions_.erase(target_->transitions_.begin() + selectedTransitionIndex_);
			selectedTransitionIndex_ = SIZE_MAX;
			selectedConditionIndex_ = SIZE_MAX;
		}

		ed::SetCurrentEditor(nullptr);

		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##details", ImVec2(0, availableHeight), true);
		ImGui::TextDisabled("パラメータ");
		ImGui::Separator();

		{
			Size removeParameterIndex = SIZE_MAX;

			for (Size parameterIndex = 0; parameterIndex < target_->parameters_.size(); ++parameterIndex)
			{
				AnimationParameter& parameter = target_->parameters_[parameterIndex];

				ImGui::PushID(static_cast<Int>(parameterIndex));

				std::string parameterNameBuffer = parameter.name_.str();
				parameterNameBuffer.resize(64);
				ImGui::SetNextItemWidth(100.0f);
				if (ImGui::InputText("##parameterName", parameterNameBuffer.data(), parameterNameBuffer.capacity()))
				{
					parameter.name_ = String(std::string_view(parameterNameBuffer.c_str()));
				}

				ImGui::SameLine();

				ImGui::SetNextItemWidth(80.0f);
				if (ImGui::BeginCombo("##parameterType", ParameterTypeLabel(parameter.type_)))
				{
					for (Int typeValue = 0; typeValue <= static_cast<Int>(AnimationParameterType::Trigger); ++typeValue)
					{
						AnimationParameterType type = static_cast<AnimationParameterType>(typeValue);
						Bool selected = (parameter.type_ == type);
						if (ImGui::Selectable(ParameterTypeLabel(type), selected))
						{
							parameter.type_ = type;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();

				if (ImGui::SmallButton("-"))
				{
					removeParameterIndex = parameterIndex;
				}

				ImGui::PopID();
			}

			if (removeParameterIndex != SIZE_MAX)
			{
				target_->parameters_.erase(target_->parameters_.begin() + removeParameterIndex);
			}

			if (ImGui::Button("パラメータを追加", ImVec2(-1.0f, 0.0f)))
			{
				String uniqueName = MakeUniqueParameterName(*target_, "Parameter");
				AnimationParameter& newParameter = target_->parameters_.emplace_back();
				newParameter.name_ = uniqueName;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("詳細");
		ImGui::Separator();

		if (selectedTransitionIndex_ != SIZE_MAX && selectedTransitionIndex_ < target_->transitions_.size())
		{
			AnimationTransition& transition = target_->transitions_[selectedTransitionIndex_];

			Size fromIndex = static_cast<Size>(transition.fromState_);
			Size toIndex = static_cast<Size>(transition.toState_);
			const Char* fromName = fromIndex < target_->states_.size() ? target_->states_[fromIndex].name_.c_str() : "?";
			const Char* toName = toIndex < target_->states_.size() ? target_->states_[toIndex].name_.c_str() : "?";

			ImGui::Text("%s から %s", fromName, toName);
			ImGui::Spacing();

			ImGui::DragFloat("遷移時間", &transition.duration_, 0.01f, 0.0f, FLT_MAX);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextDisabled("条件");

			Size removeIndex = SIZE_MAX;

			for (Size conditionIndex = 0; conditionIndex < transition.conditions_.size(); ++conditionIndex)
			{
				AnimationCondition& condition = transition.conditions_[conditionIndex];

				ImGui::PushID(static_cast<Int>(conditionIndex));

				Bool isSelected = (selectedConditionIndex_ == conditionIndex);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.35f, 0.25f, 0.1f, 0.4f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, isSelected ? ImVec4(1.0f, 0.69f, 0.2f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.25f));

				Float boxHeight = ImGui::GetFrameHeightWithSpacing() * (conditionIndex > 0 ? 4.0f : 3.0f) + 8.0f;
				ImGui::BeginChild("##conditionBox", ImVec2(0.0f, boxHeight), true);

				if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					selectedConditionIndex_ = conditionIndex;
				}
				if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					selectedConditionIndex_ = conditionIndex;
					ImGui::OpenPopup("##conditionContext");
				}
				if (ImGui::BeginPopup("##conditionContext"))
				{
					if (ImGui::MenuItem("削除"))
					{
						removeIndex = conditionIndex;
					}
					ImGui::EndPopup();
				}
				if (isSelected && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
				{
					removeIndex = conditionIndex;
				}

				if (conditionIndex > 0)
				{
					const Char* logicLabel = condition.isOr_ ? "OR" : "AND";
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::BeginCombo("##logic", logicLabel))
					{
						if (ImGui::Selectable("AND", !condition.isOr_))
						{
							condition.isOr_ = false;
						}
						if (ImGui::Selectable("OR", condition.isOr_))
						{
							condition.isOr_ = true;
						}
						ImGui::EndCombo();
					}
				}

				std::string parameterPreview = condition.parameterName_.str();
				if (parameterPreview.empty())
				{
					parameterPreview = "(未選択)";
				}

				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::BeginCombo("##parameter", parameterPreview.c_str()))
				{
					for (const AnimationParameter& parameter : target_->parameters_)
					{
						std::string parameterName = parameter.name_.str();
						Bool selected = (condition.parameterName_ == parameter.name_);
						if (ImGui::Selectable(parameterName.c_str(), selected))
						{
							condition.parameterName_ = parameter.name_;
						}
					}
					ImGui::EndCombo();
				}

				const AnimationParameter* parameter = FindParameter(*target_, condition.parameterName_);
				AnimationParameterType parameterType = parameter ? parameter->type_ : AnimationParameterType::Float;

				if (parameterType != AnimationParameterType::Trigger)
				{
					if (parameterType == AnimationParameterType::Bool)
					{
						Bool boolValue = condition.value_ != 0.0f;
						if (ImGui::Checkbox("値", &boolValue))
						{
							condition.value_ = boolValue ? 1.0f : 0.0f;
							condition.comparison_ = AnimationConditionComparison::Equal;
						}
					}
					else
					{
						ImGui::SetNextItemWidth(120.0f);
						if (ImGui::BeginCombo("##comparison", ComparisonLabel(condition.comparison_)))
						{
							for (Int comparisonValue = 0; comparisonValue <= static_cast<Int>(AnimationConditionComparison::LessOrEqual); ++comparisonValue)
							{
								AnimationConditionComparison comparison = static_cast<AnimationConditionComparison>(comparisonValue);
								Bool selected = (condition.comparison_ == comparison);
								if (ImGui::Selectable(ComparisonLabel(comparison), selected))
								{
									condition.comparison_ = comparison;
								}
							}
							ImGui::EndCombo();
						}

						ImGui::SetNextItemWidth(120.0f);
						if (parameterType == AnimationParameterType::Int)
						{
							Int intValue = static_cast<Int>(condition.value_);
							if (ImGui::DragInt("##value", &intValue, 1.0f))
							{
								condition.value_ = static_cast<Float>(intValue);
							}
						}
						else
						{
							ImGui::DragFloat("##value", &condition.value_, 0.01f);
						}
					}
				}

				ImGui::EndChild();

				ImGui::PopStyleColor(2);

				ImGui::PopID();
			}

			if (removeIndex != SIZE_MAX)
			{
				transition.conditions_.erase(transition.conditions_.begin() + removeIndex);
				if (selectedConditionIndex_ == removeIndex)
				{
					selectedConditionIndex_ = SIZE_MAX;
				}
			}

			if (ImGui::Button("条件を追加", ImVec2(-1.0f, 0.0f)))
			{
				transition.conditions_.emplace_back();
			}
		}
		else if (selectedStateIndex_ != SIZE_MAX && selectedStateIndex_ < target_->states_.size())
		{
			AnimationState& state = target_->states_[selectedStateIndex_];

			std::string nameBuffer = state.name_.str();
			nameBuffer.resize(128);
			if (ImGui::InputText("名前", nameBuffer.data(), nameBuffer.capacity()))
			{
				state.name_ = String(std::string_view(nameBuffer.c_str()));
			}
			ImGui::Spacing();

			std::string preview = AssetLabel(context_.worldContext_.resource_, state.animationID_);
			if (ImGui::BeginCombo("Animation", preview.c_str()))
			{
				for (Uint32 animationId : target_->animationIDs_)
				{
					std::string label = AssetLabel(context_.worldContext_.resource_, static_cast<Int>(animationId));
					Bool selected = (state.animationID_ == static_cast<Int>(animationId));
					if (ImGui::Selectable(label.c_str(), selected))
					{
						state.animationID_ = static_cast<Int>(animationId);
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Spacing();

			ImGui::Checkbox("ルートモーションを使用", &state.useRootMotion_);
			ImGui::Checkbox("IKを使用", &state.useIK_);
		}
		else
		{
			ImGui::TextDisabled("ノードを選択してください");
		}

		ImGui::EndChild();
	}
}
