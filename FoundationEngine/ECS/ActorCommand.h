#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>
#include <FoundationEngine/Resource/ActorSerialization.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	class Actor;

	/**
	* [EN]
	* Undo/redo command for creating one Actor subtree - a single Actor
	* (the Hierarchy panel's "空のActorを作成"/"空のActorを追加") or a
	* whole captured hierarchy (the Hierarchy panel's "複製", which
	* clones an Actor and every descendant). nodes_ is the flat,
	* parent-index-linked capture (see CaptureActorNode) with nodes_[0]
	* as the root; Redo replays it exactly like Scene::Instantiate does,
	* parenting the root under whatever actor currently holds
	* parentPersistentId_ (or as a root Actor if 0/gone). Undo destroys
	* every actor it instantiated, re-resolved by persistent ID each time
	* rather than cached, since Redo builds brand new Actor instances.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1つのActorサブツリー作成に対するUndo/Redoコマンド - 単一Actor
	* (Hierarchyパネルの「空のActorを作成」/「空のActorを追加」)、または
	* 取得済みの階層まるごと(Hierarchyパネルの「複製」— Actorとその全子孫を
	* 複製する)のいずれか。nodes_はフラットな親インデックス連結の取得結果
	* (CaptureActorNode参照)で、nodes_[0]がルート。RedoはScene::Instantiate
	* と全く同じ手順で再生し、ルートをparentPersistentId_を現在保持している
	* actorの下へ親付けする(0/存在しなければルートActorとして)。Undoは
	* インスタンス化した全actorを破棄する - Redoのたびに新しいActor
	* インスタンスが作られるため、キャッシュせず毎回永続IDで再解決する。
	*/
	class SEEDCORE_API ActorCreateCommand : public Command
	{
	public:
		/**
		* [EN]
		* world/cache are used to (re)instantiate/destroy the actors; nodes
		* is the captured subtree (see CaptureActorNode), already carrying
		* the persistent IDs it was created with; parentPersistentId is 0
		* for a root-level subtree.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world/cacheはactor群の(再)インスタンス化/破棄に使う。nodesは
		* 取得済みのサブツリー(CaptureActorNode参照)であり、作成時に
		* 割り当てられた永続IDを既に持っている。parentPersistentIdは
		* ルートレベルのサブツリーであれば0。
		*/
		ActorCreateCommand(World& world, ResourceCache& cache, DynamicArray<SerializedActorNode> nodes, Uint32 parentPersistentId = 0);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		ResourceCache& cache_;
		DynamicArray<SerializedActorNode> nodes_;
		Uint32 parentPersistentId_;
	};

	/**
	* [EN]
	* Undo/redo command for deleting a single Actor (the Hierarchy
	* panel's "Actorを削除"/Delete key). Matches
	* World::DestroyActor's own behavior of detaching (not destroying)
	* children - node_ captures only actor's own data (not its
	* descendants, which stay alive as orphans), and childPersistentIds_
	* records which live actors to re-parent back under it on Undo.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一Actor削除(Hierarchyパネルの「Actorを削除」/Deleteキー)に対する
	* Undo/Redoコマンド。World::DestroyActor自身の挙動(子を破棄せず切り
	* 離すだけ)に合わせ、node_はactor自身のデータのみを取得する(子孫は
	* 破棄されず孤立したまま生き続ける)。childPersistentIds_はUndo時に
	* 再びこの下へ親付けし直す、生きている子actor群を記録する。
	*/
	class SEEDCORE_API ActorDeleteCommand : public Command
	{
	public:
		/**
		* [EN]
		* Captures actor's own data plus its current parent/children (by
		* persistent ID) - call before World::DestroyActor(actor) actually
		* runs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actor自身のデータと、現在の親/子(永続ID経由)を取得する -
		* World::DestroyActor(actor)が実際に実行される前に呼ぶこと。
		*/
		ActorDeleteCommand(World& world, ResourceCache& cache, Actor* actor);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		ResourceCache& cache_;
		SerializedActorNode node_;
		Uint32 persistentId_ = 0;
		Uint32 parentPersistentId_ = 0;
		DynamicArray<Uint32> childPersistentIds_;
	};

	/**
	* [EN]
	* Undo/redo command for re-parenting an Actor (the Hierarchy panel's
	* drag-and-drop reparenting, including dropping onto the empty area to
	* clear the parent). Both the actor and its old/new parent are
	* re-resolved by persistent ID each time rather than cached.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actorの再親付け(Hierarchyパネルのドラッグ&ドロップによる再親付け。
	* 空白領域へのドロップによる親解除も含む)に対するUndo/Redoコマンド。
	* actor自身とその新旧の親は、キャッシュせず毎回永続IDで再解決する。
	*/
	class SEEDCORE_API ActorReparentCommand : public Command
	{
	public:
		/// [EN] oldParentPersistentId/newParentPersistentId are 0 for a root (no parent).
		/// [JP] oldParentPersistentId/newParentPersistentIdは、親が無い(ルート)場合0。
		ActorReparentCommand(World& world, Uint32 actorPersistentId, Uint32 oldParentPersistentId, Uint32 newParentPersistentId);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		Uint32 actorPersistentId_;
		Uint32 oldParentPersistentId_;
		Uint32 newParentPersistentId_;
	};

	/**
	* [EN]
	* Undo/redo command for adding/removing a single tag on a single
	* Actor (the Inspector's tag popup and per-tag checkbox list -
	* not the "タグを削除（すべてのActorから）" context menu item, which
	* mutates the global TagRegistry across every Actor holding the tag
	* and is out of scope here). The actor is re-resolved by persistent
	* ID each time rather than cached.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一Actorへの単一タグの追加/削除(Inspectorのタグポップアップと
	* タグ別チェックボックス一覧 - 「タグを削除（すべてのActorから）」
	* コンテキストメニュー項目は対象外。これはそのタグを持つ全Actorに
	* 渡ってグローバルなTagRegistryを変更するため、ここでは扱わない)に
	* 対するUndo/Redoコマンド。actorはキャッシュせず毎回永続IDで再解決する。
	*/
	class SEEDCORE_API ActorTagCommand : public Command
	{
	public:
		/// [EN] addOnRedo selects the direction: true for adding tag (Redo adds, Undo removes), false for removing it (Redo removes, Undo adds).
		/// [JP] addOnRedoが方向を選ぶ: trueならtagの追加(Redoで追加、Undoで削除)、falseなら削除(Redoで削除、Undoで追加)。
		ActorTagCommand(World& world, Uint32 actorPersistentId, const String& tag, Bool addOnRedo);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		Uint32 actorPersistentId_;
		String tag_;
		Bool addOnRedo_;
	};

	/**
	* [EN]
	* Undo/redo command for toggling an Actor's active state (the
	* Inspector's "有効" checkbox). Actor::SetActive cascades onto every
	* descendant, so the constructor recursively captures the current
	* active state of actor and every descendant (by persistent ID)
	* before the toggle is applied; Redo re-runs the cascading
	* SetActive(newActive_), but Undo restores each captured entry's
	* individual prior state directly (not via another cascading
	* SetActive call), so a descendant that already differed from its
	* parent before the edit comes back correctly instead of being
	* forced to match the parent.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actorのアクティブ状態切り替え(Inspectorの「有効」チェックボックス)に
	* 対するUndo/Redoコマンド。Actor::SetActiveは全子孫へ連鎖するため、
	* コンストラクタは切り替え前の時点でactorと全子孫の現在のアクティブ
	* 状態を(永続ID付きで)再帰的に取得しておく。Redoは連鎖する
	* SetActive(newActive_)を再実行するが、Undoは取得済みの各エントリの
	* 個別の元の状態を(連鎖するSetActive呼び出しではなく)直接復元する -
	* 編集前に親と異なる状態だった子孫が、親に強制的に合わせられることなく
	* 正しく元へ戻るようにするため。
	*/
	class SEEDCORE_API ActorActiveCommand : public Command
	{
	public:
		/**
		* [EN]
		* Recursively captures the current active state of actor and every
		* descendant - call before Actor::SetActive(newActive) actually
		* runs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorと全子孫の現在のアクティブ状態を再帰的に取得する -
		* Actor::SetActive(newActive)が実際に実行される前に呼ぶこと。
		*/
		ActorActiveCommand(World& world, Actor* actor, Bool newActive);

		void Redo()override;

		void Undo()override;

	private:
		struct Entry
		{
			Uint32 persistentId_ = 0;
			Bool oldActive_ = true;
		};

		World& world_;
		Uint32 rootPersistentId_;
		Bool newActive_;
		DynamicArray<Entry> entries_;
	};
}
