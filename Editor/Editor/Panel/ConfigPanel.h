#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Config/GameConfig.h>
#include <FoundationEngine/Resource/Config/EditorConfig.h>
#include <FoundationEngine/Resource/Config/IconConfig.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	class ConfigPanel
	{
	public:
		ConfigPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~ConfigPanel() = default;

		void Draw();

		void Open();

	private:
		Bool DrawEditorConfigTab();

		Bool DrawGameConfigTab();

		Bool DrawAudioBindingTab();

		Bool DrawIconConfigTab();

		void DrawInputBindingTab();

	private:
		enum class ConfigCategory
		{
			Editor,
			Game,
			Audio,
			Icon,
			Input,
		};

	private:
		EditorContext& context_;

		ImGuiTexture& imguiTexture_;

		Bool show_ = false;

		ConfigCategory selectedCategory_ = ConfigCategory::Editor;

		EditorConfig editorConfig_;

		GameConfig gameConfig_;

		IconConfig iconConfig_;

		std::string initialScenePathBuffer_;

		std::string executableNameBuffer_;

		std::string newActionBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> iconPreviewResource_;

		Uint iconPreviewIndex_ = 0;

		Bool iconPreviewDirty_ = true;
	};
}
