#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Identifies a single entity within a World. index_ is the entity's
	* slot - the value that indexes slot-based storage (chunk rows,
	* sparse arrays) - and generation_ is that slot's generation at the
	* time this id was formed. A lookup with a stale id (its slot has
	* since been recycled) mismatches on generation_ and resolves to
	* nothing, rather than silently aliasing whichever entity now
	* occupies the slot. A default-constructed EntityID is invalid.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の単一エンティティを識別する。index_ はエンティティの
	* スロット - スロットベースのストレージ（チャンク行、スパース配列）の
	* 添字になる値 - で、generation_ はこの id が生成された時点のその
	* スロットの世代。古い id（そのスロットはその後再利用された）での
	* 検索は generation_ が一致せず、いま スロットを占有しているエンティティ
	* へ黙ってエイリアスするのではなく、何も見つからない結果に解決される。
	* デフォルト構築された EntityID は無効。
	*/
	struct EntityID
	{
		/// [EN] Slot index. 0xFFFFFFFF marks an invalid (default-constructed) id.
		/// [JP] スロットインデックス。0xFFFFFFFF は無効な（デフォルト構築された）id を表す。
		Uint32 index_ = 0xFFFFFFFFu;

		/// [EN] Generation of the slot at the time this id was formed.
		/// [JP] この id が生成された時点のスロットの世代。
		Uint32 generation_ = 0;

		/// [EN] Two ids are equal iff both slot and generation match.
		/// [JP] スロットと世代の両方が一致する場合にのみ等しい。
		friend Bool operator==(const EntityID&, const EntityID&) = default;
	};

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
		* Returns this entity's EntityID (slot index + generation), or a
		* default (invalid) EntityID if this entity's handle is null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このエンティティの EntityID（スロットインデックス + 世代）を返す。
		* このエンティティのハンドルが null ならデフォルト（無効）の
		* EntityID を返す。
		*/
		EntityID GetID()const;

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

/**
* [EN]
* std::hash specialization for EntityID, so it can key a FlatMap /
* unordered_map. Mixes the packed {generation, index} through a
* splitmix64 finalizer so keys distribute well across a power-of-two
* bucket count.
*
* ---------------------------------------------------------------------
*
* [JP]
* EntityID の std::hash 特殊化。FlatMap / unordered_map のキーに
* できるようにする。パックした {generation, index} を splitmix64 の
* 最終化で混ぜ、2の冪のバケット数に対してキーがよく分散するようにする。
*/
template<>
struct std::hash<SeedCore::EntityID>
{
	SeedCore::Size operator()(const SeedCore::EntityID& id)const noexcept
	{
		SeedCore::Uint64 h = (static_cast<SeedCore::Uint64>(id.generation_) << 32) | static_cast<SeedCore::Uint64>(id.index_);
		h ^= h >> 33;
		h *= 0xFF51AFD7ED558CCDull;
		h ^= h >> 33;
		return static_cast<SeedCore::Size>(h);
	}
};
