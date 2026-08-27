#include <FoundationEngine/ECS/ActorCommand.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* world/cache are used to (re)instantiate/destroy the actors; nodes is
	* the captured subtree (see CaptureActorNode), already carrying the
	* persistent IDs it was created with; parentPersistentId is 0 for a
	* root-level subtree.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world/cacheはactor群の(再)インスタンス化/破棄に使う。nodesは取得済み
	* のサブツリー(CaptureActorNode参照)であり、作成時に割り当てられた
	* 永続IDを既に持っている。parentPersistentIdはルートレベルの
	* サブツリーであれば0。
	*/
	ActorCreateCommand::ActorCreateCommand(World& world, ResourceCache& cache, DynamicArray<SerializedActorNode> nodes, Uint32 parentPersistentId)
		: world_(world), cache_(cache), nodes_(std::move(nodes)), parentPersistentId_(parentPersistentId)
	{
		/// No Code
	}

	/**
	* [EN]
	* Re-instantiates nodes_, parenting the root under whatever actor
	* currently holds parentPersistentId_ (or as a root if 0/gone).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* nodes_を再インスタンス化し、ルートをparentPersistentId_を現在保持
	* しているactorの下へ親付けする(0/存在しなければルートとして)。
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
	}

	/**
	* [EN]
	* Destroys every actor currently holding one of nodes_'s persistent
	* IDs, from the last-captured (deepest) node back to the root.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* nodes_のいずれかの永続IDを現在保持している全actorを、最後に取得
	* された(最も深い)ノードからルートへ向かって順に破棄する。
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
	ActorDeleteCommand::ActorDeleteCommand(World& world, ResourceCache& cache, Actor* actor)
		: world_(world), cache_(cache)
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

	ActorReparentCommand::ActorReparentCommand(World& world, Uint32 actorPersistentId, Uint32 oldParentPersistentId, Uint32 newParentPersistentId)
		: world_(world), actorPersistentId_(actorPersistentId), oldParentPersistentId_(oldParentPersistentId), newParentPersistentId_(newParentPersistentId)
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
	* actorPersistentId_のactorを、newParentPersistentId_を現在保持している
	* actorの下へ再親付けする(0/存在しなければルートとして)。
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
	* Re-parents actorPersistentId_'s actor back under whatever actor
	* currently holds oldParentPersistentId_ (or as a root if 0/gone).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_のactorを、oldParentPersistentId_を現在保持している
	* actorの下へ再び親付けし直す(0/存在しなければルートとして)。
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
	}

	ActorTagCommand::ActorTagCommand(World& world, Uint32 actorPersistentId, const String& tag, Bool addOnRedo)
		: world_(world), actorPersistentId_(actorPersistentId), tag_(tag), addOnRedo_(addOnRedo)
	{
		/// No Code
	}

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

	ActorLayerCommand::ActorLayerCommand(World& world, Uint32 actorPersistentId, const String& oldLayerName, const String& newLayerName)
		: world_(world), actorPersistentId_(actorPersistentId), oldLayerName_(oldLayerName), newLayerName_(newLayerName)
	{
		/// No Code
	}

	void ActorLayerCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		actor->SetLayer(newLayerName_);
	}

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
	ActorActiveCommand::ActorActiveCommand(World& world, Actor* actor, Bool newActive)
		: world_(world), rootPersistentId_(actor->GetPersistentID()), newActive_(newActive)
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
