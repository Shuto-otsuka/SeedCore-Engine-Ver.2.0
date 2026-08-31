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
		* world/cache (re)instantiate and destroy the actors. nodes is the
		* captured subtree (see CaptureActorNode), already carrying the
		* persistent IDs it was created with. parentPersistentId is 0 for a
		* root-level subtree. prevSiblingPersistentId is the sibling the new
		* root should sit directly after under that parent (0 = append at
		* the end / no ordering to restore) - used by "複製" so Redo puts
		* the copy back next to its original instead of at the bottom.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world/cache が actor 群の(再)インスタンス化と破棄を行う。nodes は
		* 取得済みのサブツリー(CaptureActorNode 参照)で、作成時に割り当て
		* られた永続 ID を既に持っている。parentPersistentId はルート
		* レベルのサブツリーであれば 0。prevSiblingPersistentId は、その親の
		* 下で新しいルートを直後に置くべき兄弟(0 = 末尾へ追加 / 復元すべき
		* 並び順なし) - 「複製」が使い、Redo で copy を末尾ではなく元の隣へ
		* 戻せるようにする。
		*/
		ActorCreateCommand(World& world, ResourceCache& cache, DynamicArray<SerializedActorNode> nodes, Uint32 parentPersistentId = 0, Uint32 prevSiblingPersistentId = 0);

		/**
		* [EN]
		* Re-instantiates nodes_, parenting the root under whatever actor
		* currently holds parentPersistentId_ (or as a root if 0/gone), then
		* restores its sibling position from prevSiblingPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nodes_ を再インスタンス化し、ルートを parentPersistentId_ を現在
		* 保持している actor の下へ親付けし(0/存在しなければルートとして)、
		* その兄弟内の位置を prevSiblingPersistentId_ から復元する。
		*/
		void Redo()override;

		/**
		* [EN]
		* Destroys every actor currently holding one of nodes_'s persistent
		* IDs, from the deepest captured node back to the root.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nodes_ のいずれかの永続 ID を現在保持している全 actor を、最も
		* 深い取得ノードからルートへ向かって順に破棄する。
		*/
		void Undo()override;

	private:
		/// [EN] The World the actors live in.
		/// [JP] actor 群が属する World。
		World& world_;

		/// [EN] Cache used to resolve nested-prefab references while (re)instantiating.
		/// [JP] (再)インスタンス化時にネストされたプレハブ参照を解決するために使うキャッシュ。
		ResourceCache& cache_;

		/// [EN] Flat, parent-index-linked capture of the created subtree; nodes_[0] is the root.
		/// [JP] 作成されたサブツリーの、親インデックス連結のフラットな取得結果。nodes_[0] がルート。
		DynamicArray<SerializedActorNode> nodes_;

		/// [EN] Persistent ID of the actor the root is parented under (0 = root-level).
		/// [JP] ルートが親付けされている actor の永続 ID(0 = ルートレベル)。
		Uint32 parentPersistentId_;

		/// [EN] Persistent ID of the sibling the root sits directly after (0 = append / no order to restore).
		/// [JP] ルートが直後に位置する兄弟の永続 ID(0 = 末尾へ追加 / 復元すべき順序なし)。
		Uint32 prevSiblingPersistentId_;
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
		ActorDeleteCommand(World& world, ResourceCache& cache, Actor actor);

		/**
		* [EN]
		* Destroys whatever actor currently holds persistentId_ (its
		* children are detached, not destroyed, matching World::DestroyActor).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* persistentId_ を現在保持している actor を破棄する(その子は
		* World::DestroyActor と同じく破棄されず切り離される)。
		*/
		void Redo()override;

		/**
		* [EN]
		* Re-instantiates node_ under whatever actor currently holds
		* parentPersistentId_ (or as a root if none/gone), then re-parents
		* every still-alive child in childPersistentIds_ back onto it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* parentPersistentId_ を現在保持している actor の下へ node_ を再
		* インスタンス化し(無い/消えていればルートとして)、
		* childPersistentIds_ のうちまだ生きている子を全てこの下へ再び
		* 親付けし直す。
		*/
		void Undo()override;

	private:
		/// [EN] The World the actor lived in.
		/// [JP] actor が属していた World。
		World& world_;

		/// [EN] Cache used to resolve nested-prefab references while re-instantiating on Undo.
		/// [JP] Undo での再インスタンス化時にネストされたプレハブ参照を解決するために使うキャッシュ。
		ResourceCache& cache_;

		/// [EN] The deleted actor's own captured data (not its descendants).
		/// [JP] 削除された actor 自身の取得データ(子孫は含まない)。
		SerializedActorNode node_;

		/// [EN] The deleted actor's persistent ID.
		/// [JP] 削除された actor の永続 ID。
		Uint32 persistentId_ = 0;

		/// [EN] Persistent ID of the actor it was parented under at delete time (0 = root-level).
		/// [JP] 削除時に親付けされていた actor の永続 ID(0 = ルートレベル)。
		Uint32 parentPersistentId_ = 0;

		/// [EN] Persistent IDs of the children that were detached (not deleted) - re-parented back on Undo if still alive.
		/// [JP] 切り離された(削除はされていない)子の永続 ID 群 - まだ生きていれば Undo で再び親付けし直す。
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
		/**
		* [EN]
		* oldParentPersistentId/newParentPersistentId are 0 for a root (no
		* parent). oldPrevSiblingPersistentId is the actor that was directly
		* before this one under the old parent (0 if it was the first child
		* or was a root), so Undo restores its exact slot instead of
		* appending it to the end.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* oldParentPersistentId/newParentPersistentId は親が無い(ルート)場合
		* 0。oldPrevSiblingPersistentId は、旧親の下でこの actor の直前に
		* あった actor(先頭の子だった、またはルートだった場合は 0)。Undo が
		* 末尾へ追加するのではなく元の位置を正確に復元できるようにするため。
		*/
		ActorReparentCommand(World& world, Uint32 actorPersistentId, Uint32 oldParentPersistentId, Uint32 oldPrevSiblingPersistentId, Uint32 newParentPersistentId);

		/**
		* [EN]
		* Re-parents actorPersistentId_'s actor under whatever actor
		* currently holds newParentPersistentId_ (or as a root if 0/gone).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorPersistentId_ の actor を、newParentPersistentId_ を現在
		* 保持している actor の下へ再親付けする(0/存在しなければルートとして)。
		*/
		void Redo()override;

		/**
		* [EN]
		* Re-parents actorPersistentId_'s actor back under oldParentPersistentId_'s
		* actor (or as a root if 0/gone), then restores its original sibling
		* slot via oldPrevSiblingPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorPersistentId_ の actor を oldParentPersistentId_ の actor の
		* 下へ(0/存在しなければルートとして)再び親付けし直し、
		* oldPrevSiblingPersistentId_ で元の兄弟内の位置を復元する。
		*/
		void Undo()override;

	private:
		/// [EN] The World the actor lives in.
		/// [JP] actor が属する World。
		World& world_;

		/// [EN] Persistent ID of the re-parented actor.
		/// [JP] 再親付けされた actor の永続 ID。
		Uint32 actorPersistentId_;

		/// [EN] Persistent ID of the parent before the move (0 = was a root).
		/// [JP] 移動前の親の永続 ID(0 = ルートだった)。
		Uint32 oldParentPersistentId_;

		/// [EN] Persistent ID of the sibling directly before the actor under the old parent (0 = first child / root).
		/// [JP] 旧親の下で actor の直前にあった兄弟の永続 ID(0 = 先頭の子 / ルート)。
		Uint32 oldPrevSiblingPersistentId_;

		/// [EN] Persistent ID of the parent after the move (0 = becomes a root).
		/// [JP] 移動後の親の永続 ID(0 = ルートになる)。
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
		/**
		* [EN]
		* addOnRedo selects the direction: true for adding tag (Redo adds,
		* Undo removes), false for removing it (Redo removes, Undo adds).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* addOnRedo が方向を選ぶ: true なら tag の追加(Redo で追加、Undo で
		* 削除)、false なら削除(Redo で削除、Undo で追加)。
		*/
		ActorTagCommand(World& world, Uint32 actorPersistentId, const String& tag, Bool addOnRedo);

		/**
		* [EN]
		* Adds (or removes, when addOnRedo_ is false) tag_ on whatever actor
		* currently holds actorPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorPersistentId_ を現在保持している actor に対して tag_ を追加
		* する(addOnRedo_ が false のときは削除)。
		*/
		void Redo()override;

		/**
		* [EN]
		* Reverses Redo: removes tag_ (or adds it, when addOnRedo_ is false).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Redo の逆: tag_ を削除する(addOnRedo_ が false のときは追加)。
		*/
		void Undo()override;

	private:
		/// [EN] The World the actor lives in.
		/// [JP] actor が属する World。
		World& world_;

		/// [EN] Persistent ID of the tagged actor.
		/// [JP] タグ付けされた actor の永続 ID。
		Uint32 actorPersistentId_;

		/// [EN] The tag being added or removed.
		/// [JP] 追加または削除されるタグ。
		String tag_;

		/// [EN] true = Redo adds the tag and Undo removes it; false = the reverse.
		/// [JP] true = Redo でタグを追加し Undo で削除、false = その逆。
		Bool addOnRedo_;
	};

	/**
	* [EN]
	* Undo/redo command for changing a single Actor's layer (the
	* Inspector's layer dropdown). Unlike tags, an actor has exactly one
	* layer, so this stores oldLayerName_/newLayerName_ (by name, not
	* index, matching SerializedActorNode::layerName_) rather than a
	* toggle direction. actor isn't cached, and is re-resolved by
	* persistent ID each time, matching ActorTagCommand.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一Actorのレイヤー変更（Inspectorのレイヤードロップダウン）に対する
	* Undo/Redoコマンド。タグと違い actor のレイヤーは常に1つなので、
	* トグル方向ではなく oldLayerName_/newLayerName_（インデックスでは
	* なく名前で。SerializedActorNode::layerName_ と対応）を保持する。
	* actorはキャッシュせず、ActorTagCommandと同様に毎回永続IDで再解決する。
	*/
	class SEEDCORE_API ActorLayerCommand : public Command
	{
	public:
		/**
		* [EN]
		* oldLayerName/newLayerName are layer names (matching
		* SerializedActorNode::layerName_), not indices - an unknown name
		* falls back to the default layer on apply.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* oldLayerName/newLayerName はレイヤー名(SerializedActorNode::
		* layerName_ と対応)で、インデックスではない - 未知の名前は適用時に
		* デフォルトレイヤーへフォールバックする。
		*/
		ActorLayerCommand(World& world, Uint32 actorPersistentId, const String& oldLayerName, const String& newLayerName);

		/**
		* [EN]
		* Sets newLayerName_ on whatever actor currently holds actorPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorPersistentId_ を現在保持している actor に newLayerName_ を
		* 設定する。
		*/
		void Redo()override;

		/**
		* [EN]
		* Restores oldLayerName_ on whatever actor currently holds actorPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actorPersistentId_ を現在保持している actor に oldLayerName_ を
		* 復元する。
		*/
		void Undo()override;

	private:
		/// [EN] The World the actor lives in.
		/// [JP] actor が属する World。
		World& world_;

		/// [EN] Persistent ID of the actor whose layer changed.
		/// [JP] レイヤーが変わった actor の永続 ID。
		Uint32 actorPersistentId_;

		/// [EN] Layer name before the change; restored on Undo.
		/// [JP] 変更前のレイヤー名。Undo で復元される。
		String oldLayerName_;

		/// [EN] Layer name after the change; reapplied on Redo.
		/// [JP] 変更後のレイヤー名。Redo で再適用される。
		String newLayerName_;
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
		ActorActiveCommand(World& world, Actor actor, Bool newActive);

		/**
		* [EN]
		* Re-runs the cascading Actor::SetActive(newActive_) on whatever
		* actor currently holds rootPersistentId_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rootPersistentId_ を現在保持している actor へ、連鎖する
		* Actor::SetActive(newActive_) を再実行する。
		*/
		void Redo()override;

		/**
		* [EN]
		* Restores every captured entry's individual prior active state by
		* writing straight into its Active component - not via another
		* cascading SetActive, so a descendant that differed from its parent
		* comes back unchanged.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みの各エントリの個別の元のアクティブ状態を、その Active
		* コンポーネントへ直接書き込んで復元する - 連鎖する SetActive を
		* 介さないので、親と異なっていた子孫がそのまま元へ戻る。
		*/
		void Undo()override;

	private:
		/**
		* [EN]
		* One captured actor's pre-edit active state, keyed by persistent ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得された1 actor の編集前のアクティブ状態。永続 ID をキーとする。
		*/
		struct Entry
		{
			/// [EN] Persistent ID of the captured actor.
			/// [JP] 取得された actor の永続 ID。
			Uint32 persistentId_ = 0;

			/// [EN] Its IsActive() value before the toggle; restored on Undo.
			/// [JP] 切り替え前の IsActive() の値。Undo で復元される。
			Bool oldActive_ = true;
		};

		/// [EN] The World the actors live in.
		/// [JP] actor 群が属する World。
		World& world_;

		/// [EN] Persistent ID of the actor the toggle was applied to (the cascade root).
		/// [JP] 切り替えが適用された actor の永続 ID(連鎖のルート)。
		Uint32 rootPersistentId_;

		/// [EN] The active state Redo sets on the root (and thus the whole subtree).
		/// [JP] Redo がルート(ひいてはサブツリー全体)に設定するアクティブ状態。
		Bool newActive_;

		/// [EN] Pre-edit state of the root and every descendant, captured in the constructor.
		/// [JP] ルートと全子孫の編集前の状態。コンストラクタで取得される。
		DynamicArray<Entry> entries_;
	};
}
