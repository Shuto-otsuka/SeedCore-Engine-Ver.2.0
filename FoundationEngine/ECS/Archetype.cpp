#include <FoundationEngine/ECS/Archetype.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/Chunk.h>

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
		for (ComponentID id : layout)
		{
			/// [EN] Grow the signature bitset lazily to fit each component's dense internal ID before setting its bit.
			/// [JP] 各コンポーネントの密な内部 ID にビットを立てる前に、それを収められるようシグネチャビットセットを遅延的に拡張する。
			Size internalID = ComponentRegistry::GetID(id);

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
		chunkPool_.Recycle(chunk);
	}
}
