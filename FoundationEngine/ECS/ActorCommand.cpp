#include <FoundationEngine/ECS/ActorCommand.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* world/cache (re)instantiate and destroy the actors. nodes is the
	* captured subtree (see CaptureActorNode), already carrying the
	* persistent IDs it was created with. parentPersistentId is 0 for a
	* root-level subtree. prevSiblingPersistentId is the sibling the new
	* root should sit directly after under that parent (0 = append / no
	* ordering to restore).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world/cache が actor 群の(再)インスタンス化と破棄を行う。nodes は
	* 取得済みのサブツリー(CaptureActorNode 参照)で、作成時に割り当て
	* られた永続 ID を既に持っている。parentPersistentId はルートレベルの
	* サブツリーであれば 0。prevSiblingPersistentId は、その親の下で新しい
	* ルートを直後に置くべき兄弟(0 = 末尾へ追加 / 復元すべき並び順なし)。
	*/
	ActorCreateCommand::ActorCreateCommand(World& world, ResourceCache& cache, DynamicArray<SerializedActorNode> nodes, Uint32 parentPersistentId, Uint32 prevSiblingPersistentId) : world_(world), cache_(cache), nodes_(std::move(nodes)), parentPersistentId_(parentPersistentId), prevSiblingPersistentId_(prevSiblingPersistentId)
	{
		/// No Code
	}

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
	void ActorCreateCommand::Redo()
	{
		Actor* rootParent = parentPersistentId_ ? world_.FindActor(parentPersistentId_) : nullptr;

		DynamicArray<Actor*> instantiated;
		instantiated.reserve(nodes_.size());

		for (Size index = 0; index < nodes_.size(); ++index)
		{
			const SerializedActorNode& node = nodes_[index];

			Actor* parent = nullptr;
			if (index == 0)
			{
				parent = rootParent;
			}
			else if (node.parentIndex_ >= 0 && static_cast<Size>(node.parentIndex_) < instantiated.size())
			{
				parent = instantiated[node.parentIndex_];
			}

			Actor* actor = InstantiateActorNode(world_, cache_, node, parent, false);
			instantiated.push_back(actor);
		}

		/// [EN] The root was just appended to rootParent's children; move it back next to the sibling it originally followed (null -> front). Only meaningful under a parent - root order is not tracked.
		/// [JP] ルートは今 rootParent の子リスト末尾へ追加されたところ。元々続いていた兄弟の隣へ(null なら先頭へ)戻す。親がある場合のみ意味を持つ - ルートの並び順は追跡していない。
		if (rootParent && !instantiated.empty() && instantiated[0])
		{
			rootParent->MoveChildAfter(instantiated[0], world_.FindActor(prevSiblingPersistentId_));
		}
	}

	/**
	* [EN]
	* Destroys every actor currently holding one of nodes_'s persistent
	* IDs, from the deepest captured node back to the root.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* nodes_ のいずれかの永続 ID を現在保持している全 actor を、最も深い
	* 取得ノードからルートへ向かって順に破棄する。
	*/
	void ActorCreateCommand::Undo()
	{
		for (Size index = nodes_.size(); index > 0; --index)
		{
			Actor* actor = world_.FindActor(nodes_[index - 1].persistentId_);
			if (actor)
			{
				world_.DestroyActor(actor);
			}
		}
	}

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
	ActorDeleteCommand::ActorDeleteCommand(World& world, ResourceCache& cache, Actor* actor) : world_(world), cache_(cache)
	{
		DynamicArray<SerializedActorNode> nodes;
		CaptureActorNode(actor, -1, nodes);
		node_ = nodes[0];

		persistentId_ = actor->GetPersistentID();

		Actor* parent = actor->GetParent();
		parentPersistentId_ = parent ? parent->GetPersistentID() : 0;

		std::ranges::transform(actor->GetChildren(), std::back_inserter(childPersistentIds_), [](Actor* child) { return child->GetPersistentID(); });
	}

	/**
	* [EN]
	* Destroys whatever actor currently holds persistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* persistentId_を現在保持しているactorを破棄する。
	*/
	void ActorDeleteCommand::Redo()
	{
		Actor* actor = world_.FindActor(persistentId_);
		if (actor)
		{
			world_.DestroyActor(actor);
		}
	}

	/**
	* [EN]
	* Re-instantiates node_ under whatever actor currently holds
	* parentPersistentId_ (or as a root if there was none/it's gone), then
	* re-parents every still-alive child recorded in childPersistentIds_
	* back onto it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* parentPersistentId_を現在保持しているactorの下へnode_を再
	* インスタンス化する(存在しなければルートとして)。その後、
	* childPersistentIds_に記録された、まだ生きている子actorを全て
	* この下へ再び親付けし直す。
	*/
	void ActorDeleteCommand::Undo()
	{
		Actor* parent = parentPersistentId_ ? world_.FindActor(parentPersistentId_) : nullptr;

		Actor* actor = InstantiateActorNode(world_, cache_, node_, parent, false);
		if (!actor)
		{
			return;
		}

		for (Uint32 childPersistentId : childPersistentIds_)
		{
			Actor* child = world_.FindActor(childPersistentId);
			if (child)
			{
				child->SetParent(actor);
			}
		}
	}

	/**
	* [EN]
	* oldPrevSiblingPersistentId is captured (before the reparent runs) so
	* Undo can put the actor back in its exact old slot; see the header.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* oldPrevSiblingPersistentId は(再親付けが走る前に)取得され、Undo が
	* actor を元の正確な位置へ戻せるようにする。ヘッダを参照。
	*/
	ActorReparentCommand::ActorReparentCommand(World& world, Uint32 actorPersistentId, Uint32 oldParentPersistentId, Uint32 oldPrevSiblingPersistentId, Uint32 newParentPersistentId) : world_(world), actorPersistentId_(actorPersistentId), oldParentPersistentId_(oldParentPersistentId), oldPrevSiblingPersistentId_(oldPrevSiblingPersistentId), newParentPersistentId_(newParentPersistentId)
	{
		/// No Code
	}

	/**
	* [EN]
	* Re-parents actorPersistentId_'s actor under whatever actor currently
	* holds newParentPersistentId_ (or as a root if 0/gone).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_ の actor を、newParentPersistentId_ を現在保持
	* している actor の下へ再親付けする(0/存在しなければルートとして)。
	*/
	void ActorReparentCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		Actor* newParent = newParentPersistentId_ ? world_.FindActor(newParentPersistentId_) : nullptr;
		actor->SetParent(newParent);
	}

	/**
	* [EN]
	* Re-parents actorPersistentId_'s actor back under oldParentPersistentId_'s
	* actor (or as a root if 0/gone), then restores its original sibling
	* slot via oldPrevSiblingPersistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_ の actor を oldParentPersistentId_ の actor の下へ
	* (0/存在しなければルートとして)再び親付けし直し、
	* oldPrevSiblingPersistentId_ で元の兄弟内の位置を復元する。
	*/
	void ActorReparentCommand::Undo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		Actor* oldParent = oldParentPersistentId_ ? world_.FindActor(oldParentPersistentId_) : nullptr;
		actor->SetParent(oldParent);

		/// [EN] SetParent appends to the end of the new sibling list; restore the original slot by moving actor back after whichever sibling preceded it (null -> front). Root order is not tracked, so this only applies under an old parent.
		/// [JP] SetParent は新しい兄弟リストの末尾へ追加する。元の位置を復元するため、直前にあった兄弟の後ろへ(null なら先頭へ)戻す。ルートの並び順は追跡していないので、旧親がある場合のみ行う。
		if (oldParent)
		{
			oldParent->MoveChildAfter(actor, world_.FindActor(oldPrevSiblingPersistentId_));
		}
	}

	/**
	* [EN]
	* addOnRedo fixes the direction (see the header); nothing is applied
	* here - the caller has already made the edit.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* addOnRedo が方向を固定する(ヘッダ参照)。ここでは何も適用しない -
	* 呼び出し側が既に編集を行っている。
	*/
	ActorTagCommand::ActorTagCommand(World& world, Uint32 actorPersistentId, const String& tag, Bool addOnRedo) : world_(world), actorPersistentId_(actorPersistentId), tag_(tag), addOnRedo_(addOnRedo)
	{
		/// No Code
	}

	/**
	* [EN]
	* Adds tag_ on actorPersistentId_'s actor (removes it when addOnRedo_
	* is false).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_ の actor に tag_ を追加する(addOnRedo_ が false の
	* ときは削除)。
	*/
	void ActorTagCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		if (addOnRedo_)
		{
			actor->AddTag(tag_);
		}
		else
		{
			actor->RemoveTag(tag_);
		}
	}

	/**
	* [EN]
	* Reverses Redo: removes tag_ (adds it when addOnRedo_ is false).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Redo の逆: tag_ を削除する(addOnRedo_ が false のときは追加)。
	*/
	void ActorTagCommand::Undo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		if (addOnRedo_)
		{
			actor->RemoveTag(tag_);
		}
		else
		{
			actor->AddTag(tag_);
		}
	}

	/**
	* [EN]
	* Layer names are stored as given; SetLayer resolves them (and falls
	* back to the default layer for an unknown name) at Redo/Undo time.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイヤー名は渡されたまま保持する。SetLayer が Redo/Undo 時にそれらを
	* 解決する(未知の名前はデフォルトレイヤーへフォールバックする)。
	*/
	ActorLayerCommand::ActorLayerCommand(World& world, Uint32 actorPersistentId, const String& oldLayerName, const String& newLayerName) : world_(world), actorPersistentId_(actorPersistentId), oldLayerName_(oldLayerName), newLayerName_(newLayerName)
	{
		/// No Code
	}

	/**
	* [EN]
	* Sets newLayerName_ on actorPersistentId_'s actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_ の actor に newLayerName_ を設定する。
	*/
	void ActorLayerCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		actor->SetLayer(newLayerName_);
	}

	/**
	* [EN]
	* Restores oldLayerName_ on actorPersistentId_'s actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_ の actor に oldLayerName_ を復元する。
	*/
	void ActorLayerCommand::Undo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		actor->SetLayer(oldLayerName_);
	}

	/**
	* [EN]
	* Recursively captures the current active state of actor and every
	* descendant - call before Actor::SetActive(newActive) actually runs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorと全子孫の現在のアクティブ状態を再帰的に取得する -
	* Actor::SetActive(newActive)が実際に実行される前に呼ぶこと。
	*/
	ActorActiveCommand::ActorActiveCommand(World& world, Actor* actor, Bool newActive) : world_(world), rootPersistentId_(actor->GetPersistentID()), newActive_(newActive)
	{
		DynamicArray<Actor*> pending;
		pending.push_back(actor);

		while (!pending.empty())
		{
			Actor* current = pending.back();
			pending.pop_back();

			Entry entry;
			entry.persistentId_ = current->GetPersistentID();
			entry.oldActive_ = current->IsActive();
			entries_.push_back(entry);

			std::ranges::copy(current->GetChildren(), std::back_inserter(pending));
		}
	}

	/**
	* [EN]
	* Re-applies the cascading toggle: SetActive(newActive_) on whatever
	* actor currently holds rootPersistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 連鎖する切り替えを再適用する: rootPersistentId_を現在保持している
	* actorへSetActive(newActive_)を呼ぶ。
	*/
	void ActorActiveCommand::Redo()
	{
		Actor* actor = world_.FindActor(rootPersistentId_);
		if (actor)
		{
			actor->SetActive(newActive_);
		}
	}

	/**
	* [EN]
	* Restores every captured entry's individual prior active state
	* directly (writing straight into each entity's Active component,
	* not via another cascading SetActive call).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みの各エントリの個別の元のアクティブ状態を直接復元する
	* (連鎖するSetActive呼び出しではなく、各entityのActiveコンポーネントへ
	* 直接書き込む)。
	*/
	void ActorActiveCommand::Undo()
	{
		static const String activeString("Active");
		ComponentID activeID = ComponentRegistry::GetComponentID(activeString);

		for (const Entry& entry : entries_)
		{
			Actor* actor = world_.FindActor(entry.persistentId_);
			if (!actor)
			{
				continue;
			}

			Active* component = static_cast<Active*>(world_.GetComponent(actor->GetEntity(), activeID));
			if (component)
			{
				component->active_ = entry.oldActive_;
			}
		}
	}
}
