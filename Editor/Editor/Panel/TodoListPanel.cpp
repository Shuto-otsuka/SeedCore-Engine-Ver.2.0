#include <Editor/Editor/Panel/TodoListPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	namespace
	{
		const Char* PriorityLabel(TodoPriority priority)
		{
			switch (priority)
			{
			case TodoPriority::High: return "高";
			case TodoPriority::Mid:  return "中";
			case TodoPriority::Low:  return "低";
			}
			return "中";
		}

		ImVec4 PriorityColor(TodoPriority priority)
		{
			switch (priority)
			{
			case TodoPriority::High: return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
			case TodoPriority::Mid:  return ImVec4(0.95f, 0.80f, 0.30f, 1.0f);
			case TodoPriority::Low:  return ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
			}
			return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		Bool PriorityCombo(const Char* id, TodoPriority& priority)
		{
			static const TodoPriority order[] = { TodoPriority::High, TodoPriority::Mid, TodoPriority::Low };

			Bool changed = false;

			ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor(priority));
			if (ImGui::BeginCombo(id, PriorityLabel(priority), ImGuiComboFlags_WidthFitPreview))
			{
				ImGui::PopStyleColor();

				for (TodoPriority candidate : order)
				{
					Bool selected = (candidate == priority);
					ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor(candidate));
					if (ImGui::Selectable(PriorityLabel(candidate), selected))
					{
						priority = candidate;
						changed = true;
					}
					ImGui::PopStyleColor();
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			else
			{
				ImGui::PopStyleColor();
			}

			return changed;
		}
	}

	TodoListPanel::TodoListPanel(EditorContext& context)
	{
		newItemBuffer_.resize(256);
		Load();
	}

	void TodoListPanel::Load()
	{
		if (!std::filesystem::exists("../Editor/TodoList.json"))
		{
			return;
		}

		JsonInputArchive archive;
		if (!archive.Read("../Editor/TodoList.json"))
		{
			items_.clear();
			return;
		}

		archive.Field("items", items_);
	}

	void TodoListPanel::Save()
	{
		JsonOutputArchive archive;
		archive.Field("items", items_);
		archive.Write("../Editor/TodoList.json");
	}

	void TodoListPanel::Open()
	{
		show_ = true;
	}

	void TodoListPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 440), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("ToDoリスト", &show_, flags))
		{
			Bool changed = false;

			ImGui::SetNextItemWidth(-180.0f);
			Bool entered = ImGui::InputText("##NewTodoItem", newItemBuffer_.data(), newItemBuffer_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70.0f);
			PriorityCombo("##NewTodoPriority", newItemPriority_);
			ImGui::SameLine();
			Bool addClicked = ImGui::Button("追加", ImVec2(80.0f, 0));

			if (entered || addClicked)
			{
				std::string text(newItemBuffer_.c_str());
				if (!text.empty())
				{
					TodoItem item{};
					item.text_ = text;
					item.done_ = false;
					item.priority_ = newItemPriority_;
					items_.push_back(item);
					changed = true;
				}
				std::ranges::fill(newItemBuffer_, '\0');
				ImGui::SetKeyboardFocusHere(-1);
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::BeginTable("##TodoTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
			{
				ImGui::TableSetupColumn("##done", ImGuiTableColumnFlags_WidthFixed, 36.0f);
				ImGui::TableSetupColumn("##priority", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("##text", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("##delete", ImGuiTableColumnFlags_WidthFixed, 32.0f);

				Int32 removeIndex = -1;
				for (Int32 itemIndex = 0; itemIndex < static_cast<Int32>(items_.size()); ++itemIndex)
				{
					TodoItem& item = items_[itemIndex];

					ImGui::TableNextRow();
					ImGui::PushID(itemIndex);

					ImGui::TableNextColumn();
					Bool done = item.done_;
					if (ImGui::Checkbox("##Done", &done))
					{
						item.done_ = done;
						changed = true;
					}

					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (PriorityCombo("##Priority", item.priority_))
					{
						changed = true;
					}

					ImGui::TableNextColumn();
					if (item.done_)
					{
						ImGui::TextDisabled("%s", item.text_.c_str());
					}
					else
					{
						ImGui::TextUnformatted(item.text_.c_str());
					}

					ImGui::TableNextColumn();
					if (ImGui::SmallButton("×"))
					{
						removeIndex = itemIndex;
					}

					ImGui::PopID();
				}

				if (removeIndex >= 0)
				{
					items_.erase(items_.begin() + removeIndex);
					changed = true;
				}

				ImGui::EndTable();
			}

			ImGui::Separator();

			Uint doneCount = 0;
			for (const TodoItem& item : items_)
			{
				if (item.done_)
				{
					doneCount++;
				}
			}
			ImGui::Text("完了: %u / %u", doneCount, static_cast<Uint>(items_.size()));

			ImGui::SameLine();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				show_ = false;
			}

			if (changed)
			{
				Save();
			}
		}
		ImGui::End();
	}
}
