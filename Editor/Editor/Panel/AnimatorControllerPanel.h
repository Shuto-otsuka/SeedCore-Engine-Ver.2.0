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

	private:
		void DrawNodeEditor();

	private:
		EditorContext& context_;

		Bool show_ = false;
		Animator* target_ = nullptr;
		Bool needsPositionSync_ = false;

		Size middleDragStateIndex_ = SIZE_MAX;
		Bool middleDraggingEntry_ = false;
		Bool middleDraggingExit_ = false;
		Size selectedStateIndex_ = SIZE_MAX;
		Size selectedTransitionIndex_ = SIZE_MAX;
		Size selectedConditionIndex_ = SIZE_MAX;

		Size pendingFromStateIndex_ = SIZE_MAX;
		Float pendingFromOffsetX_ = 0.0f;
		Float pendingFromOffsetY_ = 0.0f;

		ax::NodeEditor::EditorContext* nodeEditorContext_ = nullptr;
	};
}
