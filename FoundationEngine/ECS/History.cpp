#include <FoundationEngine/ECS/History.h>

namespace SeedCore
{
	/**
	* [EN]
	* Pushes command (already applied by the caller) onto the undo stack,
	* and discards the redo stack - a fresh edit invalidates any
	* previously undone future. Oldest entries are dropped once the stack
	* exceeds maxDepth_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* (呼び出し側で既に適用済みの)command をUndoスタックへ積み、
	* Redoスタックを破棄する(新しい編集が行われると、それ以前にUndoした
	* 「未来」は無効になるため)。スタックが maxDepth_ を超えたら最古の
	* ものから破棄する。
	*/
	void History::Push(ResourcePtr<Command> command)
	{
		redoStack_.clear();

		undoStack_.push_back(std::move(command));

		if (undoStack_.size() > maxDepth_)
		{
			undoStack_.erase(undoStack_.begin());
		}
	}

	/**
	* [EN]
	* Steps one entry back: pops the most recent undo entry, calls its
	* Undo(), and moves it onto the redo stack. Does nothing if the undo
	* stack is empty.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1エントリ分過去へ戻る: 最新のUndoエントリをポップしてUndo()を呼び、
	* Redoスタックへ移す。Undoスタックが空であれば何もしない。
	*/
	void History::Undo()
	{
		if (undoStack_.empty())
		{
			return;
		}

		ResourcePtr<Command> command = std::move(undoStack_.back());
		undoStack_.pop_back();

		command->Undo();

		redoStack_.push_back(std::move(command));
	}

	/**
	* [EN]
	* Steps one entry forward: pops the most recent redo entry, calls its
	* Redo(), and moves it onto the undo stack. Does nothing if the redo
	* stack is empty.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1エントリ分未来へ進む: 最新のRedoエントリをポップしてRedo()を呼び、
	* Undoスタックへ移す。Redoスタックが空であれば何もしない。
	*/
	void History::Redo()
	{
		if (redoStack_.empty())
		{
			return;
		}

		ResourcePtr<Command> command = std::move(redoStack_.back());
		redoStack_.pop_back();

		command->Redo();

		undoStack_.push_back(std::move(command));
	}
}
