#include <FoundationEngine/World/ECS/Archetype/Archetype.h>
#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>
#include <FoundationEngine/World/ECS/Archetype/Chunk.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs an archetype for the given component layout, building
	* its signature bitset from each component's registered ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定されたコンポーネントレイアウト用のアーキタイプを構築し、各
	* コンポーネントの登録済み ID からシグネチャビットセットを構築する。
	*/
	Archetype::Archetype(const DynamicArray<ComponentID>& layout) : layout_(layout)
	{
		/// [EN] One bit per component type, indexed by its dense internal ID, so a query can match archetypes with a bitset test.
		/// [JP] コンポーネント型ごとに、その密な内部 ID の位置へビットを1つ立てる。クエリはビットセットの判定でアーキタイプを照合できる。
		for (ComponentID id : layout)
		{
			/// [EN] Grow the signature bitset lazily to fit each component's dense internal ID before setting its bit.
			/// [JP] 各コンポーネントの密な内部 ID にビットを立てる前に、それを収められるようシグネチャビットセットを遅延的に拡張する。
			Size internalID = ComponentRegistry::GetID(id);

			/// [EN] Dense IDs are handed out in first-use order, so a later-registered type may need more bits than the set has.
			/// [JP] 密な ID は最初に使われた順に配られるので、後から登録された型ではビットセットの長さが足りないことがある。
			if (internalID >= signature_.size())
			{
				signature_.resize(internalID + 1);
			}

			signature_.set(internalID);
		}
	}

	/**
	* [EN]
	* Returns the ordered list of component types making up this archetype.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このアーキタイプを構成する、順序付きのコンポーネント型一覧を返す。
	*/
	const DynamicArray<ComponentID>& Archetype::Layout()const
	{
		return layout_;
	}

	/**
	* [EN]
	* Returns the bitset signature identifying which components this
	* archetype has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このアーキタイプがどのコンポーネントを持つかを識別する、ビットセット
	* シグネチャを返す。
	*/
	const Bitset& Archetype::Signature()const
	{
		return signature_;
	}

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
	Chunk* Archetype::CreateChunk()
	{
		/// [EN] Chunks are large (16 KB of storage), so a recycled one is reused before allocating another.
		/// [JP] チャンクは大きい(16KB の格納領域)ので、新しく確保する前に返却済みのものを使い回す。
		return chunkPool_.Create(this);
	}

	/**
	* [EN]
	* Returns chunk to the pool for later reuse.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* chunk を後で再利用できるようプールへ返却する。
	*/
	void Archetype::RecycleChunk(Chunk* chunk)
	{
		/// [EN] The chunk stays allocated in the pool, ready for the next CreateChunk().
		/// [JP] チャンクは解放せずプールに残し、次の CreateChunk() で使えるようにする。
		chunkPool_.Recycle(chunk);
	}
}
