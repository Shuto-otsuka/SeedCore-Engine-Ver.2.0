#include <FoundationEngine/World/ECS/Archetype/Chunk.h>
#include <FoundationEngine/World/ECS/Archetype/Archetype.h>
#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>

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

		/// [EN] Per-entity byte size of one row, and the worst-case padding every sub-array may need to reach its alignment.
		/// [JP] 1行(エンティティ1体)分のバイト数と、各サブ配列が整列のために必要としうる最悪のパディング量。
		Size totalSize = 0;
		Size alignmentSlack = 0;
		for (Size index = 0; index < componentCount_; ++index)
		{
			ComponentID id = layout[index];
			totalSize += ComponentRegistry::ComponentSize(id);

			/// [EN] Aligning a sub-array start wastes at most alignment - 1 bytes; a zero alignment (no data) wastes nothing.
			/// [JP] サブ配列の先頭を揃えるときに無駄になるのは最大で alignment - 1 バイト。alignment が 0(データ無し)なら無駄は無い。
			Size alignment = ComponentRegistry::ComponentAlignment(id);
			alignmentSlack += (alignment > 0) ? (alignment - 1) : 0;
		}

		if (totalSize == 0)
		{
			/// [EN] No sized components (e.g. tag-only archetype): capacity is bounded only by maxEntitiesPerChunk_.
			/// [JP] サイズを持つコンポーネントが無い場合（タグのみのアーキタイプなど）: 容量は maxEntitiesPerChunk_ のみで制限される。
			capacity_ = maxEntitiesPerChunk_;
		}
		else
		{
			/// [EN] Reserve worst-case alignment padding up front, then divide the remaining usable bytes by the per-entity component size.
			/// [JP] 最悪ケースのアラインメント用パディングをあらかじめ確保し、残りの使用可能バイト数をエンティティ1体あたりのコンポーネントサイズで割る。
			Size usable = (chunkSize_ > alignmentSlack) ? (chunkSize_ - alignmentSlack) : 0;
			capacity_ = usable / totalSize;

			/// [EN] Small rows could fit more entities than entityIDs_ has slots, so the capacity is capped there.
			/// [JP] 1行が小さいと entityIDs_ の枠より多く入ってしまうので、容量はそこで頭打ちにする。
			if (capacity_ > maxEntitiesPerChunk_)
			{
				capacity_ = maxEntitiesPerChunk_;
			}
		}

		/// [EN] Carve buffer_ into one aligned sub-array per component, each capacity_ elements long, in layout order.
		/// [JP] buffer_ をレイアウトの順に、コンポーネントごとの整列済みサブ配列(それぞれ capacity_ 要素分)へ切り分ける。
		Uint8* cursor = buffer_;
		for (Size index = 0; index < componentCount_; ++index)
		{
			ComponentID id = layout[index];
			Size size = ComponentRegistry::ComponentSize(id);
			Size alignment = ComponentRegistry::ComponentAlignment(id);

			/// [EN] Advance cursor to the next multiple of alignment: (alignment - address % alignment) % alignment is 0 when already aligned.
			/// [JP] cursor を alignment の次の倍数まで進める。(alignment - アドレス % alignment) % alignment は、揃っていれば 0 になる。
			Size padding = (alignment - (reinterpret_cast<std::uintptr_t>(cursor) % alignment)) % alignment;
			cursor += padding;

			componentSizes_[index] = size;
			componentDataPointers_[index] = cursor;

			/// [EN] The next sub-array starts right after this one's capacity_ elements.
			/// [JP] 次のサブ配列は、このサブ配列の capacity_ 要素分の直後から始まる。
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
		/// [EN] buffer_ is raw bytes, so each live component must be destructed explicitly through its registered destructor.
		/// [JP] buffer_ は生のバイト列なので、有効な各コンポーネントは登録済みのデストラクタで明示的に破棄する。
		for (Uint32 entityIndex = 0; entityIndex < entityCount_; ++entityIndex)
		{
			for (Size compIndex = 0; compIndex < componentCount_; ++compIndex)
			{
				/// [EN] Element entityIndex of a sub-array sits at its start plus entityIndex elements.
				/// [JP] サブ配列の entityIndex 番目の要素は、先頭から entityIndex 要素分進んだ位置にある。
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
		/// [EN] A full chunk refuses the row; the archetype then opens a new chunk.
		/// [JP] 満杯のチャンクは行を受け付けない。その場合はアーキタイプが新しいチャンクを用意する。
		if (entityCount_ >= capacity_)
		{
			return false;
		}

		/// [EN] The new row goes at the end: record its entity, then default-construct each component in place.
		/// [JP] 新しい行は末尾に置く。エンティティを記録してから、各コンポーネントをその場でデフォルト構築する。
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

		/// [EN] Find the entity's row by a linear scan; UINT32_MAX means it is not in this chunk.
		/// [JP] エンティティの行を線形探索で探す。UINT32_MAX はこのチャンクに居ないことを表す。
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

		/// [EN] Destroy the removed row's components first.
		/// [JP] まず、削除する行のコンポーネントを破棄する。
		for (Size index = 0; index < componentCount_; ++index)
		{
			const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[index]);
			Uint8* slot = componentDataPointers_[index] + (componentSizes_[index] * removeIndex);
			meta.destruct_(slot);
		}

		/// [EN] Removing the last row leaves no gap, so nothing moves.
		/// [JP] 最後の行を消した場合は穴が空かないので、何も移動しない。
		if (removeIndex == lastIndex)
		{
			entityCount_--;

			/// [EN] An invalid EntityID tells the caller that no other entity changed rows.
			/// [JP] 無効な EntityID は、行が変わったエンティティが他に無いことを呼び出し側に伝える。
			return EntityID{};
		}

		/// [EN] Swap-remove: move the last row into the gap so the rows stay packed.
		/// [JP] swap-remove: 最後の行を空いた位置へ移し、行を詰めたままにする。
		for (Size index = 0; index < componentCount_; ++index)
		{
			const ComponentMetadata& meta = ComponentRegistry::Get(archetype_->Layout()[index]);
			Uint8* dest = componentDataPointers_[index] + (componentSizes_[index] * removeIndex);
			Uint8* source = componentDataPointers_[index] + (componentSizes_[index] * lastIndex);

			/// [EN] move_ leaves the source slot moved-from; it is past the new end, so it is simply forgotten.
			/// [JP] move_ の後、移動元の枠は新しい末尾より後ろになるので、そのまま使われなくなる。
			meta.move_(dest, source);
		}

		/// [EN] The moved entity now lives at removeIndex; returned so the caller can update its EntityRecord.
		/// [JP] 移されたエンティティは removeIndex に居る。呼び出し側がその EntityRecord を直せるよう返す。
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
