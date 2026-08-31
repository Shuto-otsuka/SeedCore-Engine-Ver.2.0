#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs an entity wrapping the given handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された handle を包むエンティティを構築する。
	*/
	Entity::Entity(Handle<Entity> handle) : handle_(handle)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns whether this and other refer to the same handle (including
	* generation).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* this と other が（世代も含めて）同じハンドルを指しているかどうかを
	* 返す。
	*/
	Bool Entity::operator==(const Entity& other)const
	{
		return handle_ == other.handle_;
	}

	/**
	* [EN]
	* Negation of operator==.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* operator== の否定。
	*/
	Bool Entity::operator!=(const Entity& other)const
	{
		return handle_ != other.handle_;
	}

	/**
	* [EN]
	* Returns this entity's underlying handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このエンティティの内部ハンドルを返す。
	*/
	const Handle<Entity>& Entity::GetHandle()const
	{
		return handle_;
	}

	/**
	* [EN]
	* Returns this entity's EntityID (slot index + generation), or a
	* default (invalid) EntityID if this entity's handle is null.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このエンティティの EntityID（スロットインデックス + 世代）を返す。
	* このエンティティのハンドルが null ならデフォルト（無効）の EntityID
	* を返す。
	*/
	EntityID Entity::GetID()const
	{
		if (!handle_.exists())
		{
			return EntityID{};
		}

		return EntityID{ static_cast<Uint32>(handle_.index_), static_cast<Uint32>(handle_.generation_) };
	}

	/**
	* [EN]
	* Returns whether this entity's handle still refers to a live entity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このエンティティのハンドルが、まだ有効なエンティティを指している
	* かどうかを返す。
	*/
	Bool Entity::Exists()const
	{
		return handle_.exists();
	}

	/**
	* [EN]
	* Returns a null (non-existent) entity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* null（存在しない）エンティティを返す。
	*/
	Entity Entity::Null()
	{
		return Entity(Handle<Entity>::null());
	}
}
