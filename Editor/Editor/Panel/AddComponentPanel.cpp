#include <Editor/Editor/Panel/AddComponentPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/ComponentLifecycleCommand.h>

namespace SeedCore
{
	AddComponentPanel::AddComponentPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void AddComponentPanel::Draw(Actor actor, ImGuiTexture& imguiTexture)
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

		/// [EN] Cap the popup's height to whichever space it actually opens
		///      into (with a small margin so it never touches the viewport
		///      edge) - without this, a long category list simply grows past
		///      the bottom of the screen and the lower entries become
		///      unreachable, since AlwaysAutoResize by itself has no notion of
		///      the viewport bound. Once height is constrained, ImGui's popup
		///      window adds its own scrollbar automatically for the overflow.
		/// [JP] ポップアップの高さを、実際に開く側の余白に収まるよう上限を
		///      掛ける(端に張り付かないよう少し余裕を持たせる) - これが無いと
		///      カテゴリ一覧が長い時に画面下へそのまま伸び続け、下の方の項目に
		///      届かなくなる(AlwaysAutoResize 単体はビューポート境界を知らない
		///      ため)。高さを制限すれば、はみ出た分は ImGui のポップアップ
		///      ウィンドウが自動でスクロールバーを付ける。
		constexpr Float minReasonableSpace = 150.0f;
		constexpr Float viewportEdgeMargin = 16.0f;
		Bool opensDownward = spaceBelow >= minReasonableSpace || spaceBelow >= spaceAbove;
		Float maxPopupHeight = (opensDownward ? spaceBelow : spaceAbove) - viewportEdgeMargin;
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, maxPopupHeight));

		if (opensDownward)
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
			DrawComponentList(actor, imguiTexture);
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

	void AddComponentPanel::DrawComponentList(Actor actor, ImGuiTexture& imguiTexture)
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

		/// [EN] "Custom" (ComponentRegistry::Register's default category, for
		///      components registered with no explicit one) is drawn last,
		///      after a separator, instead of wherever it lands alphabetically
		///      - it is a catch-all bucket rather than a real category, so
		///      mixing it in with the named ones makes the list harder to scan.
		/// [JP] "Custom"(ComponentRegistry::Register のデフォルトカテゴリ —
		///      明示的なカテゴリを指定せず登録したコンポーネント向け)は、
		///      アルファベット順の位置ではなく区切り線の後、一番下に描く —
		///      これは本物のカテゴリではなく雑多な受け皿なので、名前付きの
		///      カテゴリと混ぜると一覧が見づらくなる。
		static const std::string customCategory = "Custom";

		auto drawCategory = [&](const std::string& category, DynamicArray<std::pair<String, ComponentID>>& entries)
		{
			std::ranges::sort(entries, [](const auto& a, const auto& b)
				{
					return a.first.str() < b.first.str();
				});

			if (searching)
			{
				for (auto& [componentName, componentID] : entries)
				{
					DrawMenuItem(actor, componentName, componentID, imguiTexture);
				}
			}
			else if (ImGui::BeginMenu(category.c_str()))
			{
				for (auto& [componentName, componentID] : entries)
				{
					DrawMenuItem(actor, componentName, componentID, imguiTexture);
				}
				ImGui::EndMenu();
			}
		};

		for (auto& [category, entries] : grouped)
		{
			if (category == customCategory)
			{
				continue;
			}

			drawCategory(category, entries);
		}

		auto customEntries = grouped.find(customCategory);
		if (customEntries != grouped.end())
		{
			ImGui::Separator();
			drawCategory(customEntries->first, customEntries->second);
		}
	}

	void AddComponentPanel::DrawMenuItem(Actor actor, const String& componentName, ComponentID componentID, ImGuiTexture& imguiTexture)
	{
		Bool alreadyAttached = actor.HasComponent(componentID);

		if (alreadyAttached)
		{
			ImGui::BeginDisabled();
		}

		Float iconSize = ImGui::GetTextLineHeight();
		ImGui::Image(imguiTexture.Icon(ImGuiTexture::ComponentIconType(componentName)), ImVec2(iconSize, iconSize));
		ImGui::SameLine();

		Bool isSelected = (componentName == state_.selectedName);
		ImGuiSelectableFlags flags = ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowDoubleClick;
		if (ImGui::Selectable(componentName.c_str(), isSelected, flags))
		{
			state_.selectedName = componentName;
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				context_.sceneContext_.history_.Push(MakePtr<ComponentAddCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), componentID));
				actor.AddComponent(componentID);
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
			String("Active"),
			String("Bounds"),
			String("Material")
		};

		return std::ranges::contains(builtinComponents, componentName);
	}

	Bool AddComponentPanel::CheckFilterMatch(const String& componentName, const std::string& filterText)const
	{
		if (filterText.empty())
		{
			return true;
		}

		std::string targetName = componentName.str();
		auto found = std::ranges::search(targetName, filterText, [](char a, char b) { return std::tolower(a) == std::tolower(b); });
		return !found.empty();
	}
}
