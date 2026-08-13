#pragma once
#include <FoundationEngine/Prelude.h>

namespace ax
{
	namespace NodeEditor
	{
		struct EditorContext;
	}
}

namespace SeedCore
{
	struct EditorContext;
	class Animator;

	class AnimatorControllerPanel
	{
	public:
		AnimatorControllerPanel(EditorContext& context);
		~AnimatorControllerPanel();

		void Draw();

		void Open(Animator* target);

		void DrawDetails();

		[[nodiscard]] Bool IsFocused()const { return isFocused_; }

	private:
		void DrawNodeEditor();

	private:
		EditorContext& context_;

		Bool show_ = false;
		Bool isFocused_ = false;
		Animator* target_ = nullptr;
		Bool needsPositionSync_ = false;

		Size selectedStateIndex_ = SIZE_MAX;
		Size selectedTransitionIndex_ = SIZE_MAX;
		Size selectedConditionIndex_ = SIZE_MAX;

		Bool creatingTransition_ = false;
		Bool creatingTransitionArmed_ = false;
		Int creatingTransitionSource_ = -1;
		Float creatingTransitionOffsetX_ = 0.0f;
		Float creatingTransitionOffsetY_ = 0.0f;

		Bool altPinsActive_ = false;
		Float altDragOffsetX_ = 0.0f;
		Float altDragOffsetY_ = 0.0f;

		Int contextMenuSource_ = -1;
		Float contextMenuCanvasX_ = 0.0f;
		Float contextMenuCanvasY_ = 0.0f;

		ax::NodeEditor::EditorContext* nodeEditorContext_ = nullptr;
	};
}
