#include <Editor/Editor/Panel/AddComponentPanel.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	void AddComponentPanel::Draw(Actor* actor, ImGuiTexture& imguiTexture)
	{
		Float availableWidth = ImGui::GetContentRegionAvail().x;
		Float buttonWidth = 200.0f;
		ImGui::SetCursorPosX((availableWidth - buttonWidth) * 0.5f);

		if (ImGui::Button("コンポーネントを追加", ImVec2(buttonWidth, 0)))
		{
			state_.searchBuffer = String("");
			state_.selectedName = String("");
			ImGui::OpenPopup("AddComponentPopup");
		}

		/// [EN] Anchor below the button by default (predictable position,
		///      like a combo box), but flip to open upward when there isn't
		///      reasonable room below - same flip behavior ImGui's own combo
		///      boxes use, applied manually since a plain SetNextWindowPos
		///      would otherwise always force it downward even off-screen.
		///      Nested BeginMenu flyouts are unaffected and keep ImGui's own
		///      automatic edge avoidance.
		/// [JP] 既定はボタン直下（コンボボックスと同じ予測しやすい位置）。
		///      ただし下に十分な余白が無いときは上向きに開く（ImGui の
		///      コンボボックス自身が使う反転と同じ挙動を手動で再現）。
		///      単純な SetNextWindowPos だと画面外でも常に下向きに固定
		///      されてしまうため。ネストされた BeginMenu フライアウトは
		///      影響を受けず、ImGui 本来の自動画面端回避のまま。
		ImVec2 buttonMin = ImGui::GetItemRectMin();
		ImVec2 buttonMax = ImGui::GetItemRectMax();
		Float buttonCenterX = (buttonMin.x + buttonMax.x) * 0.5f;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		Float spaceBelow = (viewport->WorkPos.y + viewport->WorkSize.y) - buttonMax.y;
		Float spaceAbove = buttonMin.y - viewport->WorkPos.y;

		constexpr Float minReasonableSpace = 150.0f;
		if (spaceBelow >= minReasonableSpace || spaceBelow >= spaceAbove)
		{
			ImGui::SetNextWindowPos(ImVec2(buttonCenterX, buttonMax.y), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		}
		else
		{
			ImGui::SetNextWindowPos(ImVec2(buttonCenterX, buttonMin.y), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::SetNextItemWidth(240.0f);
			DrawSearchBar(imguiTexture);
			ImGui::Separator();
			DrawComponentList(actor);
			ImGui::EndPopup();
		}
	}

	void AddComponentPanel::DrawSearchBar(ImGuiTexture& imguiTexture)
	{
		std::string searchText = state_.searchBuffer.str();
		searchText.resize(256);

		Float iconSize = ImGui::GetTextLineHeight();
		Float originalPaddingX = ImGui::GetStyle().FramePadding.x;
		Float iconPadding = iconSize + originalPaddingX * 2.0f;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(iconPadding, ImGui::GetStyle().FramePadding.y));

		if (ImGui::InputTextWithHint("##Search", "検索...", searchText.data(), searchText.capacity()))
		{
			state_.searchBuffer = String(std::string_view(searchText.c_str()));
		}

		ImGui::PopStyleVar();
		ImVec2 inputMin = ImGui::GetItemRectMin();
		Float inputHeight = ImGui::GetItemRectSize().y;
		Float iconY = inputMin.y + (inputHeight - iconSize) * 0.5f;
		ImGui::GetWindowDrawList()->AddImage(imguiTexture.Icon(IconType::Search), ImVec2(inputMin.x + originalPaddingX, iconY), ImVec2(inputMin.x + originalPaddingX + iconSize, iconY + iconSize));
	}

	void AddComponentPanel::DrawComponentList(Actor* actor)
	{
		auto& componentList = ComponentRegistry::GetComponentList();
		std::string filterText = state_.searchBuffer.str();

		/// [EN] Group by ComponentMetadata::category_ (e.g. "Light"). std::map
		///      keeps categories alphabetically sorted for free.
		/// [JP] ComponentMetadata::category_（例: "Light"）でグループ化。
		///      std::map によってカテゴリはそのままアルファベット順になる。
		std::map<std::string, DynamicArray<std::pair<String, ComponentID>>> grouped;

		for (auto& [componentName, componentID] : componentList)
		{
			if (CheckBuiltinComponent(componentName))
			{
				continue;
			}
			if (!CheckFilterMatch(componentName, filterText))
			{
				continue;
			}

			std::string category = ComponentRegistry::Get(componentID).category_.str();
			grouped[category].emplace_back(componentName, componentID);
		}

		if (grouped.empty())
		{
			ImGui::TextDisabled("該当なし");
			return;
		}

		/// [EN] While searching, show a flat list of matches (no submenu
		///      hierarchy to open) - faster to scan than hunting through
		///      category flyouts. With no search text, show the VS-style
		///      category flyout submenus (BeginMenu).
		/// [JP] 検索中はフラットな一致リストを表示する（サブメニューを開く
		///      手間を無くす）。検索文字が無いときは VS 風のカテゴリ
		///      フライアウトサブメニュー（BeginMenu）を表示する。
		Bool searching = !filterText.empty();

		for (auto& [category, entries] : grouped)
		{
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
				{
					return a.first.str() < b.first.str();
				});

			if (searching)
			{
				for (auto& [componentName, componentID] : entries)
				{
					DrawMenuItem(actor, componentName, componentID);
				}
			}
			else if (ImGui::BeginMenu(category.c_str()))
			{
				for (auto& [componentName, componentID] : entries)
				{
					DrawMenuItem(actor, componentName, componentID);
				}
				ImGui::EndMenu();
			}
		}
	}

	void AddComponentPanel::DrawMenuItem(Actor* actor, const String& componentName, ComponentID componentID)
	{
		Bool alreadyAttached = actor->HasComponent(componentID);

		if (alreadyAttached)
		{
			ImGui::BeginDisabled();
		}

		Bool isSelected = (componentName == state_.selectedName);
		ImGuiSelectableFlags flags = ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowDoubleClick;
		if (ImGui::Selectable(componentName.c_str(), isSelected, flags))
		{
			state_.selectedName = componentName;
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				actor->AddComponent(componentID);
				ImGui::CloseCurrentPopup();
			}
		}

		if (alreadyAttached)
		{
			ImGui::EndDisabled();
		}
	}

	Bool AddComponentPanel::CheckBuiltinComponent(const String& componentName)const
	{
		static const String builtinComponents[] =
		{
			String("Name"),
			String("Position"),
			String("Rotation"),
			String("Scale"),
			String("Velocity"),
			String("Active")
		};

		for (const auto& builtin : builtinComponents)
		{
			if (componentName == builtin)
			{
				return true;
			}
		}
		return false;
	}

	Bool AddComponentPanel::CheckFilterMatch(const String& componentName, const std::string& filterText)const
	{
		if (filterText.empty())
		{
			return true;
		}

		std::string targetName = componentName.str();
		for (Size charIndex = 0; charIndex + filterText.size() <= targetName.size(); ++charIndex)
		{
			Bool isMatch = true;
			for (Size filterIndex = 0; filterIndex < filterText.size(); ++filterIndex)
			{
				if (std::tolower(targetName[charIndex + filterIndex]) != std::tolower(filterText[filterIndex]))
				{
					isMatch = false;
					break;
				}
			}

			if (isMatch)
			{
				return true;
			}
		}
		return false;
	}
}
