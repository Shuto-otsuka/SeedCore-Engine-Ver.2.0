#include <FoundationEngine/ECS/ComponentLifecycleCommand.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ComponentAddCommand::ComponentAddCommand(World& world, Uint32 actorPersistentId, ComponentID componentID)
		: world_(world), actorPersistentId_(actorPersistentId), componentID_(componentID)
	{
		/// No Code
	}

	/**
	* [EN]
	* Re-attaches componentID_ to whatever actor currently holds
	* actorPersistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_を現在保持しているactorへcomponentid_を再アタッチ
	* する。
	*/
	void ComponentAddCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (actor)
		{
			actor->AddComponent(componentID_);
		}
	}

	/**
	* [EN]
	* Removes componentID_ from whatever actor currently holds
	* actorPersistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_を現在保持しているactorからcomponentid_を削除する。
	*/
	void ComponentAddCommand::Undo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (actor)
		{
			actor->RemoveComponent(componentID_);
		}
	}

	/**
	* [EN]
	* Captures componentData's current field values - call before
	* Actor::RemoveComponent(componentID) actually runs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* componentDataの現在のフィールド値を取得する -
	* Actor::RemoveComponent(componentID)が実際に実行される前に呼ぶこと。
	*/
	ComponentRemoveCommand::ComponentRemoveCommand(World& world, Uint32 actorPersistentId, ComponentID componentID, const String& componentName, void* componentData)
		: world_(world), actorPersistentId_(actorPersistentId), componentID_(componentID), captured_(CaptureComponent(componentName, componentData))
	{
		/// No Code
	}

	/**
	* [EN]
	* Removes componentID_ from whatever actor currently holds
	* actorPersistentId_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_を現在保持しているactorからcomponentid_を削除する。
	*/
	void ComponentRemoveCommand::Redo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (actor)
		{
			actor->RemoveComponent(componentID_);
		}
	}

	/**
	* [EN]
	* Re-attaches componentID_ to whatever actor currently holds
	* actorPersistentId_, then restores its captured field values.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actorPersistentId_を現在保持しているactorへcomponentid_を再アタッチ
	* し、取得済みのフィールド値を復元する。
	*/
	void ComponentRemoveCommand::Undo()
	{
		Actor* actor = world_.FindActor(actorPersistentId_);
		if (!actor)
		{
			return;
		}

		actor->AddComponent(componentID_);

		void* componentData = world_.GetComponent(actor->GetEntity(), componentID_);
		if (componentData)
		{
			ApplyComponent(captured_, componentData);
		}
	}
}
