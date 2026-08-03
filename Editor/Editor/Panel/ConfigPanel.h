#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/Build/Config.h>

namespace SeedCore
{
	struct EditorContext;

	class ConfigPanel
	{
	public:
		ConfigPanel(EditorContext& context);
		~ConfigPanel() = default;

		void Draw();

		void Open();

	private:
		Bool DrawEditorConfigTab();

		Bool DrawGameConfigTab();

		void DrawInputBindingTab();

	private:
		EditorContext& context_;

		Bool show_ = false;

		EditorConfig editorConfig_;

		GameConfig gameConfig_;

		std::string initialScenePathBuffer_;

		std::string newActionBuffer_;
	};
}
