#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	struct EditorContext;
	class Actor;
	class ImGuiTexture;

	class AddComponentPanel
	{
	public:
		AddComponentPanel(EditorContext& context);

		void Draw(Actor* actor, ImGuiTexture& imguiTexture);

	private:
		struct State
		{
			String searchBuffer;
			String selectedName;
		};

		void DrawSearchBar(ImGuiTexture& imguiTexture);
		void DrawComponentList(Actor* actor);

		/// [EN] Renders one leaf entry as a Selectable: single click selects
		///      (highlight only, popup stays open via NoAutoClosePopups),
		///      double click adds the component and closes the popup chain.
		///      Already-attached components are shown but disabled.
		/// [JP] 1つの葉ノードを Selectable として描画する。単クリックは選択
		///      のみ（NoAutoClosePopups でポップアップは閉じない）、
		///      ダブルクリックでコンポーネントを追加しポップアップ階層を
		///      閉じる。既に付いているコンポーネントは表示はするが無効化する。
		void DrawMenuItem(Actor* actor, const String& componentName, ComponentID componentID);

		Bool CheckBuiltinComponent(const String& componentName)const;
		Bool CheckFilterMatch(const String& componentName, const std::string& filterText)const;

		EditorContext& context_;

		State state_;
	};
}
