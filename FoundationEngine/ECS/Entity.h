#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Lightweight handle identifying a single entity within a World.
	* Carries no data itself beyond a generation-checked Handle, so it
	* can be freely copied/compared; the actual component data lives in
	* the World's archetype/sparse-set storage.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の単一エンティティを識別する、軽量なハンドル。世代チェック
	* 付きの Handle 以外のデータは持たないため、自由にコピー・比較できる。
	* 実際のコンポーネントデータは World のアーキタイプ/スパースセット
	* ストレージ内に存在する。
	*/
	class SEEDCORE_API Entity
	{
	public:
		/**
		* [EN]
		* Default constructor: constructs a null (non-existent) entity handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: null（存在しない）エンティティハンドルを
		* 構築する。
		*/
		Entity() = default;

		/**
		* [EN]
		* Constructs an entity wrapping the given handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された handle を包むエンティティを構築する。
		*/
		explicit Entity(Handle<Entity> handle);

		/**
		* [EN]
		* Returns whether this and other refer to the same handle
		* (including generation).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* this と other が（世代も含めて）同じハンドルを指しているかどうかを
		* 返す。
		*/
		Bool operator==(const Entity& other)const;

		/**
		* [EN]
		* Negation of operator==.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator== の否定。
		*/
		Bool operator!=(const Entity& other)const;

		/**
		* [EN]
		* Returns this entity's underlying handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このエンティティの内部ハンドルを返す。
		*/
		const Handle<Entity>& GetHandle()const;

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
		Bool Exists()const;

		/**
		* [EN]
		* Returns a null (non-existent) entity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* null（存在しない）エンティティを返す。
		*/
		static Entity Null();

	private:
		/// [EN] The underlying generation-checked handle identifying this entity.
		/// [JP] このエンティティを識別する、世代チェック付きの内部ハンドル。
		Handle<Entity> handle_;
	};
}
