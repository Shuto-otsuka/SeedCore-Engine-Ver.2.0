#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class Archetype;

	/**
	* [EN]
	* Fixed-size (16 KB), cache-line-aligned block of structure-of-arrays
	* component storage for entities sharing a single Archetype's
	* layout. Each component type gets its own contiguous, aligned
	* sub-array within buffer_, sized for capacity_ entities (computed
	* once in the constructor from the archetype's component
	* sizes/alignments). Removal is swap-remove: the moved entity's ID
	* is returned so callers can fix up its EntityRecord.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一の Archetype のレイアウトを共有するエンティティ向けの、固定
	* サイズ（16KB）でキャッシュライン整列された、構造体配列（SoA）
	* コンポーネントストレージのブロック。各コンポーネント型は buffer_
	* 内に、capacity_ エンティティ分のサイズを持つ連続した整列済み
	* サブ配列を持つ（コンストラクタでアーキタイプのコンポーネント
	* サイズ/アラインメントから一度だけ計算される）。削除は
	* swap-remove 方式であり、移動したエンティティの ID を返すため、
	* 呼び出し側はその EntityRecord を修正できる。
	*/
	class SEEDCORE_API Chunk
	{
	private:
		/// [EN] Total byte size of the raw storage buffer.
		/// [JP] 生ストレージバッファの総バイトサイズ。
		static constexpr Size ChunkSize = 16 * 1024;

		/// [EN] Hard cap on the number of entities a single chunk can ever hold.
		/// [JP] 単一チャンクが保持できるエンティティ数の絶対上限。
		static constexpr Size MaxEntitiesPerChunk = 1024;

		/// [EN] Hard cap on the number of distinct component types a single archetype/chunk can hold.
		/// [JP] 単一のアーキタイプ/チャンクが保持できる、異なるコンポーネント型数の絶対上限。
		static constexpr Size MaxComponentsPerArchetype = 32;

		/// [EN] Per-component-type pointers into buffer_ marking where each type's sub-array begins.
		/// [JP] 各コンポーネント型のサブ配列が buffer_ 内で始まる位置を示す、型ごとのポインタ。
		Uint8* componentDataPointers_[MaxComponentsPerArchetype];

		/// [EN] Per-component-type element byte size, indexed the same as componentDataPointers_.
		/// [JP] 型ごとの1要素あたりのバイトサイズ。componentDataPointers_ と同じインデックスでアクセスする。
		Size componentSizes_[MaxComponentsPerArchetype];

		/// [EN] Number of distinct component types stored in this chunk (mirrors the owning archetype's layout size).
		/// [JP] このチャンクに格納されている、異なるコンポーネント型の数（所有元アーキタイプのレイアウトサイズと一致する）。
		Size componentCount_ = 0;

		/// [EN] Maximum number of entities this chunk can hold, computed from component sizes/alignments and ChunkSize.
		/// [JP] このチャンクが保持できる最大エンティティ数。コンポーネントのサイズ/アラインメントと ChunkSize から計算される。
		Size capacity_ = 0;

	public:
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
		Chunk(Archetype* archetype);

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
		~Chunk();

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
		Bool Add(EntityID entityID);

		/**
		* [EN]
		* Removes the row belonging to entityID via swap-remove:
		* destructs its component slots, then (unless it was the last
		* row) moves the last row's components into the vacated slot.
		* Returns the entityID of whichever entity was moved into the
		* removed row's position (so its EntityRecord can be updated),
		* or a default (invalid) EntityID if none was moved (entityID not
		* found, or it was already the last row).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entityID に属する行を swap-remove 方式で削除する: そのコンポーネント
		* スロットを破棄し、（それが最後の行でない限り）最後の行の
		* コンポーネントを空いたスロットへ移動する。削除された行の位置へ
		* 移動されたエンティティの entityID を返す（呼び出し側がその
		* EntityRecord を更新できるように）。何も移動されなかった場合
		* （entityID が見つからない、またはそれが既に最後の行だった場合）は
		* デフォルト（無効）の EntityID を返す。
		*/
		EntityID Remove(EntityID entityID);

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
		Uint8* Data(Size layoutIndex);

		/**
		* [EN]
		* Returns the per-element byte size of the component at layoutIndex.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* layoutIndex のコンポーネントの、1要素あたりのバイトサイズを返す。
		*/
		Size Length(Size layoutIndex)const;

		/**
		* [EN]
		* Returns whether the chunk has reached its entity capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* チャンクがエンティティ容量の上限に達しているかどうかを返す。
		*/
		Bool Full()const;

		/**
		* [EN]
		* Returns the maximum number of entities this chunk can hold.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このチャンクが保持できる最大エンティティ数を返す。
		*/
		Size Capacity()const;

		/**
		* [EN]
		* Returns the EntityID stored at row index.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 行 index に格納されている EntityID を返す。
		*/
		EntityID EntityAt(Size index)const;

		/**
		* [EN]
		* Returns the current number of live entities in this chunk.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このチャンク内の現在の有効なエンティティ数を返す。
		*/
		Uint32 EntityCount()const;

	private:
		/// [EN] The archetype this chunk's layout was derived from.
		/// [JP] このチャンクのレイアウトの導出元となったアーキタイプ。
		Archetype* archetype_;

		/// [EN] Raw, cache-line-aligned backing storage for every component's sub-array.
		/// [JP] 各コンポーネントのサブ配列を保持する、キャッシュライン整列された生のバッキングストレージ。
		alignas(64) Uint8 buffer_[ChunkSize];

		/// [EN] Current number of live entities (rows) in this chunk.
		/// [JP] このチャンク内の現在の有効なエンティティ（行）数。
		Uint32 entityCount_ = 0;

		/// [EN] Per-row EntityID, parallel to the component sub-arrays in buffer_.
		/// [JP] buffer_ 内のコンポーネントサブ配列と対応する、行ごとの EntityID。
		alignas(64) EntityID entityIDs_[MaxEntitiesPerChunk];
	};
}
