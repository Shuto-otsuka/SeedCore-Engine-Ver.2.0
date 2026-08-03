#include <Editor/Editor/Panel/ConsolePanel.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/Log/LogSystem.h>

namespace SeedCore
{
	ConsolePanel::ConsolePanel(ImGuiTexture& imguiTexture) : imguiTexture_(imguiTexture)
	{
		/// No Code
	}

	void ConsolePanel::Draw()
	{
		const auto& logs = LogSystem::GetLogs();

		Uint32 errorCount = 0;
		Uint32 warningCount = 0;
		Uint32 noticeCount = 0;
		for (const auto& entry : logs)
		{
			switch (entry.level_)
			{
			case LogLevel::Error:
				errorCount++;
				break;
			case LogLevel::Warning:
				warningCount++;
				break;
			case LogLevel::Notice:
				noticeCount++;
				break;
			}
		}

		if (ImGui::Button("クリア"))
		{
			LogSystem::Clear();
		}

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImVec2 iconSize(14, 14);

		auto FilterButton = [&](const Char* id, IconType icon, Bool& flag, Uint32 count, const ImVec4& color)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, flag ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) : ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));

			Char label[64];
			snprintf(label, sizeof(label), "%u", count);
			Float textWidth = ImGui::CalcTextSize(label).x;
			Float totalWidth = iconSize.x + 4.0f + textWidth + ImGui::GetStyle().FramePadding.x * 2.0f;
			Float buttonHeight = ImGui::GetFrameHeight();

			ImVec2 cursor = ImGui::GetCursorScreenPos();
			if (ImGui::InvisibleButton(id, ImVec2(totalWidth, buttonHeight)))
			{
				flag = !flag;
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			if (ImGui::IsItemHovered())
			{
				drawList->AddRectFilled(cursor, ImVec2(cursor.x + totalWidth, cursor.y + buttonHeight), ImGui::GetColorU32(ImGuiCol_HeaderHovered), ImGui::GetStyle().FrameRounding);
			}
			else if (flag)
			{
				drawList->AddRectFilled(cursor, ImVec2(cursor.x + totalWidth, cursor.y + buttonHeight), ImGui::GetColorU32(ImGuiCol_ButtonActive), ImGui::GetStyle().FrameRounding);
			}

			Float iconY = cursor.y + (buttonHeight - iconSize.y) * 0.5f;
			Float iconX = cursor.x + ImGui::GetStyle().FramePadding.x;
			drawList->AddImage(imguiTexture_.Icon(icon), ImVec2(iconX, iconY), ImVec2(iconX + iconSize.x, iconY + iconSize.y));

			Float textX = iconX + iconSize.x + 4.0f;
			Float textY = cursor.y + (buttonHeight - ImGui::GetTextLineHeight()) * 0.5f;
			drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(color), label);

			ImGui::PopStyleColor(2);
		};

		FilterButton("##WarningFilter", IconType::LogWarning, showWarning_, warningCount, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::SameLine();
		FilterButton("##ErrorFilter",   IconType::LogError,   showError_,   errorCount,   ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		ImGui::SameLine();
		FilterButton("##NoticeFilter",  IconType::LogNotice,  showNotice_,  noticeCount,  ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

		ImGui::Separator();

		ImGui::BeginChild("##LogArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		ImGuiListClipper clipper;
		DynamicArray<Uint32> visibleIndices;
		for (Uint32 index = 0; index < static_cast<Uint32>(logs.size()); index++)
		{
			if (logs[index].level_ == LogLevel::Notice && !showNotice_)
			{
				continue;
			}
			if (logs[index].level_ == LogLevel::Warning && !showWarning_)
			{
				continue;
			}
			if (logs[index].level_ == LogLevel::Error && !showError_)
			{
				continue;
			}
			visibleIndices.push_back(index);
		}

		for (Uint32 row = 0; row < static_cast<Uint32>(visibleIndices.size()); row++)
		{
			const auto& entry = logs[visibleIndices[row]];

			/// [EN] Built as a std::string rather than a fixed buffer because a single entry can be arbitrarily long — a failed Runtime build logs MSBuild's entire output as one message. Truncating here would cut the text mid-UTF-8-sequence (garbling the last character) and, worse, silently hide the actual compiler/linker error that follows.
			/// [JP] 1エントリが任意の長さになりうるため、固定バッファではなく std::string で組み立てる — Runtimeビルド失敗時は MSBuild の全出力が1つのメッセージとして記録される。ここで切り詰めると UTF-8 の途中で切れて末尾の文字が化けるうえ、その後ろにある肝心のコンパイラ/リンカのエラーを黙って隠してしまう。
			std::string rowText = std::format("{}  ({}:{})", entry.message_.str(), entry.file_.str(), entry.line_);

			Float textOffsetX = 4.0f + 14.0f + 6.0f;
			ImVec2 textSize = ImGui::CalcTextSize(rowText.c_str(), rowText.c_str() + rowText.size(), false, ImGui::GetContentRegionAvail().x - textOffsetX);
			Float rowHeight = (textSize.y > 14.0f ? textSize.y : 14.0f) + 6.0f;

			ImVec4 color;
			IconType icon;
			switch (entry.level_)
			{
			case LogLevel::Error:
				color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
				icon = IconType::LogError;
				break;
			case LogLevel::Warning:
				color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
				icon = IconType::LogWarning;
				break;
			default:
				color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
				icon = IconType::LogNotice;
				break;
			}

			ImVec2 position = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			if (row % 2 == 1)
			{
				drawList->AddRectFilled(position, ImVec2(position.x + ImGui::GetContentRegionAvail().x, position.y + rowHeight), IM_COL32(255, 255, 255, 10));
			}

			ImVec2 logIconSize(14, 14);
			Float iconY = position.y + (rowHeight - logIconSize.y) * 0.5f;
			drawList->AddImage(imguiTexture_.Icon(icon), ImVec2(position.x + 4.0f, iconY), ImVec2(position.x + 4.0f + logIconSize.x, iconY + logIconSize.y));

			ImGui::SetCursorScreenPos(ImVec2(position.x + textOffsetX, position.y + 3.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			/// [EN] TextUnformatted with an explicit end pointer, not TextWrapped("%s", ...): ImGui's printf-style path formats into its own fixed-size buffer, which would reintroduce exactly the truncation this avoids. Wrapping is instead set up by the caller-side PushTextWrapPos below.
			/// [JP] TextWrapped("%s", ...) ではなく終端ポインタ付きの TextUnformatted を使う: ImGui の printf 形式の経路は内部の固定サイズバッファへ整形するため、まさにここで避けている切り詰めが再発してしまう。折り返しは代わりに下の PushTextWrapPos で設定する。
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextUnformatted(rowText.c_str(), rowText.c_str() + rowText.size());
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();

			/// [EN] Invisible hit area over the row for a right-click context menu
			///      (copy the line to the clipboard).
			/// [JP] 右クリックコンテキストメニュー（行をクリップボードにコピー）用に
			///      行全体を覆う不可視ヒットエリアを置く。
			ImGui::SetCursorScreenPos(position);
			Char rowId[32];
			snprintf(rowId, sizeof(rowId), "##logRow%u", visibleIndices[row]);
			ImGui::InvisibleButton(rowId, ImVec2(ImGui::GetContentRegionAvail().x, rowHeight));
			if (ImGui::BeginPopupContextItem(rowId))
			{
				if (ImGui::MenuItem("この行をコピー"))
				{
					ImGui::SetClipboardText(rowText.c_str());
				}
				if (ImGui::MenuItem("メッセージのみコピー"))
				{
					ImGui::SetClipboardText(entry.message_.c_str());
				}
				ImGui::EndPopup();
			}

			ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + rowHeight));
			ImGui::Dummy(ImVec2(0, 0));
		}

		if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
	}
}
