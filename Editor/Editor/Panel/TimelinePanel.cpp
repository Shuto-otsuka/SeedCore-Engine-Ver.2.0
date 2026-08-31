#include <Editor/Editor/Panel/TimelinePanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiCommon.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/File/FileDialog.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	namespace
	{
		std::string AnimationLabel(ResourceCache* resource, Uint32 assetId)
		{
			Asset* asset = resource->GetAsset(assetId);
			if (!asset)
			{
				return "";
			}
			return std::filesystem::path(asset->path_.c_str()).filename().string();
		}

		constexpr Float tracksAreaHeight = 220.0f;

		DynamicArray<Size> SortedByTime(const DynamicArray<AnimationSpeedKeyframe>& keys)
		{
			DynamicArray<Size> order(keys.size());
			for (Size orderIndex = 0; orderIndex < order.size(); ++orderIndex)
			{
				order[orderIndex] = orderIndex;
			}
			std::ranges::sort(order, [&](Size a, Size b) { return keys[a].time_ < keys[b].time_; });
			return order;
		}

		Float CatmullRom(Float p0, Float p1, Float p2, Float p3, Float t)
		{
			Float t2 = t * t;
			Float t3 = t2 * t;
			return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
		}

		Float EvaluateTimeRemap(const DynamicArray<AnimationSpeedKeyframe>& keys, Float time)
		{
			if (keys.size() < 2)
			{
				return time;
			}

			DynamicArray<Size> order = SortedByTime(keys);

			if (time <= keys[order.front()].time_)
			{
				return keys[order.front()].value_;
			}
			if (time >= keys[order.back()].time_)
			{
				return keys[order.back()].value_;
			}

			for (Size orderIndex = 0; orderIndex + 1 < order.size(); ++orderIndex)
			{
				const AnimationSpeedKeyframe& p1 = keys[order[orderIndex]];
				const AnimationSpeedKeyframe& p2 = keys[order[orderIndex + 1]];
				if (time < p1.time_ || time > p2.time_)
				{
					continue;
				}

				const AnimationSpeedKeyframe& p0 = keys[order[orderIndex > 0 ? orderIndex - 1 : orderIndex]];
				const AnimationSpeedKeyframe& p3 = keys[order[orderIndex + 2 < order.size() ? orderIndex + 2 : orderIndex + 1]];

				Float span = p2.time_ - p1.time_;
				Float t = span > 0.0f ? (time - p1.time_) / span : 0.0f;
				return CatmullRom(p0.value_, p1.value_, p2.value_, p3.value_, t);
			}

			return time;
		}

		Float DrawTimelineBackground(const ImVec2& origin, const ImVec2& end, Float duration, Bool isActive = false)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(origin, end, IM_COL32(28, 28, 28, 255));
			drawList->AddRect(origin, end, isActive ? IM_COL32(255, 200, 60, 255) : IM_COL32(90, 90, 90, 255), 0.0f, 0, isActive ? 2.0f : 1.0f);

			for (Int division = 0; division <= 4; ++division)
			{
				Float x = origin.x + (end.x - origin.x) * (static_cast<Float>(division) / 4.0f);
				drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), IM_COL32(60, 60, 60, 255));
			}

			return duration > 0.0f ? duration : 1.0f;
		}

		void DrawEventTrack(DynamicArray<AnimationNotifyEvent>& notifyEvents, Float duration, Float& scrubTime, Size& selectedNotifyIndex, Bool& speedGraphActive)
		{
			constexpr Float trackHeight = 36.0f;
			constexpr Float hitRadius = 8.0f;

			ImGui::TextDisabled("Event");

			ImVec2 origin = ImGui::GetCursorScreenPos();
			ImVec2 size(ImGui::GetContentRegionAvail().x, trackHeight);
			ImVec2 end(origin.x + size.x, origin.y + size.y);

			ImGui::InvisibleButton("##eventTrack", size);
			Bool hovered = ImGui::IsItemHovered();

			Float safeDuration = DrawTimelineBackground(origin, end, duration, !speedGraphActive);
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			auto timeToX = [&](Float t) { return origin.x + (t / safeDuration) * size.x; };
			auto xToTime = [&](Float x) { return std::clamp((x - origin.x) / size.x, 0.0f, 1.0f) * safeDuration; };

			Float centerY = origin.y + size.y * 0.5f;

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 mouse = ImGui::GetIO().MousePos;
				Float bestDistance = hitRadius;
				Size best = SIZE_MAX;

				for (Size index = 0; index < notifyEvents.size(); ++index)
				{
					ImVec2 p(timeToX(notifyEvents[index].time_), centerY);
					Float distance = std::sqrt((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y));
					if (distance < bestDistance)
					{
						bestDistance = distance;
						best = index;
					}
				}

				selectedNotifyIndex = best;
				if (best == SIZE_MAX)
				{
					scrubTime = xToTime(mouse.x);
				}
				else
				{
					speedGraphActive = false;
				}
			}

			for (Size index = 0; index < notifyEvents.size(); ++index)
			{
				ImVec2 p(timeToX(notifyEvents[index].time_), centerY);
				Bool selected = (selectedNotifyIndex == index);
				ImU32 color = selected ? IM_COL32(255, 210, 60, 255) : IM_COL32(220, 170, 40, 255);
				ImVec2 diamond[4] = { ImVec2(p.x, p.y - 7.0f), ImVec2(p.x + 7.0f, p.y), ImVec2(p.x, p.y + 7.0f), ImVec2(p.x - 7.0f, p.y) };
				drawList->AddConvexPolyFilled(diamond, 4, color);
				if (selected)
				{
					drawList->AddPolyline(diamond, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.5f);
				}
			}

			Float scrubX = timeToX(scrubTime);
			drawList->AddLine(ImVec2(scrubX, origin.y), ImVec2(scrubX, end.y), IM_COL32(255, 255, 255, 180), 2.0f);
		}

		void DrawSpeedTrack(const DynamicArray<AnimationSpeedKeyframe>& speedKeys, Float duration, Float scrubTime, Bool& speedGraphActive, Size& selectedNotifyIndex)
		{
			constexpr Float trackHeight = 36.0f;

			ImGui::TextDisabled("Speed");

			ImVec2 origin = ImGui::GetCursorScreenPos();
			ImVec2 size(ImGui::GetContentRegionAvail().x, trackHeight);
			ImVec2 end(origin.x + size.x, origin.y + size.y);

			ImGui::InvisibleButton("##speedTrack", size);
			Bool hovered = ImGui::IsItemHovered();

			Float safeDuration = DrawTimelineBackground(origin, end, duration, speedGraphActive);
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			auto timeToX = [&](Float t) { return origin.x + (t / safeDuration) * size.x; };
			Float topY = origin.y + size.y * 0.1f;
			Float baseY = origin.y + size.y * 0.9f;
			auto valueToY = [&](Float v) { return baseY - std::clamp(v / safeDuration, 0.0f, 1.0f) * (baseY - topY); };

			drawList->AddLine(ImVec2(origin.x, baseY), ImVec2(end.x, topY), IM_COL32(70, 70, 70, 255));

			constexpr Int kSegments = 32;
			ImVec2 previous(timeToX(0.0f), valueToY(EvaluateTimeRemap(speedKeys, 0.0f)));
			for (Int step = 1; step <= kSegments; ++step)
			{
				Float t = safeDuration * (static_cast<Float>(step) / static_cast<Float>(kSegments));
				ImVec2 current(timeToX(t), valueToY(EvaluateTimeRemap(speedKeys, t)));
				drawList->AddLine(previous, current, IM_COL32(100, 190, 255, 220), 2.0f);
				previous = current;
			}

			for (const AnimationSpeedKeyframe& key : speedKeys)
			{
				ImVec2 p(timeToX(key.time_), valueToY(key.value_));
				drawList->AddCircleFilled(p, 3.5f, IM_COL32(100, 190, 255, 255));
			}

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				speedGraphActive = true;
				selectedNotifyIndex = SIZE_MAX;
			}

			Float scrubX = timeToX(scrubTime);
			drawList->AddLine(ImVec2(scrubX, origin.y), ImVec2(scrubX, end.y), IM_COL32(255, 255, 255, 180), 2.0f);
		}

		void DrawSpeedGraph(DynamicArray<AnimationSpeedKeyframe>& speedKeys, Float duration, Size& selectedSpeedIndex, Bool& isDragging)
		{
			constexpr Float graphHeight = 160.0f;
			constexpr Float hitRadius = 16.0f;

			ImVec2 origin = ImGui::GetCursorScreenPos();
			ImVec2 size(ImGui::GetContentRegionAvail().x, graphHeight);
			ImVec2 end(origin.x + size.x, origin.y + size.y);

			ImGui::InvisibleButton("##speedGraph", size);
			Bool hovered = ImGui::IsItemHovered();
			Bool active = ImGui::IsItemActive();

			Float safeDuration = DrawTimelineBackground(origin, end, duration);
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			auto timeToX = [&](Float t) { return origin.x + (t / safeDuration) * size.x; };
			auto xToTime = [&](Float x) { return std::clamp((x - origin.x) / size.x, 0.0f, 1.0f) * safeDuration; };

			Float topY = origin.y + size.y * 0.06f;
			Float baseY = origin.y + size.y * 0.94f;
			auto valueToY = [&](Float v) { return baseY - std::clamp(v / safeDuration, 0.0f, 1.0f) * (baseY - topY); };
			auto yToValue = [&](Float y) { return std::clamp((baseY - y) / (baseY - topY), 0.0f, 1.0f) * safeDuration; };

			drawList->AddLine(ImVec2(origin.x, baseY), ImVec2(end.x, topY), IM_COL32(70, 70, 70, 255));

			constexpr Int kSegments = 48;
			ImVec2 previous(timeToX(0.0f), valueToY(EvaluateTimeRemap(speedKeys, 0.0f)));
			for (Int step = 1; step <= kSegments; ++step)
			{
				Float t = safeDuration * (static_cast<Float>(step) / static_cast<Float>(kSegments));
				ImVec2 current(timeToX(t), valueToY(EvaluateTimeRemap(speedKeys, t)));
				drawList->AddLine(previous, current, IM_COL32(100, 190, 255, 220), 2.0f);
				previous = current;
			}

			ImVec2 mouse = ImGui::GetIO().MousePos;
			Bool mouseClicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

			if (mouseClicked)
			{
				Float bestDistance = hitRadius;
				Size best = SIZE_MAX;

				for (Size index = 0; index < speedKeys.size(); ++index)
				{
					ImVec2 p(timeToX(speedKeys[index].time_), valueToY(speedKeys[index].value_));
					Float distance = std::sqrt((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y));
					if (distance < bestDistance)
					{
						bestDistance = distance;
						best = index;
					}
				}

				selectedSpeedIndex = best;
				isDragging = (best != SIZE_MAX);
			}

			if (!active)
			{
				isDragging = false;
			}

			if (isDragging && active && selectedSpeedIndex != SIZE_MAX && selectedSpeedIndex < speedKeys.size())
			{
				speedKeys[selectedSpeedIndex].time_ = xToTime(mouse.x);
				speedKeys[selectedSpeedIndex].value_ = yToValue(mouse.y);
			}

			for (Size index = 0; index < speedKeys.size(); ++index)
			{
				ImVec2 p(timeToX(speedKeys[index].time_), valueToY(speedKeys[index].value_));
				Bool selected = (selectedSpeedIndex == index);
				ImU32 color = selected ? IM_COL32(140, 220, 255, 255) : IM_COL32(100, 190, 255, 255);
				drawList->AddCircleFilled(p, selected ? 6.0f : 5.0f, color);
				if (selected)
				{
					drawList->AddCircle(p, 8.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
				}
			}
		}
	}

	TimelinePanel::TimelinePanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void TimelinePanel::Open()
	{
		show_ = true;
		ImGui::SetWindowFocus("タイムライン");
	}

	void TimelinePanel::SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle)
	{
		previewHandle_ = previewHandle;
	}

	void TimelinePanel::Draw()
	{
		if (!show_)
		{
			context_.timelinePreviewContext_.previewActive_ = false;
			isPlaying_ = false;
			isFocused_ = false;
			return;
		}

		Animator* selectedTarget = context_.selectionContext_.selectedActor_ ? const_cast<Animator*>(context_.selectionContext_.selectedActor_.GetComponent<Animator>()) : nullptr;
		if (selectedTarget != target_)
		{
			target_ = selectedTarget;
			selectedAnimationIndex_ = SIZE_MAX;
			scrubTime_ = 0.0f;
			isPlaying_ = false;
		}

		context_.timelinePreviewContext_.previewActive_ = false;

		ImGui::DockBuilderDockWindow("タイムライン", context_.graphicsContext_.imgui_->GetDockSpaceID());
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);

		isFocused_ = ImGui::Begin("タイムライン", &show_);
		if (isFocused_)
		{
			if (!target_)
			{
				ImGui::TextDisabled("対象のAnimatorがありません");
			}
			else if (target_->animationIDs_.empty())
			{
				ImGui::TextDisabled("アニメーションIDが登録されていません");
			}
			else
			{
				std::string preview = (selectedAnimationIndex_ < target_->animationIDs_.size())
					? AnimationLabel(context_.worldContext_.resource_, target_->animationIDs_[selectedAnimationIndex_])
					: "(未選択)";

				ImGui::SetNextItemWidth(200.0f);
				if (ImGui::BeginCombo("アニメーション", preview.c_str()))
				{
					for (Size index = 0; index < target_->animationIDs_.size(); ++index)
					{
						std::string label = AnimationLabel(context_.worldContext_.resource_, target_->animationIDs_[index]);
						Bool selected = (selectedAnimationIndex_ == index);
						if (ImGui::Selectable(label.c_str(), selected))
						{
							selectedAnimationIndex_ = index;
							scrubTime_ = 0.0f;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				Uint32 assetId = (selectedAnimationIndex_ != SIZE_MAX) ? target_->animationIDs_[selectedAnimationIndex_] : 0;
				Float duration = 0.0f;
				Animation* animation = nullptr;

				if (selectedAnimationIndex_ != SIZE_MAX)
				{
					AnimationResource* animationResource = context_.worldContext_.resource_->GetAnimationResource();
					Handle<Animation> handle = animationResource->Load(*context_.worldContext_.loader_, *context_.worldContext_.resource_, assetId);
					animation = animationResource->Resolve(*context_.worldContext_.loader_, handle);

					duration = animation ? animation->Duration() : 0.0f;

					if (isPlaying_ && duration > 0.0f)
					{
						scrubTime_ += ImGui::GetIO().DeltaTime;
						if (scrubTime_ > duration)
						{
							scrubTime_ = std::fmod(scrubTime_, duration);
						}
					}
				}

				const Mesh* mesh = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetComponent<Mesh>() : nullptr;
				if (mesh && mesh->meshID_ != 0)
				{
					context_.timelinePreviewContext_.previewActive_ = true;
					context_.timelinePreviewContext_.previewMeshAssetId_ = mesh->meshID_;
					context_.timelinePreviewContext_.previewAnimationAssetId_ = assetId;
					context_.timelinePreviewContext_.previewTime_ = animation ? EvaluateTimeRemap(animation->SpeedCurve(), scrubTime_) : scrubTime_;

					Float unit = ImGui::GetFrameHeightWithSpacing();
					Float separatorHeight = ImGui::GetStyle().ItemSpacing.y * 2.0f + 1.0f;

					Float reservedHeight = separatorHeight + unit * 2.0f;

					ImVec2 previewSize = ImGui::GetContentRegionAvail();
					previewSize.y = Max(previewSize.y - reservedHeight, 100.0f);

					if (context_.cameraContext_.timelineCamera_)
					{
						context_.cameraContext_.timelineCamera_->Resize(previewSize.x, previewSize.y);
					}

					ImGui::Image(ImTextureID(previewHandle_.ptr), previewSize);

					/// [EN] Orbit-style preview navigation: left-click drag orbits around
					///      Focus(), middle-click drag pans, wheel dollies - see
					///      PreviewCameraController. Gated on hovering the image so
					///      the playback/track controls below don't trigger it.
					/// [JP] オービット式のプレビュー操作: 左クリックドラッグでFocus()を
					///      中心に回転、中クリックドラッグでパン、ホイールでドリー -
					///      PreviewCameraController参照。画像自体のホバーでゲートする
					///      (下の再生/トラック操作では発火しない)。
					Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
					Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

					if (ImGui::IsItemHovered() && context_.cameraContext_.timelineCamera_ && context_.cameraContext_.timelineCameraController_)
					{
						if ((orbitHeld || panHeld) && !InputSystem::IsMouseCaptured())
						{
							InputSystem::BeginMouseCapture();
						}

						context_.cameraContext_.timelineCameraController_->Update(*context_.cameraContext_.timelineCamera_, ImGui::GetIO().DeltaTime);
					}

					if (!orbitHeld && !panHeld && InputSystem::IsMouseCaptured())
					{
						InputSystem::EndMouseCapture();
					}

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					if (selectedAnimationIndex_ == SIZE_MAX)
					{
						ImGui::TextDisabled("アニメーション未選択 — バインドポーズを表示中");
					}
					else
					{
						if (ImGui::Button(isPlaying_ ? "一時停止" : "再生"))
						{
							isPlaying_ = !isPlaying_;
						}

						ImGui::SameLine();

						ImGui::SetNextItemWidth(-1.0f);
						ImGui::SliderFloat("##scrub", &scrubTime_, 0.0f, duration > 0.0f ? duration : 1.0f, "%.3f 秒");

						ImGui::Text("再生時間: %.3f 秒", duration);
					}
				}
			}
		}
		ImGui::End();
	}

	void TimelinePanel::DrawDetails()
	{
		if (!target_ || selectedAnimationIndex_ == SIZE_MAX || selectedAnimationIndex_ >= target_->animationIDs_.size())
		{
			ImGui::TextDisabled("アニメーションを選択してください");
			return;
		}

		Uint32 assetId = target_->animationIDs_[selectedAnimationIndex_];
		AnimationResource* animationResource = context_.worldContext_.resource_->GetAnimationResource();
		Handle<Animation> handle = animationResource->Load(*context_.worldContext_.loader_, *context_.worldContext_.resource_, assetId);
		Animation* animation = animationResource->Resolve(*context_.worldContext_.loader_, handle);
		if (!animation)
		{
			ImGui::TextDisabled("アニメーションを選択してください");
			return;
		}

		Float duration = animation->Duration();
		DynamicArray<AnimationNotifyEvent>& events = animation->NotifyEvents();
		DynamicArray<AnimationSpeedKeyframe>& keys = animation->SpeedCurve();

		ImGui::TextWrapped("トラック (クリックで選択/シーク、点はドラッグで移動、Deleteで削除)");

		if (ImGui::Button("通知イベントを追加"))
		{
			AnimationNotifyEvent& event = events.emplace_back();
			event.time_ = scrubTime_;
			event.label_ = "Event";
			selectedNotifyIndex_ = events.size() - 1;
			speedGraphActive_ = false;
		}

		if (ImGui::Button("速度キーフレームを追加"))
		{
			AnimationSpeedKeyframe& key = keys.emplace_back();
			key.time_ = scrubTime_;
			key.value_ = scrubTime_;
			selectedSpeedKeyIndex_ = keys.size() - 1;
			selectedNotifyIndex_ = SIZE_MAX;
			speedGraphActive_ = true;
		}

		ImGui::BeginChild("##tracksColumn", ImVec2(0.0f, tracksAreaHeight), true);
		{
			DrawEventTrack(events, duration, scrubTime_, selectedNotifyIndex_, speedGraphActive_);
			DrawSpeedTrack(keys, duration, scrubTime_, speedGraphActive_, selectedNotifyIndex_);

			if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				if (selectedNotifyIndex_ != SIZE_MAX && selectedNotifyIndex_ < events.size())
				{
					events.erase(events.begin() + selectedNotifyIndex_);
					selectedNotifyIndex_ = SIZE_MAX;
				}
				else if (speedGraphActive_ && selectedSpeedKeyIndex_ != SIZE_MAX && selectedSpeedKeyIndex_ < keys.size())
				{
					keys.erase(keys.begin() + selectedSpeedKeyIndex_);
					selectedSpeedKeyIndex_ = SIZE_MAX;
				}
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		Asset* asset = context_.worldContext_.resource_->GetAsset(assetId);

		if (ImGui::Button("上書き保存") && asset)
		{
			std::filesystem::path overwritePath(asset->fullpath_.c_str());
			if (context_.worldContext_.loader_->animationLoader_->Save(*animation, overwritePath))
			{
				SC_LOG_NOTICE("アニメーションを上書き保存しました: {}", overwritePath.string());
			}
			else
			{
				SC_LOG_WARNING("アニメーションの上書き保存に失敗しました: {}", overwritePath.string());
			}
		}

		if (ImGui::Button("名前を付けて保存") && asset)
		{
			std::filesystem::path initialDir = std::filesystem::path(asset->fullpath_.c_str()).parent_path();

			std::filesystem::path savePath;
			if (FileDialog::SaveFile(savePath, initialDir, L"Animation Files (*.animation)", L"*.animation", L"animation"))
			{
				if (context_.worldContext_.loader_->animationLoader_->Save(*animation, savePath))
				{
					SC_LOG_NOTICE("アニメーションを保存しました: {}", savePath.string());
				}
				else
				{
					SC_LOG_WARNING("アニメーションの保存に失敗しました: {}", savePath.string());
				}
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (selectedNotifyIndex_ != SIZE_MAX && selectedNotifyIndex_ < events.size())
		{
			AnimationNotifyEvent& event = events[selectedNotifyIndex_];

			ImGui::Text("Event 詳細");
			ImGui::Spacing();

			ImGui::SetNextItemWidth(-1.0f);
			ImGui::DragFloat("時刻##notifyTime", &event.time_, 0.01f, 0.0f, duration > 0.0f ? duration : 1.0f, "%.3f 秒");

			ImGui::SetNextItemWidth(-1.0f);
			Char labelBuffer[128];
			strncpy_s(labelBuffer, event.label_.c_str(), sizeof(labelBuffer) - 1);
			if (ImGui::InputText("ラベル##notifyLabel", labelBuffer, sizeof(labelBuffer)))
			{
				event.label_ = labelBuffer;
			}
		}
		else if (speedGraphActive_)
		{
			ImGui::Text("Speed カーブ");
			ImGui::Spacing();

			DrawSpeedGraph(keys, duration, selectedSpeedKeyIndex_, isDraggingKeyframe_);

			if (selectedSpeedKeyIndex_ != SIZE_MAX && selectedSpeedKeyIndex_ < keys.size())
			{
				AnimationSpeedKeyframe& key = keys[selectedSpeedKeyIndex_];

				ImGui::SetNextItemWidth(90.0f);
				ImGui::DragFloat("時刻##speedTime", &key.time_, 0.01f, 0.0f, duration > 0.0f ? duration : 1.0f, "%.3f 秒");

				ImGui::SameLine();

				ImGui::SetNextItemWidth(120.0f);
				ImGui::DragFloat("出力時刻##speedValue", &key.value_, 0.01f, 0.0f, duration > 0.0f ? duration : 1.0f, "%.3f 秒");
			}
		}
		else
		{
			ImGui::TextDisabled("Event か Speed トラックをクリックしてください");
		}
	}
}
