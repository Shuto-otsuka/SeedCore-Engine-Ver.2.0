#include <Editor/Editor/Panel/AnimatorControllerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
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

		ed::PinId ExitPinId()
		{
			return ed::PinId(static_cast<uintptr_t>(900102));
		}

		ed::PinId AnyPinId()
		{
			return ed::PinId(static_cast<uintptr_t>(900103));
		}

		ed::NodeId EntryNodeId()
		{
			return ed::NodeId(static_cast<uintptr_t>(900001));
		}

		ed::NodeId ExitNodeId()
		{
			return ed::NodeId(static_cast<uintptr_t>(900002));
		}

		ed::NodeId AnyNodeId()
		{
			return ed::NodeId(static_cast<uintptr_t>(900003));
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
		/// [EN] The library binds the right button to both canvas navigation and
		///      the context menu by default, and navigation consumes the click
		///      before the menu can open. Panning moves to the middle button so
		///      the right button is free for menus.
		/// [JP] ライブラリの既定では右ボタンがキャンバスのパン操作とコンテキスト
		///      メニューの両方に割り当てられており、パン側がクリックを消費して
		///      メニューが開けない。パンを中ボタンへ移し、右ボタンをメニュー
		///      専用にする。
		ed::Config config;
		config.NavigateButtonIndex = 2;

		nodeEditorContext_ = ed::CreateEditor(&config);
	}

	AnimatorControllerPanel::~AnimatorControllerPanel()
	{
		ed::DestroyEditor(nodeEditorContext_);
	}

	void AnimatorControllerPanel::Open(Animator* target)
	{
		show_ = true;
		ImGui::SetWindowFocus("アニメーターコントローラー");

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
			isFocused_ = false;
			return;
		}

		Animator* selectedTarget = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_.GetComponent<Animator>()) : nullptr;
		if (selectedTarget != target_)
		{
			target_ = selectedTarget;
			needsPositionSync_ = true;
			selectedStateIndex_ = SIZE_MAX;
			selectedTransitionIndex_ = SIZE_MAX;
			selectedConditionIndex_ = SIZE_MAX;
		}

		ImGui::DockBuilderDockWindow("アニメーターコントローラー", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1100, 650), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("アニメーターコントローラー", &show_);
		if (isFocused_)
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

		ImGui::BeginChild("##graph", ImVec2(0.0f, availableHeight), true);

		ed::Begin("AnimatorControllerEditor", ImVec2(0, 0));

		/// [EN] A node covered by a pin is dragged as a link by the editor instead of
		///      being moved, which is exactly the split we want: hold Alt and the whole
		///      node becomes a pin so left-drag pulls a transition, release it and the
		///      node is bare so left-drag moves it. The latch keeps the pins alive once
		///      a drag is under way, so letting go of Alt mid-drag does not delete the
		///      pin out from under the editor.
		/// [JP] ピンで覆われたノードはエディタ側で移動ではなくリンクのドラッグとして
		///      扱われる。これがまさに欲しい切り分けで、Alt を押している間だけノード
		///      全体をピンにすれば左ドラッグは遷移を引く操作になり、離せば素のノード
		///      に戻って左ドラッグは移動になる。ラッチはドラッグ開始後ピンを維持する
		///      ためのもので、ドラッグ途中で Alt を離してもエディタの足元からピンが
		///      消えないようにする。
		if (ImGui::GetIO().KeyAlt)
		{
			altPinsActive_ = true;
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			altPinsActive_ = false;
		}

		/// [EN] Kept as an absolute canvas position because the node the drag started
		///      from is only known once the link is accepted; it is turned into a
		///      node-relative anchor there.
		/// [JP] ドラッグの開始ノードはリンクが確定するまで判らないため、絶対キャンバス
		///      座標のまま保持し、確定時にノード相対のアンカーへ変換する。
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt)
		{
			ImVec2 pressCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
			altDragOffsetX_ = pressCanvasPos.x;
			altDragOffsetY_ = pressCanvasPos.y;
		}

		for (Size index = 0; index < target_->states_.size(); ++index)
		{
			AnimationState& state = target_->states_[index];

			ed::NodeId nodeId = StateNodeId(index);

			if (syncPositions)
			{
				ed::SetNodePosition(nodeId, ImVec2(state.nodePositionX_, state.nodePositionY_));
			}

			Bool isDefaultState = (target_->entryStateIndex_ == static_cast<Int>(index));
			if (isDefaultState)
			{
				ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.62f, 0.35f, 0.08f, 1.0f));
				ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(1.0f, 0.68f, 0.22f, 1.0f));
			}

			ed::BeginNode(nodeId);
			ImGui::PushID(static_cast<Int>(index));

			if (altPinsActive_)
			{
				ed::BeginPin(StatePinId(index), ed::PinKind::Output);
				ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
				ed::PinPivotSize(ImVec2(-1.0f, -1.0f));
			}

			constexpr Float nodeWidth = 200.0f;

			ImGui::Dummy(ImVec2(nodeWidth, 8.0f));

			Float textWidth = ImGui::CalcTextSize(state.name_.c_str()).x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (nodeWidth - textWidth) * 0.5f));
			ImGui::Text("%s", state.name_.c_str());

			ImGui::Dummy(ImVec2(nodeWidth, 8.0f));

			if (altPinsActive_)
			{
				ed::EndPin();
			}

			ImGui::PopID();
			ed::EndNode();

			if (isDefaultState)
			{
				ed::PopStyleColor(2);
			}

			ImVec2 currentPosition = ed::GetNodePosition(nodeId);
			state.nodePositionX_ = currentPosition.x;
			state.nodePositionY_ = currentPosition.y;
		}

		if (syncPositions)
		{
			ed::SetNodePosition(EntryNodeId(), ImVec2(target_->entryNodePositionX_, target_->entryNodePositionY_));
			ed::SetNodePosition(ExitNodeId(), ImVec2(target_->exitNodePositionX_, target_->exitNodePositionY_));
			ed::SetNodePosition(AnyNodeId(), ImVec2(target_->anyNodePositionX_, target_->anyNodePositionY_));
		}

		constexpr Float specialNodeWidth = 100.0f;

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.12f, 0.35f, 0.12f, 1.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.35f, 0.9f, 0.35f, 1.0f));
		ed::BeginNode(EntryNodeId());
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		Float entryTextWidth = ImGui::CalcTextSize("Entry").x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (specialNodeWidth - entryTextWidth) * 0.5f));
		ImGui::Text("Entry");
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		ed::EndNode();
		ed::PopStyleColor(2);

		ImVec2 entryPosition = ed::GetNodePosition(EntryNodeId());
		target_->entryNodePositionX_ = entryPosition.x;
		target_->entryNodePositionY_ = entryPosition.y;

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.35f, 0.12f, 0.12f, 1.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
		ed::BeginNode(ExitNodeId());
		if (altPinsActive_)
		{
			ed::BeginPin(ExitPinId(), ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
			ed::PinPivotSize(ImVec2(-1.0f, -1.0f));
		}
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		Float exitTextWidth = ImGui::CalcTextSize("Exit").x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (specialNodeWidth - exitTextWidth) * 0.5f));
		ImGui::Text("Exit");
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		if (altPinsActive_)
		{
			ed::EndPin();
		}
		ed::EndNode();
		ed::PopStyleColor(2);

		ImVec2 exitPosition = ed::GetNodePosition(ExitNodeId());
		target_->exitNodePositionX_ = exitPosition.x;
		target_->exitNodePositionY_ = exitPosition.y;

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.12f, 0.16f, 0.35f, 1.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.35f, 0.55f, 0.9f, 1.0f));
		ed::BeginNode(AnyNodeId());
		if (altPinsActive_)
		{
			ed::BeginPin(AnyPinId(), ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(0.0f, 0.0f));
			ed::PinPivotSize(ImVec2(-1.0f, -1.0f));
		}
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		Float anyTextWidth = ImGui::CalcTextSize("Any").x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (specialNodeWidth - anyTextWidth) * 0.5f));
		ImGui::Text("Any");
		ImGui::Dummy(ImVec2(specialNodeWidth, 8.0f));
		if (altPinsActive_)
		{
			ed::EndPin();
		}
		ed::EndNode();
		ed::PopStyleColor(2);

		ImVec2 anyPosition = ed::GetNodePosition(AnyNodeId());
		target_->anyNodePositionX_ = anyPosition.x;
		target_->anyNodePositionY_ = anyPosition.y;

		if (ed::BeginCreate())
		{
			ed::PinId startPinId, endPinId;
			if (ed::QueryNewLink(&startPinId, &endPinId))
			{
				if (ed::AcceptNewItem())
				{
					uintptr_t startValue = startPinId.Get();
					uintptr_t endValue = endPinId.Get();

					Int fromState = -1;
					Int toState = -1;

					if (startValue == 900103)
					{
						fromState = Animator::AnyState;
					}
					else if (startValue >= pinIdBase && startValue < linkIdBase)
					{
						Size candidate = static_cast<Size>(startValue - pinIdBase - 1);
						if (candidate < target_->states_.size())
						{
							fromState = static_cast<Int>(candidate);
						}
					}

					if (endValue == 900102)
					{
						toState = Animator::ExitState;
					}
					else if (endValue >= pinIdBase && endValue < linkIdBase)
					{
						Size candidate = static_cast<Size>(endValue - pinIdBase - 1);
						if (candidate < target_->states_.size())
						{
							toState = static_cast<Int>(candidate);
						}
					}

					if (fromState != -1 && toState != -1 && fromState != toState)
					{
						AnimationTransition& transition = target_->transitions_.emplace_back();
						transition.fromState_ = fromState;
						transition.toState_ = toState;

						ed::NodeId fromNodeId = (fromState == Animator::AnyState) ? AnyNodeId() : StateNodeId(static_cast<Size>(fromState));
						ImVec2 fromNodePos = ed::GetNodePosition(fromNodeId);
						ImVec2 fromNodeSize = ed::GetNodeSize(fromNodeId);

						/// [EN] The press position is only meaningful when it actually
						///      landed on the node the link starts from; if Alt went down
						///      after the button, no press was recorded and the stale value
						///      would produce a wild anchor. Centre it in that case.
						/// [JP] 記録した押下位置は、それがリンクの始点ノード上だった場合
						///      にのみ意味を持つ。ボタンより後に Alt を押した場合は押下が
						///      記録されず、古い値のまま出鱈目なアンカーになる。その場合は
						///      中心に置く。
						Bool pressLandedOnSource = altDragOffsetX_ >= fromNodePos.x && altDragOffsetX_ <= fromNodePos.x + fromNodeSize.x &&
							altDragOffsetY_ >= fromNodePos.y && altDragOffsetY_ <= fromNodePos.y + fromNodeSize.y;

						transition.fromOffsetX_ = pressLandedOnSource ? (altDragOffsetX_ - fromNodePos.x) : (fromNodeSize.x * 0.5f);
						transition.fromOffsetY_ = pressLandedOnSource ? (altDragOffsetY_ - fromNodePos.y) : (fromNodeSize.y * 0.5f);

						ed::NodeId toNodeId = (toState == Animator::ExitState) ? ExitNodeId() : StateNodeId(static_cast<Size>(toState));
						ImVec2 toCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
						ImVec2 toNodePos = ed::GetNodePosition(toNodeId);
						transition.toOffsetX_ = toCanvasPos.x - toNodePos.x;
						transition.toOffsetY_ = toCanvasPos.y - toNodePos.y;
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
				if (deletedNodeId == EntryNodeId() || deletedNodeId == ExitNodeId() || deletedNodeId == AnyNodeId())
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

							/// [EN] Every state after the erased one shifts down an index,
							///      so its StateNodeId changes and it would otherwise
							///      inherit the editor-side position stored against the
							///      previous owner of that id. Re-pushing all positions
							///      from the state data next frame keeps the authored
							///      layout intact.
							/// [JP] 削除位置より後ろのステートはインデックスが1つ繰り上がる
							///      ため StateNodeId が変わり、その id を以前使っていた
							///      ノードのエディタ側位置を引き継いでしまう。次フレームで
							///      ステートデータから全位置を押し直し、作成時のレイアウトを
							///      保つ。
							needsPositionSync_ = true;

							Bool removedTransition = SeedCore::erase_if(target_->transitions_, [stateIndex](const AnimationTransition& transition) { return transition.fromState_ == static_cast<Int>(stateIndex) || transition.toState_ == static_cast<Int>(stateIndex); }) > 0;

							for (AnimationTransition& transition : target_->transitions_)
							{
								if (transition.fromState_ > static_cast<Int>(stateIndex))
								{
									--transition.fromState_;
								}
								if (transition.toState_ > static_cast<Int>(stateIndex))
								{
									--transition.toState_;
								}
							}

							if (target_->entryStateIndex_ == static_cast<Int>(stateIndex))
							{
								target_->entryStateIndex_ = -1;
							}
							else if (target_->entryStateIndex_ > static_cast<Int>(stateIndex))
							{
								--target_->entryStateIndex_;
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

							if (creatingTransition_ && creatingTransitionSource_ >= 0)
							{
								if (creatingTransitionSource_ == static_cast<Int>(stateIndex))
								{
									creatingTransition_ = false;
									creatingTransitionArmed_ = false;
									creatingTransitionSource_ = -1;
								}
								else if (creatingTransitionSource_ > static_cast<Int>(stateIndex))
								{
									--creatingTransitionSource_;
								}
							}
						}
					}
				}
			}
		}
		ed::EndDelete();

		ed::Suspend();

		ed::NodeId contextNodeId;
		if (ed::ShowNodeContextMenu(&contextNodeId))
		{
			ImVec2 contextCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
			contextMenuCanvasX_ = contextCanvasPos.x;
			contextMenuCanvasY_ = contextCanvasPos.y;

			contextMenuSource_ = -1;
			if (contextNodeId == AnyNodeId())
			{
				contextMenuSource_ = Animator::AnyState;
			}
			else if (contextNodeId != EntryNodeId() && contextNodeId != ExitNodeId())
			{
				uintptr_t value = contextNodeId.Get();
				if (value >= nodeIdBase + 1)
				{
					Size candidate = static_cast<Size>(value - nodeIdBase - 1);
					if (candidate < target_->states_.size())
					{
						contextMenuSource_ = static_cast<Int>(candidate);
					}
				}
			}

			if (contextMenuSource_ != -1)
			{
				ImGui::OpenPopup("##nodeContext");
			}
		}
		else if (ed::ShowBackgroundContextMenu())
		{
			ImVec2 contextCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
			contextMenuCanvasX_ = contextCanvasPos.x;
			contextMenuCanvasY_ = contextCanvasPos.y;

			ImGui::OpenPopup("##backgroundContext");
		}

		if (ImGui::BeginPopup("##backgroundContext"))
		{
			if (ImGui::MenuItem("ステートを追加"))
			{
				AnimationState& state = target_->states_.emplace_back();
				state.name_ = String("State");
				state.nodePositionX_ = contextMenuCanvasX_;
				state.nodePositionY_ = contextMenuCanvasY_;

				ed::SetNodePosition(StateNodeId(target_->states_.size() - 1), ImVec2(state.nodePositionX_, state.nodePositionY_));
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("##nodeContext"))
		{
			if (ImGui::MenuItem("遷移を作成"))
			{
				creatingTransition_ = true;

				/// [EN] The menu item itself activates on a left release, and that
				///      same release must not be read as the target pick. A fresh
				///      left press has to arrive first.
				/// [JP] メニュー項目自体が左ボタンのリリースで確定するため、その
				///      リリースをそのまま対象選択として拾ってはいけない。改めて
				///      左ボタンが押されるまで受け付けない。
				creatingTransitionArmed_ = false;
				creatingTransitionSource_ = contextMenuSource_;

				ed::NodeId sourceNodeId = (contextMenuSource_ == Animator::AnyState) ? AnyNodeId() : StateNodeId(static_cast<Size>(contextMenuSource_));
				ImVec2 sourceNodePos = ed::GetNodePosition(sourceNodeId);
				creatingTransitionOffsetX_ = contextMenuCanvasX_ - sourceNodePos.x;
				creatingTransitionOffsetY_ = contextMenuCanvasY_ - sourceNodePos.y;
			}

			if (contextMenuSource_ >= 0)
			{
				if (ImGui::MenuItem("デフォルトステートに設定"))
				{
					target_->entryStateIndex_ = contextMenuSource_;
				}

				ImGui::Separator();

				/// [EN] Routed through the editor's own deletion queue rather than
				///      erasing here, so the single QueryDeletedNode path stays the
				///      only place that removes a state and fixes up the transition
				///      indices behind it.
				/// [JP] ここで直接消さずエディタ側の削除キューへ流す。ステートの
				///      削除と、それに伴う遷移インデックスの詰め直しを行う場所を
				///      QueryDeletedNode の1経路に集約しておくため。
				if (ImGui::MenuItem("削除"))
				{
					ed::DeleteNode(StateNodeId(static_cast<Size>(contextMenuSource_)));
				}
			}

			ImGui::EndPopup();
		}

		ed::Resume();

		ed::End();

		ed::NodeId hoveredNodeId = ed::GetHoveredNode();

		Bool completedTransitionThisFrame = false;

		if (creatingTransition_)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				creatingTransition_ = false;
				creatingTransitionArmed_ = false;
				creatingTransitionSource_ = -1;
			}
			else
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					creatingTransitionArmed_ = true;
				}

				/// [EN] Resolved on release, which covers both interaction styles
				///      with one path: clicking the target outright, and pressing
				///      anywhere then dragging onto the target.
				/// [JP] リリース時に確定させることで、対象を直接クリックする操作と、
				///      押してから対象までドラッグする操作の両方を1つの経路で扱う。
				if (creatingTransitionArmed_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
				{
					/// [EN] Hit tested against node rects in canvas space rather than
					///      through GetHoveredNode: the editor takes the press for its
					///      own node drag, so its hover state is not dependable here.
					/// [JP] GetHoveredNode ではなくキャンバス座標でノード矩形と直接
					///      判定する。押下はエディタ側がノードドラッグとして処理して
					///      しまい、そのホバー状態はここでは当てにできないため。
					ImVec2 releaseCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());

					Int targetState = -1;

					ImVec2 exitNodePos = ed::GetNodePosition(ExitNodeId());
					ImVec2 exitNodeSize = ed::GetNodeSize(ExitNodeId());
					if (releaseCanvasPos.x >= exitNodePos.x && releaseCanvasPos.x <= exitNodePos.x + exitNodeSize.x &&
						releaseCanvasPos.y >= exitNodePos.y && releaseCanvasPos.y <= exitNodePos.y + exitNodeSize.y)
					{
						targetState = Animator::ExitState;
					}

					if (targetState == -1)
					{
						for (Size candidate = 0; candidate < target_->states_.size(); ++candidate)
						{
							if (static_cast<Int>(candidate) == creatingTransitionSource_)
							{
								continue;
							}

							ImVec2 candidateNodePos = ed::GetNodePosition(StateNodeId(candidate));
							ImVec2 candidateNodeSize = ed::GetNodeSize(StateNodeId(candidate));
							if (releaseCanvasPos.x >= candidateNodePos.x && releaseCanvasPos.x <= candidateNodePos.x + candidateNodeSize.x &&
								releaseCanvasPos.y >= candidateNodePos.y && releaseCanvasPos.y <= candidateNodePos.y + candidateNodeSize.y)
							{
								targetState = static_cast<Int>(candidate);
								break;
							}
						}
					}

					if (targetState != -1)
					{
						AnimationTransition& transition = target_->transitions_.emplace_back();
						transition.fromState_ = creatingTransitionSource_;
						transition.toState_ = targetState;
						transition.fromOffsetX_ = creatingTransitionOffsetX_;
						transition.fromOffsetY_ = creatingTransitionOffsetY_;

						ed::NodeId targetNodeId = (targetState == Animator::ExitState) ? ExitNodeId() : StateNodeId(static_cast<Size>(targetState));
						ImVec2 targetNodePos = ed::GetNodePosition(targetNodeId);
						transition.toOffsetX_ = releaseCanvasPos.x - targetNodePos.x;
						transition.toOffsetY_ = releaseCanvasPos.y - targetNodePos.y;

						completedTransitionThisFrame = true;
					}

					creatingTransition_ = false;
					creatingTransitionArmed_ = false;
					creatingTransitionSource_ = -1;
				}
			}
		}

		Bool releasedOnEmpty = ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !creatingTransition_ && !completedTransitionThisFrame && ImGui::IsWindowHovered();
		ImVec2 releasePos = ImGui::GetMousePos();
		constexpr Float transitionHitDistance = 6.0f;
		Size hitTransitionIndex = SIZE_MAX;
		Float hitTransitionDistance = transitionHitDistance;

		ImDrawList* graphDrawList = ImGui::GetWindowDrawList();
		for (Size index = 0; index < target_->transitions_.size(); ++index)
		{
			const AnimationTransition& transition = target_->transitions_[index];
			Bool fromIsAny = (transition.fromState_ == Animator::AnyState);
			if (transition.fromState_ < 0 && !fromIsAny)
			{
				continue;
			}

			Bool toIsExit = (transition.toState_ == Animator::ExitState);

			Size fromIndex = 0;
			if (!fromIsAny)
			{
				fromIndex = static_cast<Size>(transition.fromState_);
				if (fromIndex >= target_->states_.size())
				{
					continue;
				}
			}

			Size toIndex = 0;
			if (!toIsExit)
			{
				if (transition.toState_ < 0)
				{
					continue;
				}
				toIndex = static_cast<Size>(transition.toState_);
				if (toIndex >= target_->states_.size() || (!fromIsAny && fromIndex == toIndex))
				{
					continue;
				}
			}

			ed::NodeId fromNodeIdForDraw = fromIsAny ? AnyNodeId() : StateNodeId(fromIndex);
			ed::NodeId toNodeIdForDraw = toIsExit ? ExitNodeId() : StateNodeId(toIndex);

			ImVec2 fromNodePos = ed::GetNodePosition(fromNodeIdForDraw);
			ImVec2 fromNodeSize = ed::GetNodeSize(fromNodeIdForDraw);
			ImVec2 toNodePos = ed::GetNodePosition(toNodeIdForDraw);
			ImVec2 toNodeSize = ed::GetNodeSize(toNodeIdForDraw);

			ImVec2 fromRectMax(fromNodePos.x + fromNodeSize.x, fromNodePos.y + fromNodeSize.y);
			ImVec2 toRectMax(toNodePos.x + toNodeSize.x, toNodePos.y + toNodeSize.y);

			/// [EN] Anchors are stored relative to their own node, so a usable one always
			///      lands inside that node's rect. Anything outside was never captured or
			///      has gone stale, and adding it would throw the endpoint off toward the
			///      canvas origin — fall back to the node centre. An exact zero is the
			///      never-set default rather than a deliberate top-left pick, so it takes
			///      the same fallback.
			/// [JP] アンカーは自分のノードからの相対で保存されるため、有効な値は必ず
			///      そのノードの矩形内に収まる。範囲外の値は未取得か古くなったもので、
			///      そのまま足すと端点がキャンバス原点の方向へ飛ぶ — ノード中心へ
			///      フォールバックする。ちょうど0は左上を意図的に指したのではなく
			///      未設定の既定値なので、同じくフォールバックさせる。
			Bool fromAnchorValid = (transition.fromOffsetX_ != 0.0f || transition.fromOffsetY_ != 0.0f) &&
				transition.fromOffsetX_ >= 0.0f && transition.fromOffsetX_ <= fromNodeSize.x &&
				transition.fromOffsetY_ >= 0.0f && transition.fromOffsetY_ <= fromNodeSize.y;
			Bool toAnchorValid = (transition.toOffsetX_ != 0.0f || transition.toOffsetY_ != 0.0f) &&
				transition.toOffsetX_ >= 0.0f && transition.toOffsetX_ <= toNodeSize.x &&
				transition.toOffsetY_ >= 0.0f && transition.toOffsetY_ <= toNodeSize.y;

			ImVec2 fromPoint = fromAnchorValid ? ImVec2(fromNodePos.x + transition.fromOffsetX_, fromNodePos.y + transition.fromOffsetY_) : ImVec2(fromNodePos.x + fromNodeSize.x * 0.5f, fromNodePos.y + fromNodeSize.y * 0.5f);
			ImVec2 toPoint = toAnchorValid ? ImVec2(toNodePos.x + transition.toOffsetX_, toNodePos.y + transition.toOffsetY_) : ImVec2(toNodePos.x + toNodeSize.x * 0.5f, toNodePos.y + toNodeSize.y * 0.5f);

			Int pairLow = Min(transition.fromState_, transition.toState_);
			Int pairHigh = Max(transition.fromState_, transition.toState_);
			Size pairOrdinal = 0;
			Size pairCount = 0;
			for (Size otherIndex = 0; otherIndex < target_->transitions_.size(); ++otherIndex)
			{
				const AnimationTransition& otherTransition = target_->transitions_[otherIndex];
				if (Min(otherTransition.fromState_, otherTransition.toState_) == pairLow &&
					Max(otherTransition.fromState_, otherTransition.toState_) == pairHigh)
				{
					if (otherIndex < index)
					{
						++pairOrdinal;
					}
					++pairCount;
				}
			}

			if (pairCount > 1)
			{
				constexpr Float parallelSpacing = 14.0f;
				Float parallelOffset = (static_cast<Float>(pairOrdinal) - (static_cast<Float>(pairCount) - 1.0f) * 0.5f) * parallelSpacing;

				/// [EN] The perpendicular flips with the line's direction, so a
				///      reversed pair (B->A against A->B) would cancel the sign
				///      out and land back on the same line. Negating the offset
				///      for the reversed direction keeps both on a fixed side.
				/// [JP] 垂線は線の向きに従って反転するため、逆向きの組(A->Bに
				///      対するB->A)では符号が打ち消し合い同じ線上に戻ってしまう。
				///      逆向き側のオフセットを反転させることで、両者を空間的に
				///      固定された別々の側へ振り分ける。
				if (transition.fromState_ > transition.toState_)
				{
					parallelOffset = -parallelOffset;
				}

				ImVec2 pairDirection(toPoint.x - fromPoint.x, toPoint.y - fromPoint.y);
				Float pairLength = std::sqrt(pairDirection.x * pairDirection.x + pairDirection.y * pairDirection.y);
				if (pairLength > 0.0001f)
				{
					ImVec2 pairPerpendicular(-pairDirection.y / pairLength, pairDirection.x / pairLength);
					fromPoint.x += pairPerpendicular.x * parallelOffset;
					fromPoint.y += pairPerpendicular.y * parallelOffset;
					toPoint.x += pairPerpendicular.x * parallelOffset;
					toPoint.y += pairPerpendicular.y * parallelOffset;
				}
			}

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
			ImU32 lineColor = isSelected ? IM_COL32(255, 176, 50, 255) : (toIsExit ? IM_COL32(230, 90, 90, 200) : (fromIsAny ? IM_COL32(90, 140, 230, 200) : IM_COL32(255, 255, 255, 180)));

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

		if (creatingTransition_)
		{
			ed::NodeId sourceNodeId = (creatingTransitionSource_ == Animator::AnyState) ? AnyNodeId() : StateNodeId(static_cast<Size>(creatingTransitionSource_));
			ImVec2 sourceNodePos = ed::GetNodePosition(sourceNodeId);
			ImVec2 sourceNodeSize = ed::GetNodeSize(sourceNodeId);
			ImVec2 sourceRectMax(sourceNodePos.x + sourceNodeSize.x, sourceNodePos.y + sourceNodeSize.y);

			ImVec2 rubberFrom(sourceNodePos.x + creatingTransitionOffsetX_, sourceNodePos.y + creatingTransitionOffsetY_);
			ImVec2 rubberTo = ed::ScreenToCanvas(ImGui::GetMousePos());

			ImVec2 clippedRubberFrom = ExitPointFromRect(rubberFrom, rubberTo, sourceNodePos, sourceRectMax);

			DrawTransitionArrow(graphDrawList, ed::CanvasToScreen(clippedRubberFrom), ed::CanvasToScreen(rubberTo), IM_COL32(255, 210, 120, 230), 2.0f);
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
	}

	void AnimatorControllerPanel::DrawDetails()
	{
		if (!target_)
		{
			ImGui::TextDisabled("対象のAnimatorがありません");
			return;
		}

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

				ImGui::SetNextItemWidth(60.0f);
				switch (parameter.type_)
				{
				case AnimationParameterType::Bool:
				{
					Bool boolValue = parameter.value_ != 0.0f;
					if (ImGui::Checkbox("##parameterValue", &boolValue))
					{
						parameter.value_ = boolValue ? 1.0f : 0.0f;
					}
					break;
				}
				case AnimationParameterType::Trigger:
				{
					if (ImGui::RadioButton("##parameterValue", parameter.value_ != 0.0f))
					{
						parameter.value_ = (parameter.value_ != 0.0f) ? 0.0f : 1.0f;
					}
					break;
				}
				case AnimationParameterType::Int:
				{
					Int intValue = static_cast<Int>(parameter.value_);
					if (ImGui::DragInt("##parameterValue", &intValue, 1.0f))
					{
						parameter.value_ = static_cast<Float>(intValue);
					}
					break;
				}
				default:
				{
					ImGui::DragFloat("##parameterValue", &parameter.value_, 0.01f);
					break;
				}
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
				String uniqueName;
				for (Int suffix = 0; ; ++suffix)
				{
					std::string candidate = suffix == 0 ? std::string("Parameter") : ("Parameter" + std::to_string(suffix));
					String candidateName = String(std::string_view(candidate));

					Bool candidateExists = false;
					for (const AnimationParameter& parameter : target_->parameters_)
					{
						if (parameter.name_ == candidateName)
						{
							candidateExists = true;
							break;
						}
					}

					if (!candidateExists)
					{
						uniqueName = candidateName;
						break;
					}
				}

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
			const Char* fromName = transition.fromState_ == Animator::AnyState ? "Any" : (fromIndex < target_->states_.size() ? target_->states_[fromIndex].name_.c_str() : "?");
			const Char* toName = toIndex < target_->states_.size() ? target_->states_[toIndex].name_.c_str() : "?";

			ImGui::Text("%s から %s", fromName, toName);
			ImGui::Spacing();

			ImGui::DragFloat("遷移時間", &transition.duration_, 0.01f, 0.0f, FLT_MAX);

			ImGui::Checkbox("Exit Timeを使用", &transition.hasExitTime_);
			if (transition.hasExitTime_)
			{
				ImGui::DragFloat("Exit Time", &transition.exitTime_, 0.01f, 0.0f, FLT_MAX);
			}

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

				const AnimationParameter* parameter = nullptr;
				for (const AnimationParameter& candidateParameter : target_->parameters_)
				{
					if (candidateParameter.name_ == condition.parameterName_)
					{
						parameter = &candidateParameter;
						break;
					}
				}
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
	}

	[[nodiscard]] Bool AnimatorControllerPanel::IsFocused()const
	{
		return isFocused_;
	}
}
