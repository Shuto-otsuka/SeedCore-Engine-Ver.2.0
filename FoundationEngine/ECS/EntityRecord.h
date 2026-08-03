#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class Archetype;
	class Chunk;

	/**
	* [EN]
	* Locates a single entity's component data within archetype-chunked
	* storage: which Archetype it belongs to, which Chunk within that
	* archetype, and which row within that chunk. Looked up by EntityID
	* from World's internal entity table.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アーキタイプ・チャンク構造化ストレージ内における、単一エンティティの
	* コンポーネントデータの位置を特定する: 所属する Archetype、その
	* アーキタイプ内の Chunk、そのチャンク内の行。World の内部エンティティ
	* テーブルから EntityID で検索される。
	*/
	struct EntityRecord
	{
		/// [EN] The archetype this entity's components are stored in; nullptr if the entity doesn't exist.
		/// [JP] このエンティティのコンポーネントが格納されているアーキタイプ。エンティティが存在しなければ nullptr。
		Archetype* archetype_ = nullptr;

		/// [EN] The chunk (within archetype_) this entity's components are stored in; nullptr if the entity doesn't exist.
		/// [JP] このエンティティのコンポーネントが格納されている、archetype_ 内のチャンク。エンティティが存在しなければ nullptr。
		Chunk* chunk_ = nullptr;

		/// [EN] The row (within chunk_) this entity's components occupy; UINT32_MAX if the entity doesn't exist.
		/// [JP] このエンティティのコンポーネントが占める、chunk_ 内の行。エンティティが存在しなければ UINT32_MAX。
		Uint32 row_ = UINT32_MAX;

		/**
		* [EN]
		* Returns whether this record points at a live entity (all
		* fields populated).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このレコードが有効なエンティティを指しているか（全フィールドが
		* 設定済みか）を返す。
		*/
		Bool Exists()const;
	};
}
