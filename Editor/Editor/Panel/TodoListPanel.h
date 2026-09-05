#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	enum class TodoPriority : Int32
	{
		Low = 0,
		Mid = 1,
		High = 2,
	};

	struct TodoItem
	{
		std::string text_;
		Bool done_ = false;
		TodoPriority priority_ = TodoPriority::Mid;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			Int32 priorityValue = static_cast<Int32>(priority_);
			archive.Field("text", text_);
			archive.Field("done", done_);
			archive.Field("priority", priorityValue);
			priority_ = static_cast<TodoPriority>(priorityValue);
		}
	};

	class TodoListPanel
	{
	public:
		TodoListPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~TodoListPanel() = default;

		void Draw();

		void Open();

	private:
		void Load();

		void Save();

		ImGuiTexture& imguiTexture_;

		Bool show_ = false;

		DynamicArray<TodoItem> items_;

		std::string newItemBuffer_;

		TodoPriority newItemPriority_ = TodoPriority::Mid;
	};
}
