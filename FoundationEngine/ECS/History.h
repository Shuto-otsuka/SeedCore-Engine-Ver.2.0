#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>

namespace SeedCore
{
	/**
	* [EN]
	* Ctrl+Z/Ctrl+Y undo/redo history. Holds two stacks of Command (see
	* Command) - one entry per edit - and lets Undo/Redo step backward and
	* forward through them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Ctrl+Z/Ctrl+Y Undo/Redo履歴。Command(Command参照)を2本のスタックとして
	* 保持し(編集1回につき1エントリ)、Undo/Redoでそれらを前後にたどれる
	* ようにする。
	*/
	class SEEDCORE_API History
	{
	public:
		History() = default;

		/// [EN] Command holds move-only ResourcePtr entries (see ResourcePtr's copy-vs-move rationale), and class SEEDCORE_API forces the compiler to instantiate every implicit special member for DLL export - so copy must be explicitly deleted here (an implicit copy assignment would otherwise fail to compile trying to copy those entries) and move explicitly defaulted.
		/// [JP] Commandはムーブ専用のResourcePtrエントリを保持する(ResourcePtrのコピー/ムーブ方針を参照)。class SEEDCORE_APIはDLLエクスポートのため暗黙の特殊メンバ関数全てをコンパイラに実体化させる - そのためコピーはここで明示的にdeleteする必要があり(暗黙のコピー代入はそれらのエントリをコピーしようとしてコンパイルに失敗する)、ムーブは明示的にdefaultにする。
		History(const History&) = delete;
		History& operator=(const History&) = delete;
		History(History&&) = default;
		History& operator=(History&&) = default;

		/**
		* [EN]
		* Pushes command (already applied by the caller) onto the undo
		* stack, and discards the redo stack - a fresh edit invalidates any
		* previously undone future. Oldest entries are dropped once the
		* stack exceeds maxDepth_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* (呼び出し側で既に適用済みの)command をUndoスタックへ積み、
		* Redoスタックを破棄する(新しい編集が行われると、それ以前にUndoした
		* 「未来」は無効になるため)。スタックが maxDepth_ を超えたら最古の
		* ものから破棄する。
		*/
		void Push(ResourcePtr<Command> command);

		/**
		* [EN]
		* Steps one entry back: pops the most recent undo entry, calls its
		* Undo(), and moves it onto the redo stack. Does nothing if the
		* undo stack is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1エントリ分過去へ戻る: 最新のUndoエントリをポップしてUndo()を
		* 呼び、Redoスタックへ移す。Undoスタックが空であれば何もしない。
		*/
		void Undo();

		/**
		* [EN]
		* Steps one entry forward: pops the most recent redo entry, calls
		* its Redo(), and moves it onto the undo stack. Does nothing if the
		* redo stack is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1エントリ分未来へ進む: 最新のRedoエントリをポップしてRedo()を
		* 呼び、Undoスタックへ移す。Redoスタックが空であれば何もしない。
		*/
		void Redo();

	private:
		/// [EN] Maximum number of undo entries retained before the oldest is dropped.
		/// [JP] 保持するUndoエントリの最大数。超えると最古のものから破棄する。
		static constexpr Size maxDepth_ = 100;

		/// [EN] Commands applied so far, most recent last.
		/// [JP] これまでに適用されたコマンド。最新のものが末尾。
		DynamicArray<ResourcePtr<Command>> undoStack_;

		/// [EN] Commands undone so far, most recent last.
		/// [JP] これまでにUndoされたコマンド。最新のものが末尾。
		DynamicArray<ResourcePtr<Command>> redoStack_;
	};
}
