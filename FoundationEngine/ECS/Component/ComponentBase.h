#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Component/Awakeable.h>
#include <FoundationEngine/ECS/Component/Startable.h>
#include <FoundationEngine/ECS/Component/Tickable.h>
#include <FoundationEngine/ECS/Component/FixedTick.h>
#include <FoundationEngine/ECS/Component/LateTickable.h>
#include <FoundationEngine/ECS/Component/Destroyable.h>
#include <FoundationEngine/ECS/Component/InspectorDrawable.h>
#include <FoundationEngine/ECS/Component/Collisionable.h>
#include <FoundationEngine/ECS/Component/Triggerable.h>
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	class Actor;
	class World;

	/**
	* [EN]
	* Base class for class-based components attached to an Actor.
	* Analogous to Unity's MonoBehaviour. Subclasses can optionally
	* implement lifecycle functions (Start, Update, etc.) which are
	* detected and called automatically via concepts.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor にアタッチするクラスベースのコンポーネントの基底クラス。
	* Unity の MonoBehaviour に相当する。サブクラスはライフサイクル関数
	* （Start, Update など）を任意で実装でき、コンセプトを通じて自動的に呼ばれる。
	*/
	class SEEDCORE_API ComponentBase
	{
	public:
		/// [EN] Marker tag type used by concepts/traits to detect that a type derives from ComponentBase.
		/// [JP] ある型が ComponentBase から派生していることをコンセプト/トレイトが検出するために使う、マーカータグ型。
		using is_component_base_tag = void;

		/**
		* [EN]
		* Virtual destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		virtual ~ComponentBase() = default;

		/**
		* [EN]
		* Returns the Actor this component is attached to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このコンポーネントがアタッチされている Actor を返す。
		*/
		Actor& GetActor()const;

		/**
		* [EN]
		* Returns the World that owns this component's Actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このコンポーネントの Actor を所有する World を返す。
		*/
		World& GetWorld()const;

		/**
		* [EN]
		* Returns this component's display/type name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このコンポーネントの表示名/型名を返す。
		*/
		const String& GetName()const;

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
		void ResetLifecycleState();

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
		void DispatchDestroy();

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
		void DispatchInspectorGUI();

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
		void DispatchCollisionEnter(Entity other);

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
		void DispatchCollisionStay(Entity other);

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
		void DispatchCollisionExit(Entity other);

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
		void DispatchTriggerEnter(Entity other);

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
		void DispatchTriggerStay(Entity other);

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
		void DispatchTriggerExit(Entity other);

	private:
		friend class Actor;
		friend class World;
		friend class SystemScheduler;
		friend class ComponentRegistry;

		/// [EN] The Actor this component is attached to; set when the component is registered.
		/// [JP] このコンポーネントがアタッチされている Actor。コンポーネント登録時に設定される。
		Actor* actor_ = nullptr;

		/// [EN] This component's display/type name.
		/// [JP] このコンポーネントの表示名/型名。
		String componentName_;

		/// [EN] Whether this component's Awake lifecycle function has already fired.
		/// [JP] このコンポーネントの Awake ライフサイクル関数が既に発火済みかどうか。
		Bool awoken_ = false;

		/// [EN] Whether this component's Start lifecycle function has already fired.
		/// [JP] このコンポーネントの Start ライフサイクル関数が既に発火済みかどうか。
		Bool started_ = false;

		/// [EN] Type-erased pointer to the subclass's Awake function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの Awake 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*awake_)(ComponentBase*) = nullptr;

		/// [EN] Type-erased pointer to the subclass's Start function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの Start 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*start_)(ComponentBase*) = nullptr;

		/// [EN] Type-erased pointer to the subclass's Update (tick) function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの Update（tick）関数への型消去されたポインタ。実装していなければ nullptr。
		void (*tick_)(ComponentBase*, Float) = nullptr;

		/// [EN] Type-erased pointer to the subclass's FixedUpdate function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの FixedUpdate 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*fixedTick_)(ComponentBase*, Float) = nullptr;

		/// [EN] Type-erased pointer to the subclass's LateUpdate function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの LateUpdate 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*lateTick_)(ComponentBase*, Float) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnDestroy function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnDestroy 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*destroy_)(ComponentBase*) = nullptr;

		/// [EN] Type-erased pointer to the subclass's inspector-GUI function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスのインスペクタ GUI 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*inspectorGUI_)(ComponentBase*) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnCollisionEnter function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnCollisionEnter 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*collisionEnter_)(ComponentBase*, Entity) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnCollisionStay function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnCollisionStay 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*collisionStay_)(ComponentBase*, Entity) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnCollisionExit function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnCollisionExit 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*collisionExit_)(ComponentBase*, Entity) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnTriggerEnter function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnTriggerEnter 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*triggerEnter_)(ComponentBase*, Entity) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnTriggerStay function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnTriggerStay 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*triggerStay_)(ComponentBase*, Entity) = nullptr;

		/// [EN] Type-erased pointer to the subclass's OnTriggerExit function, or nullptr if it doesn't implement one.
		/// [JP] サブクラスの OnTriggerExit 関数への型消去されたポインタ。実装していなければ nullptr。
		void (*triggerExit_)(ComponentBase*, Entity) = nullptr;
	};
}
