#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
	class Chunk;

	/**
	* [EN]
	* Represents a unique combination of component types (a "layout")
	* shared by every entity routed to it. Owns a pool of Chunk objects
	* that actually store the component data, and a Bitset signature
	* used to quickly test whether this archetype has a given component.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* それにルーティングされる全エンティティが共有する、コンポーネント
	* 型の一意な組み合わせ（「レイアウト」）を表す。実際にコンポーネント
	* データを格納する Chunk オブジェクトのプールと、このアーキタイプが
	* 特定のコンポーネントを持つかを高速に判定するための Bitset
	* シグネチャを所有する。
	*/
	class SEEDCORE_API Archetype
	{
	public:
		/**
		* [EN]
		* Constructs an archetype for the given component layout,
		* building its signature bitset from each component's registered ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定されたコンポーネントレイアウト用のアーキタイプを構築し、
		* 各コンポーネントの登録済み ID からシグネチャビットセットを
		* 構築する。
		*/
		Archetype(const DynamicArray<ComponentID>& layout);

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~Archetype() = default;

		/**
		* [EN]
		* Returns the ordered list of component types making up this archetype.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このアーキタイプを構成する、順序付きのコンポーネント型一覧を
		* 返す。
		*/
		const DynamicArray<ComponentID>& Layout()const;

		/**
		* [EN]
		* Returns the bitset signature identifying which components this
		* archetype has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このアーキタイプがどのコンポーネントを持つかを識別する、
		* ビットセットシグネチャを返す。
		*/
		const Bitset& Signature()const;

		/**
		* [EN]
		* Allocates (or recycles from the pool) a new Chunk for this archetype.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このアーキタイプ用の新しい Chunk を確保する（またはプールから
		* 再利用する）。
		*/
		Chunk* CreateChunk();

		/**
		* [EN]
		* Returns chunk to the pool for later reuse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* chunk を後で再利用できるようプールへ返却する。
		*/
		void RecycleChunk(Chunk* chunk);

	private:
		/// [EN] The ordered list of component types making up this archetype.
		/// [JP] このアーキタイプを構成する、順序付きのコンポーネント型一覧。
		DynamicArray<ComponentID> layout_;

		/// [EN] Bitset signature identifying which components this archetype has, indexed by each component's registered ID.
		/// [JP] このアーキタイプがどのコンポーネントを持つかを識別するビットセットシグネチャ。各コンポーネントの登録済み ID でインデックスされる。
		Bitset signature_;

		/// [EN] Pool from which this archetype's Chunk objects are allocated/recycled.
		/// [JP] このアーキタイプの Chunk オブジェクトを確保・再利用するためのプール。
		ObjectPool<Chunk> chunkPool_;
	};
}
