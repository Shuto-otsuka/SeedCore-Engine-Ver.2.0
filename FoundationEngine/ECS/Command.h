#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Base type for one undoable/redoable edit. A concrete Command holds
	* everything needed to both re-apply and revert a single edit (e.g.
	* one component field going from an old value to a new one).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Undo/Redo可能な1回の編集を表す基底型。具体的なCommandは、1つの編集
	* (例: 1つのコンポーネントフィールドが旧値から新値へ変わったこと)を
	* 再適用/取り消しの両方向に行うために必要な情報を全て持つ。
	*/
	class SEEDCORE_API Command
	{
	public:
		/**
		* [EN]
		* Virtual destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		virtual ~Command() = default;

		/**
		* [EN]
		* Re-applies this edit (the new value). Called on Redo.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集(新値)を再適用する。Redo時に呼ばれる。
		*/
		virtual void Redo() = 0;

		/**
		* [EN]
		* Reverts this edit (back to the old value). Called on Undo.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集を取り消し、旧値へ戻す。Undo時に呼ばれる。
		*/
		virtual void Undo() = 0;
	};
}
