#include <FoundationEngine/ECS/Chunk.h>
#include <FoundationEngine/ECS/Archetype.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a chunk for archetype, computing capacity_ and each
	* component sub-array's offset/size from the archetype's layout.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* archetype 用のチャンクを構築する。アーキタイプのレイアウトから
	* capacity_ と各コンポーネントサブ配列のオフセット/サイズを計算する。
	*/
	Chunk::Chunk(Archetype* archetype) : archetype_(archetype)
	{
		const auto& layout = archetype_->Layout();
		componentCount_ = layout.size();

		Size totalSize = 0;
		Size alignmentSlack = 0;
		for (Size index = 0; index < componentCount_; ++index)
		{
			ComponentID id = layout[index];
			totalSize += ComponentRegistry::GetComponentSize(id);
			Size alignment = ComponentRegistry::GetComponentAlignment(id);
			alignmentSlack += (alignment > 0) ? (alignment - 1) : 0;
		}

		if (totalSize == 0)
		{
			/// [EN] No sized components (e.g. tag-only archetype): capacity is bounded only by MaxEntitiesPerChunk.
			/// [JP] サイズを持つコンポーネントが無い場合（タグのみのアーキタイプなど）: 容量は MaxEntitiesPerChunk のみで制限される。
			capacity_ = MaxEntitiesPerChunk;
		}
		else
		{
			/// [EN] Reserve worst-case alignment padding up front, then divide the remaining usable bytes by the per-entity component size.
			/// [JP] 最悪ケースのアラインメント用パディングをあらかじめ確保し、残りの使用可能バイト数をエンティティ1体あたりのコンポーネントサイズで割る。
			Size usable = (ChunkSize > alignmentSlack) ? (ChunkSize - alignmentSlack) : 0;
			capacity_ = usable / totalSize;
			if (capacity_ > MaxEntitiesPerChunk)
			{
				capacity_ = MaxEntitiesPerChunk;
			}
		}

		Uint8* cursor = buffer_;
		for (Size index = 0; index < componentCount_; ++index)
		{
			ComponentID id = layout[index];
			Size size = ComponentRegistry::GetComponentSize(id);
			Size alignment = ComponentRegistry::GetComponentAlignment(id);

			/// [EN] Advance cursor to satisfy this component type's alignment before carving out its sub-array.
			/// [JP] このコンポーネント型のサブ配列を切り出す前に、そのアラインメント要件を満たすよう cursor を進める。
			Size padding = (alignment - (reinterpret_cast<std::uintptr_t>(cursor) % alignment)) % alignment;
			cursor += padding;

			componentSizes_[index] = size;
			componentDataPointers_[index] = cursor;

			cursor += (size * capacity_);
		}
	}

	/**
	* [EN]
	* Destructs every live component instance currently stored in this chunk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このチャンクに現在格納されている、全ての有効なコンポーネント
	* インスタンスを破棄する。
	*/
	Chunk::~Chunk()
	{
		for (Uint32 entityIndex = 0; entityIndex < entityCount_; ++entityIndex)
		{
			for (Size compIndex = 0; compIndex < componentCount_; ++compIndex)
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[compIndex]);
				Uint8* slot = componentDataPointers_[compIndex] + (componentSizes_[compIndex] * entityIndex);
				meta.destruct_(slot);
			}
		}
	}

	/**
	* [EN]
	* Appends a new row for entityID, default-constructing each
	* component's storage slot. Returns false if the chunk is already full.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entityID 用の新しい行を追加し、各コンポーネントのストレージ
	* スロットをデフォルト構築する。チャンクが既に満杯であれば
	* false を返す。
	*/
	Bool Chunk::Add(EntityID entityID)
	{
		if (entityCount_ >= capacity_)
		{
			return false;
		}

		entityIDs_[entityCount_] = entityID;

		for (Size index = 0; index < componentCount_; ++index)
		{
			Uint8* slot = componentDataPointers_[index] + (componentSizes_[index] * entityCount_);
			const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[index]);
			meta.construct_(slot);
		}

		entityCount_++;
		return true;
	}

	/**
	* [EN]
	* Removes the row belonging to entityID via swap-remove: destructs
	* its component slots, then (unless it was the last row) moves the
	* last row's components into the vacated slot. Returns the entityID
	* of whichever entity was moved into the removed row's position, or
	* a default (invalid) EntityID if none was moved.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entityID に属する行を swap-remove 方式で削除する: そのコンポーネント
	* スロットを破棄し、（それが最後の行でない限り）最後の行の
	* コンポーネントを空いたスロットへ移動する。削除された行の位置へ
	* 移動されたエンティティの entityID を返す。何も移動されなかった
	* 場合はデフォルト（無効）の EntityID を返す。
	*/
	EntityID Chunk::Remove(EntityID entityID)
	{
		if (entityCount_ == 0)
		{
			return EntityID{};
		}

		Uint32 removeIndex = UINT32_MAX;

		for (Uint32 index = 0; index < entityCount_; ++index)
		{
			if (entityIDs_[index] == entityID)
			{
				removeIndex = index;
				break;
			}
		}

		if (removeIndex == UINT32_MAX)
		{
			return EntityID{};
		}

		Uint32 lastIndex = entityCount_ - 1;

		for (Size index = 0; index < componentCount_; ++index)
		{
			const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[index]);
			Uint8* slot = componentDataPointers_[index] + (componentSizes_[index] * removeIndex);
			meta.destruct_(slot);
		}

		if (removeIndex == lastIndex)
		{
			entityCount_--;
			return EntityID{};
		}

		for (Size index = 0; index < componentCount_; ++index)
		{
			const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[index]);
			Uint8* dest = componentDataPointers_[index] + (componentSizes_[index] * removeIndex);
			Uint8* source = componentDataPointers_[index] + (componentSizes_[index] * lastIndex);
			meta.move_(dest, source);
		}

		EntityID movedEntityID = entityIDs_[lastIndex];
		entityIDs_[removeIndex] = movedEntityID;
		entityCount_--;

		return movedEntityID;
	}

	/**
	* [EN]
	* Returns a pointer to the start of the component sub-array at
	* layoutIndex within the archetype's layout.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アーキタイプのレイアウト内における layoutIndex 番目の
	* コンポーネントサブ配列の先頭へのポインタを返す。
	*/
	Uint8* Chunk::Data(Size layoutIndex)
	{
		return componentDataPointers_[layoutIndex];
	}

	/**
	* [EN]
	* Returns the per-element byte size of the component at layoutIndex.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* layoutIndex のコンポーネントの、1要素あたりのバイトサイズを返す。
	*/
	Size Chunk::Length(Size layoutIndex)const
	{
		return componentSizes_[layoutIndex];
	}

	/**
	* [EN]
	* Returns whether the chunk has reached its entity capacity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* チャンクがエンティティ容量の上限に達しているかどうかを返す。
	*/
	Bool Chunk::Full()const
	{
		return entityCount_ >= capacity_;
	}

	/**
	* [EN]
	* Returns the maximum number of entities this chunk can hold.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このチャンクが保持できる最大エンティティ数を返す。
	*/
	Size Chunk::Capacity()const
	{
		return capacity_;
	}

	/**
	* [EN]
	* Returns the EntityID stored at row index.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 行 index に格納されている EntityID を返す。
	*/
	EntityID Chunk::EntityAt(Size index)const
	{
		return entityIDs_[index];
	}

	/**
	* [EN]
	* Returns the current number of live entities in this chunk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このチャンク内の現在の有効なエンティティ数を返す。
	*/
	Uint32 Chunk::EntityCount()const
	{
		return entityCount_;
	}
}
