#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Shared identifiers for the reflected-array undo/redo commands. The
	* target array is named by (entity, componentID, fieldName); its live
	* ArrayInfo::add_/remove_/lastPtr_ hooks are re-resolved from
	* ReflectionRegistry inside each subclass's Redo/Undo rather than
	* cached - the std::functions the Inspector hands out capture the
	* component instance by reference, so an archetype migration or
	* sparse-set compaction between the edit and the undo would leave them
	* dangling.
	*
	* Only a top-level array field of the component is re-resolvable this
	* way; an array nested inside a reflected sub-struct is not found by
	* name and its Redo/Undo becomes a no-op (previously: undefined behavior).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクション配列の Undo/Redo コマンドが共有する識別子。対象の
	* 配列を (entity, componentID, fieldName) で名指しする。その生きた
	* ArrayInfo::add_/remove_/lastPtr_ フックは、キャッシュせず各サブ
	* クラスの Redo/Undo の中で ReflectionRegistry から再解決する -
	* Inspector が渡す std::function はコンポーネントインスタンスを
	* 参照キャプチャしており、編集から Undo までの間にアーキタイプ移行や
	* スパースセットの詰め替えが起きると dangling になるため。
	*
	* この方法で再解決できるのはコンポーネント直下の配列フィールドのみ。
	* リフレクション対象のサブ構造体内にネストされた配列は名前で見つからず、
	* その Redo/Undo は無操作になる(以前: 未定義動作)。
	*/
	class SEEDCORE_API ArrayFieldCommand : public Command
	{
	protected:
		/**
		* [EN]
		* entity/componentID/fieldName name the reflected array field this
		* command edits; index is the row the append/remove happened at.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity/componentID/fieldName が、このコマンドが編集するリフレク
		* ション配列フィールドを名指しする。index は追加/削除が起きた行。
		*/
		ArrayFieldCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index);

	protected:
		/// [EN] The World the array-holding component lives in.
		/// [JP] 配列を持つコンポーネントが属する World。
		World& world_;

		/// [EN] Entity whose component holds the array.
		/// [JP] 配列を持つコンポーネントの Entity。
		Entity entity_;

		/// [EN] The array-holding component's registered ID.
		/// [JP] 配列を持つコンポーネントの登録済み ID。
		ComponentID componentID_;

		/// [EN] Display name of the array field within that component.
		/// [JP] そのコンポーネント内での配列フィールドの表示名。
		String fieldName_;

		/// [EN] Index the removed/appended element sits at; stable within one Ctrl+Z step (nothing else resizes this array in between).
		/// [JP] 削除/追加された要素があるインデックス。単一の Ctrl+Z ステップの間は安定(その間に他が同じ配列をリサイズしない)。
		Size index_;
	};

	/**
	* [EN]
	* Undo/redo command for the Inspector's "+" button on a plain
	* (non-payload) reflected array field: Redo appends one
	* default-constructed element, Undo removes it again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレーンな(Payload でない)リフレクション配列フィールドに対する
	* Inspector の「+」ボタンの Undo/Redo コマンド: Redo は
	* デフォルト構築済みの要素を1つ追加し、Undo はそれを再び削除する。
	*/
	class SEEDCORE_API ArrayAppendCommand : public ArrayFieldCommand
	{
	public:
		/**
		* [EN]
		* Forwards to ArrayFieldCommand; index is the appended element's
		* position (the array size before the append).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ArrayFieldCommand へ委譲する。index は追加された要素の位置
		* (追加前の配列サイズ)。
		*/
		ArrayAppendCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index);

		/**
		* [EN]
		* Re-resolves the target array and appends one default-constructed
		* element via its live add_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 対象の配列を再解決し、その生きた add_ でデフォルト構築済みの
		* 要素を1つ追加する。
		*/
		void Redo()override;

		/**
		* [EN]
		* Re-resolves the target array and removes the element Redo appended
		* via its live remove_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 対象の配列を再解決し、その生きた remove_ で Redo が追加した要素を
		* 削除する。
		*/
		void Undo()override;
	};

	/**
	* [EN]
	* Undo/redo command for adding an element to, or removing one from, a
	* payload (asset-reference) reflected array field - the Inspector's
	* drop-to-append slot and its per-row "削除" context menu. A removed
	* element's value is restored (re-appended) but its original position
	* is not: ArrayInfo has only append, no positional insert.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Payload(アセット参照)リフレクション配列フィールドへ要素を追加、
	* または要素を削除する操作の Undo/Redo コマンド - Inspector の
	* ドロップ追加枠と、行ごとの「削除」コンテキストメニュー。削除された
	* 要素の値は復元(再追加)されるが、元の位置は復元されない:
	* ArrayInfo には append しかなく、位置指定の挿入がないため。
	*/
	class SEEDCORE_API PayloadArrayCommand : public ArrayFieldCommand
	{
	public:
		/**
		* [EN]
		* value is the element's payload (asset ID). addOnRedo selects the
		* direction: true = Redo appends and writes value / Undo removes;
		* false = Redo removes / Undo re-appends and re-writes value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value は要素の Payload(アセット ID)。addOnRedo が方向を選ぶ:
		* true = Redo で追加して value を書き込み / Undo で削除、
		* false = Redo で削除 / Undo で再追加して value を書き戻す。
		*/
		PayloadArrayCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index, Int value, Bool addOnRedo);

		/**
		* [EN]
		* Appends value_ (or removes index_, when addOnRedo_ is false),
		* re-resolving the array each time.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 毎回配列を再解決して value_ を追加する(addOnRedo_ が false の
		* ときは index_ を削除)。
		*/
		void Redo()override;

		/**
		* [EN]
		* Reverses Redo: removes index_ (or re-appends value_).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Redo の逆: index_ を削除する(または value_ を再追加する)。
		*/
		void Undo()override;

	private:
		/// [EN] The payload value (asset ID) of the added/removed element; re-written on the append direction.
		/// [JP] 追加/削除された要素の Payload 値(アセット ID)。追加方向で書き戻される。
		Int value_;

		/// [EN] true = Redo appends, false = Redo removes.
		/// [JP] true = Redo で追加、false = Redo で削除。
		Bool addOnRedo_;
	};
}
