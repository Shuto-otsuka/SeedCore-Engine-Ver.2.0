#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns the Actor this component is attached to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このコンポーネントがアタッチされている Actor を返す。
	*/
	Actor& ComponentBase::GetActor()const
	{
		return *actor_;
	}

	/**
	* [EN]
	* Returns the World that owns this component's Actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このコンポーネントの Actor を所有する World を返す。
	*/
	World& ComponentBase::GetWorld()const
	{
		return actor_->GetWorld();
	}

	/**
	* [EN]
	* Returns this component's display/type name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このコンポーネントの表示名/型名を返す。
	*/
	const String& ComponentBase::GetComponentName()const
	{
		return componentName_;
	}

	/**
	* [EN]
	* Resets the awoken_/started_ flags, allowing Awake/Start to fire
	* again as if the component were freshly attached.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* awoken_/started_ フラグをリセットし、コンポーネントが新たに
	* アタッチされたかのように Awake/Start を再度発火できるようにする。
	*/
	void ComponentBase::ResetLifecycleState()
	{
		awoken_ = false;
		started_ = false;
	}

	/**
	* [EN]
	* Dispatches the OnDestroy lifecycle event to this component's
	* implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* OnDestroy ライフサイクルイベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchDestroy()
	{
		if (destroy_)
		{
			destroy_(this);
		}
	}

	/**
	* [EN]
	* Dispatches the inspector-GUI draw call to this component's
	* implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インスペクタ GUI の描画呼び出しを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchInspectorGUI()
	{
		if (inspectorGUI_)
		{
			inspectorGUI_(this);
		}
	}

	/**
	* [EN]
	* Dispatches a collision-enter event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other との衝突開始イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchCollisionEnter(Entity other)
	{
		if (collisionEnter_)
		{
			collisionEnter_(this, other);
		}
	}

	/**
	* [EN]
	* Dispatches a collision-stay event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other との衝突継続イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchCollisionStay(Entity other)
	{
		if (collisionStay_)
		{
			collisionStay_(this, other);
		}
	}

	/**
	* [EN]
	* Dispatches a collision-exit event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other との衝突終了イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchCollisionExit(Entity other)
	{
		if (collisionExit_)
		{
			collisionExit_(this, other);
		}
	}

	/**
	* [EN]
	* Dispatches a trigger-enter event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのトリガー開始イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchTriggerEnter(Entity other)
	{
		if (triggerEnter_)
		{
			triggerEnter_(this, other);
		}
	}

	/**
	* [EN]
	* Dispatches a trigger-stay event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのトリガー継続イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchTriggerStay(Entity other)
	{
		if (triggerStay_)
		{
			triggerStay_(this, other);
		}
	}

	/**
	* [EN]
	* Dispatches a trigger-exit event involving other to this
	* component's implementation, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのトリガー終了イベントを、このコンポーネントの実装
	* （存在する場合）へディスパッチする。
	*/
	void ComponentBase::DispatchTriggerExit(Entity other)
	{
		if (triggerExit_)
		{
			triggerExit_(this, other);
		}
	}
}
