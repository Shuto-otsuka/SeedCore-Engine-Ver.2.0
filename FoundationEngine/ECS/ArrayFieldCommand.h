#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>

namespace SeedCore
{
	/**
	* [EN]
	* Undo/redo command for appending a default-constructed element to a
	* plain (non-payload) reflected array field via ArrayInfo::add_ (the
	* Inspector's "+" button). Undo removes it again via
	* ArrayInfo::remove_ - safe because index_ (the appended element's
	* index at push time) never shifts between the append and the undo,
	* since nothing else can resize this same array in between within a
	* single Ctrl+Z step.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ArrayInfo::add_(Inspectorの「+」ボタン)経由で、プレーンな
	* (Payloadでない)リフレクション配列フィールドへデフォルト構築済みの
	* 要素を追加する操作に対するUndo/Redoコマンド。UndoはArrayInfo::remove_
	* 経由で再び削除する - index_(追加時点でのその要素のインデックス)は
	* 追加からUndoまでの間ずれない(単一のCtrl+Zステップの間に、他の操作が
	* 同じ配列をリサイズすることはないため)ので安全。
	*/
	class SEEDCORE_API ArrayAppendCommand : public Command
	{
	public:
		ArrayAppendCommand(std::function<void()> add, std::function<void(Size)> remove, Size index);

		void Redo()override;

		void Undo()override;

	private:
		std::function<void()> add_;
		std::function<void(Size)> remove_;
		Size index_;
	};

	/**
	* [EN]
	* Undo/redo command for appending an element to, or removing an
	* element from, a payload (asset-reference) array field via
	* ArrayInfo::add_/remove_/lastPtr_ - covers both the Inspector's
	* drop-to-append slot and its per-row "削除" context menu, since both
	* are mirror images of the same add-then-write/remove-after-reading
	* sequence. On a remove, Undo re-appends value_ at the end of the
	* array rather than at its original index_ - ArrayInfo has no
	* positional-insert primitive, only append, so a removed element's
	* value is always fully restored but its original ordering isn't.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ArrayInfo::add_/remove_/lastPtr_経由で、Payload(アセット参照)配列
	* フィールドへ要素を追加、または要素を削除する操作に対するUndo/Redo
	* コマンド。Inspectorのドロップ追加枠と、行ごとの「削除」コンテキスト
	* メニューの両方をカバーする - どちらも「追加してから書き込む/削除の
	* 前に読み取る」という同じ手順の裏表であるため。削除の場合、Undoは
	* value_を元のindex_ではなく配列の末尾へ再追加する - ArrayInfoには
	* 位置指定の挿入手段がなくappendしかないため、削除された要素の値は
	* 常に完全に復元されるが、元の並び順は復元されない。
	*/
	class SEEDCORE_API PayloadArrayCommand : public Command
	{
	public:
		/// [EN] addOnRedo selects the direction: true for an append (Redo appends+writes value, Undo removes), false for a removal (Redo removes, Undo appends+writes value).
		/// [JP] addOnRedoが方向を選ぶ: trueなら追加(Redoで追加+書き込み、Undoで削除)、falseなら削除(Redoで削除、Undoで追加+書き込み)。
		PayloadArrayCommand(std::function<void()> add, std::function<void(Size)> remove, std::function<void*()> lastPtr, Size index, Int value, Bool addOnRedo);

		void Redo()override;

		void Undo()override;

	private:
		void Append();

		void Remove();

		std::function<void()> add_;
		std::function<void(Size)> remove_;
		std::function<void*()> lastPtr_;
		Size index_;
		Int value_;
		Bool addOnRedo_;
	};
}
