#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;
	class Actor;

	class HierarchyPanel
	{
	public:
		HierarchyPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~HierarchyPanel() = default;

		void Draw();

	private:
		void DrawActorNode(Actor* actor);

		void SaveAsPrefab(Actor* actor);

		String GetUniqueName();

		void HandleNodeSelection(Actor* actor, Bool ctrl, Bool shift);

		Bool IsSelected(Actor* actor)const;

		void DeleteActor(Actor* actor);

		void DeleteSelection();

		void DuplicateSelection();

		/**
		* [EN]
		* Repositions `actor` immediately after `after` among its siblings
		* (or among root Actors if both are parentless).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* `actor` を、その兄弟の中で `after` の直後に再配置する
		* （両者が親を持たない場合はルート Actor の中で）。
		*/
		void MoveAfter(Actor* actor, Actor* after);

	private:
		EditorContext& context_;

		ImGuiTexture& imguiTexture_;

		struct RowRect
		{
			Actor* actor_ = nullptr;
			ImVec2 min_;
			ImVec2 max_;
		};

		/// [EN] Rects of every Actor row drawn this frame, in visual (depth-first) order.
		///      Used for Shift range-select and marquee drag-select hit testing.
		/// [JP] このフレームで描画された全 Actor 行の矩形（表示上の深さ優先順）。
		///      Shift範囲選択とマーキー矩形選択のヒット判定に使う。
		DynamicArray<RowRect> rows_;

		Actor* pendingClickActor_ = nullptr;
		Bool pendingClickCtrl_ = false;
		Bool pendingClickShift_ = false;

		Actor* rangeAnchor_ = nullptr;

		Bool marqueeActive_ = false;
		ImVec2 marqueeStart_ = ImVec2(0.0f, 0.0f);
	};
}
