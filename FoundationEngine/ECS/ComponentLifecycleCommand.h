#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Resource/ActorSerialization.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Undo/redo command for attaching a component to an Actor (Inspector's
	* "コンポーネントを追加"). Redo re-attaches componentID_ (freshly
	* default-constructed, same as the original AddComponent call); Undo
	* removes it again. The target actor is re-resolved by
	* actorPersistentId_ each time rather than cached.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actorへのコンポーネント追加(Inspectorの「コンポーネントを追加」)に
	* 対するUndo/Redoコマンド。Redoはcomponentid_を(元のAddComponent呼び
	* 出しと同じく、新規デフォルト構築で)再アタッチする。Undoは再び削除
	* する。対象actorはキャッシュせず、毎回actorPersistentId_で再解決する。
	*/
	class SEEDCORE_API ComponentAddCommand : public Command
	{
	public:
		ComponentAddCommand(World& world, Uint32 actorPersistentId, ComponentID componentID);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		Uint32 actorPersistentId_;
		ComponentID componentID_;
	};

	/**
	* [EN]
	* Undo/redo command for removing a component from an Actor
	* (Inspector's "コンポーネントを削除"). Captures the component's
	* field values (via CaptureComponent) before it's removed, so Undo
	* can re-attach it and restore them (via ApplyComponent) rather than
	* leaving it default-constructed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actorからのコンポーネント削除(Inspectorの「コンポーネントを削除」)
	* に対するUndo/Redoコマンド。削除される前にコンポーネントのフィールド
	* 値を(CaptureComponent経由で)取得しておき、Undo時にはデフォルト
	* 構築のままにせず、再アタッチしてその値を(ApplyComponent経由で)
	* 復元する。
	*/
	class SEEDCORE_API ComponentRemoveCommand : public Command
	{
	public:
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
		ComponentRemoveCommand(World& world, Uint32 actorPersistentId, ComponentID componentID, const String& componentName, void* componentData);

		void Redo()override;

		void Undo()override;

	private:
		World& world_;
		Uint32 actorPersistentId_;
		ComponentID componentID_;
		SerializedComponent captured_;
	};
}
