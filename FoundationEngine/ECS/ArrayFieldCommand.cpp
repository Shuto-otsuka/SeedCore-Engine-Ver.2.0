#include <FoundationEngine/ECS/ArrayFieldCommand.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Stores only identifiers; the live array hooks are re-resolved inside
	* each Redo/Undo (see the header).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 識別子のみを保持する。生きた配列フックは各 Redo/Undo の中で再解決
	* される(ヘッダ参照)。
	*/
	ArrayFieldCommand::ArrayFieldCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index) : world_(world), entity_(entity), componentID_(componentID), fieldName_(fieldName), index_(index)
	{
		/// No Code
	}

	/**
	* [EN]
	* Forwards to ArrayFieldCommand.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ArrayFieldCommand へ委譲する。
	*/
	ArrayAppendCommand::ArrayAppendCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index) : ArrayFieldCommand(world, entity, componentID, fieldName, index)
	{
		/// No Code
	}

	/**
	* [EN]
	* Re-resolves the target array and appends one default-constructed
	* element via its live add_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 対象の配列を再解決し、その生きた add_ でデフォルト構築済みの要素を
	* 1つ追加する。
	*/
	void ArrayAppendCommand::Redo()
	{
		/// [EN] The component may have been detached or the actor destroyed since the edit; nothing to do then.
		/// [JP] 編集後にコンポーネントが外された、または actor が破棄された可能性がある。その場合は何もしない。
		void* component = world_.GetComponent(entity_, componentID_);
		if (!component)
		{
			return;
		}

		/// [EN] Rebuild the FieldInfo list against the current instance so array_.add_ captures a live pointer, not the stale one from the edit's frame.
		/// [JP] 現在のインスタンスに対して FieldInfo 一覧を作り直し、array_.add_ が編集時のフレームの古いポインタではなく生きたポインタをキャプチャするようにする。
		auto& registry = ReflectionRegistry::GetRegistry();
		auto reflector = registry.find(ComponentRegistry::GetName(componentID_));
		if (reflector == registry.end())
		{
			return;
		}

		DynamicArray<FieldInfo> fields;
		reflector->second(component, fields);

		for (const FieldInfo& field : fields)
		{
			if (field.name_ == fieldName_ && field.array_.add_)
			{
				field.array_.add_();
				return;
			}
		}
	}

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
	void ArrayAppendCommand::Undo()
	{
		void* component = world_.GetComponent(entity_, componentID_);
		if (!component)
		{
			return;
		}

		auto& registry = ReflectionRegistry::GetRegistry();
		auto reflector = registry.find(ComponentRegistry::GetName(componentID_));
		if (reflector == registry.end())
		{
			return;
		}

		DynamicArray<FieldInfo> fields;
		reflector->second(component, fields);

		for (const FieldInfo& field : fields)
		{
			if (field.name_ == fieldName_ && field.array_.remove_)
			{
				field.array_.remove_(index_);
				return;
			}
		}
	}

	/**
	* [EN]
	* Forwards the identifiers to ArrayFieldCommand and keeps the payload
	* value and direction.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 識別子を ArrayFieldCommand へ委譲し、Payload 値と方向を保持する。
	*/
	PayloadArrayCommand::PayloadArrayCommand(World& world, Entity entity, ComponentID componentID, String fieldName, Size index, Int value, Bool addOnRedo) : ArrayFieldCommand(world, entity, componentID, fieldName, index), value_(value), addOnRedo_(addOnRedo)
	{
		/// No Code
	}

	/**
	* [EN]
	* Appends value_ (or removes index_, when addOnRedo_ is false),
	* re-resolving the array against the current component instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のコンポーネントインスタンスに対して配列を再解決し、value_ を
	* 追加する(addOnRedo_ が false のときは index_ を削除)。
	*/
	void PayloadArrayCommand::Redo()
	{
		void* component = world_.GetComponent(entity_, componentID_);
		if (!component)
		{
			return;
		}

		auto& registry = ReflectionRegistry::GetRegistry();
		auto reflector = registry.find(ComponentRegistry::GetName(componentID_));
		if (reflector == registry.end())
		{
			return;
		}

		DynamicArray<FieldInfo> fields;
		reflector->second(component, fields);

		for (const FieldInfo& field : fields)
		{
			if (field.name_ != fieldName_ || !field.array_.add_)
			{
				continue;
			}

			if (addOnRedo_)
			{
				/// [EN] Append, then write the payload value into the freshly-grown last element via lastPtr_.
				/// [JP] 追加してから、lastPtr_ 経由で追加直後の末尾要素へ Payload 値を書き込む。
				field.array_.add_();
				if (field.array_.lastPtr_)
				{
					void* element = field.array_.lastPtr_();
					if (element)
					{
						*static_cast<Int*>(element) = value_;
					}
				}
			}
			else if (field.array_.remove_)
			{
				field.array_.remove_(index_);
			}
			return;
		}
	}

	/**
	* [EN]
	* Reverses Redo: removes index_ (or re-appends value_ and re-writes it).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Redo の逆: index_ を削除する(または value_ を再追加して書き戻す)。
	*/
	void PayloadArrayCommand::Undo()
	{
		void* component = world_.GetComponent(entity_, componentID_);
		if (!component)
		{
			return;
		}

		auto& registry = ReflectionRegistry::GetRegistry();
		auto reflector = registry.find(ComponentRegistry::GetName(componentID_));
		if (reflector == registry.end())
		{
			return;
		}

		DynamicArray<FieldInfo> fields;
		reflector->second(component, fields);

		for (const FieldInfo& field : fields)
		{
			if (field.name_ != fieldName_ || !field.array_.add_)
			{
				continue;
			}

			if (addOnRedo_)
			{
				if (field.array_.remove_)
				{
					field.array_.remove_(index_);
				}
			}
			else
			{
				field.array_.add_();
				if (field.array_.lastPtr_)
				{
					void* element = field.array_.lastPtr_();
					if (element)
					{
						*static_cast<Int*>(element) = value_;
					}
				}
			}
			return;
		}
	}
}
