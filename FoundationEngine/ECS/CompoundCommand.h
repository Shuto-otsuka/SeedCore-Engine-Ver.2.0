#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>

namespace SeedCore
{
	/**
	* [EN]
	* Undo/redo command that groups several sub-commands into a single
	* history entry, so one Ctrl+Z reverts all of them at once. Used where
	* a single user gesture produces many edits - chiefly a viewport gizmo
	* drag over a multi-selection, which otherwise pushes one
	* ComponentCommand per selected actor per changed channel (position/
	* rotation/scale) and leaves the group half-reverted after one Undo.
	* Redo re-applies the sub-commands in the order they were added; Undo
	* reverts them in reverse order.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 複数のサブコマンドを1つの履歴エントリにまとめ、Ctrl+Z 一回で全部を
	* 取り消せるようにする Undo/Redo コマンド。1回のユーザー操作が多数の
	* 編集を生む場面で使う - 主にビューポートのギズモで複数選択をドラッグ
	* した場合。まとめないと、選択 actor ごと・変化したチャンネル
	* (position/rotation/scale) ごとに ComponentCommand が積まれ、Undo
	* 一回ではグループが中途半端に戻った状態になる。Redo は追加された順に、
	* Undo は逆順にサブコマンドを処理する。
	*/
	class SEEDCORE_API CompoundCommand : public Command
	{
	public:
		CompoundCommand() = default;

		/// [EN] commands_ holds move-only ResourcePtr entries, and class SEEDCORE_API forces the compiler to instantiate every implicit special member for DLL export - so copy must be explicitly deleted (an implicit copy would fail to compile copying those entries) and move explicitly defaulted. Mirrors History.
		/// [JP] commands_ はムーブ専用の ResourcePtr エントリを保持し、class SEEDCORE_API は DLL エクスポートのため暗黙の特殊メンバ全てをコンパイラに実体化させる - そのためコピーは明示的に delete し(暗黙のコピーはそれらのエントリのコピーでコンパイルに失敗する)、ムーブは明示的に default にする。History と同じ。
		CompoundCommand(const CompoundCommand&) = delete;
		CompoundCommand& operator=(const CompoundCommand&) = delete;
		CompoundCommand(CompoundCommand&&) = default;
		CompoundCommand& operator=(CompoundCommand&&) = default;

		/**
		* [EN]
		* Appends command as the next sub-command. It must already have
		* been applied by the caller (like History::Push expects).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* command を次のサブコマンドとして追加する。History::Push と同様、
		* 呼び出し側で既に適用済みであること。
		*/
		void Add(ResourcePtr<Command> command);

		/**
		* [EN]
		* Returns whether no sub-command has been added (caller can skip
		* pushing an empty group onto the history).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* サブコマンドが1つも追加されていないかを返す(呼び出し側は空の
		* グループを履歴へ積むのを省ける)。
		*/
		Bool Empty()const;

		/**
		* [EN]
		* Re-applies every sub-command, in the order they were added.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全サブコマンドを、追加された順に再適用する。
		*/
		void Redo()override;

		/**
		* [EN]
		* Reverts every sub-command, in reverse of the order they were added.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全サブコマンドを、追加された順の逆順で取り消す。
		*/
		void Undo()override;

	private:
		/// [EN] Sub-commands in the order they were applied; Undo walks them in reverse.
		/// [JP] 適用された順のサブコマンド。Undo は逆順にたどる。
		DynamicArray<ResourcePtr<Command>> commands_;
	};
}
