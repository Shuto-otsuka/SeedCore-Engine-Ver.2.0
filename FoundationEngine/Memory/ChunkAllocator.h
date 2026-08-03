#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Bump allocator that hands out raw, uninitialized T-sized slots from a
	* growing list of fixed-size chunks. Never reclaims individual slots
	* (there is no deallocate) — intended for pointer-stable, append-only
	* storage such as StablePool's backing memory, where slot lifetime is
	* managed by the caller (placement new/explicit destructor calls) and
	* freed slots are tracked separately (e.g. a freelist), not returned here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 拡張していく固定サイズチャンクのリストから、生の未初期化な T サイズの
	* スロットを払い出すバンプアロケータ。個々のスロットを回収する機構は
	* 持たない（deallocate がない）。StablePool の裏付けメモリのような、
	* ポインタが安定した追記専用ストレージ向けを想定しており、スロットの
	* 生存期間は呼び出し側（placement new・明示的なデストラクタ呼び出し）が
	* 管理し、解放済みスロットは別途（フリーリストなどで）追跡される。
	*/
	template<typename T, Size ChunkSize = 256>
	class ChunkAllocator
	{
	public:
		/**
		* [EN]
		* One fixed-size block of raw storage for up to ChunkSize objects of T.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T のオブジェクト最大 ChunkSize 個分の、固定サイズの生ストレージブロック。
		*/
		struct Chunk
		{
			/// [EN] Raw, uninitialized storage for up to ChunkSize objects of T.
			/// [JP] T のオブジェクト最大 ChunkSize 個分の、生の未初期化ストレージ。
			alignas(T) std::byte data_[sizeof(T) * ChunkSize];

			/// [EN] Number of slots handed out from this chunk so far.
			/// [JP] このチャンクからこれまでに払い出されたスロット数。
			Uint32 used_ = 0;
		};

		/**
		* [EN]
		* Returns a pointer to a fresh, uninitialized T-sized slot,
		* allocating a new Chunk first if the current one is full. The
		* caller is responsible for constructing (placement new) and later
		* destroying the object at the returned address.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新しい未初期化の T サイズスロットへのポインタを返す。現在の
		* チャンクが満杯なら、先に新しい Chunk を確保する。返されたアドレスの
		* オブジェクトを構築（placement new）し、後で破棄する責任は
		* 呼び出し側にある。
		*/
		T* allocate()
		{
			if (chunks_.empty() || chunks_.back()->used_ == ChunkSize)
			{
				chunks_.push_back(MakePtr<Chunk>());
			}

			Chunk* chunk = chunks_.back().get();
			return reinterpret_cast<T*>(&chunk->data_[chunk->used_++ * sizeof(T)]);
		}

	private:
		/// [EN] Chunks allocated so far, in allocation order.
		/// [JP] これまでに確保されたチャンク（確保順）。
		DynamicArray<ResourcePtr<Chunk>> chunks_;
	};
}