#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
#if defined(SC_POINTER_BITS)
#elif defined(__x86_64__)||defined(_M_X64)||defined(_M_AMD64)
#define SC_POINTER_BITS 48
#elif defined(__aarch64)||defined(_M_ARM64)
#define SC_POINTER_BITS 48
#elif defined(__riscv)&&__riscv_xlen == 64
#define SC_POINTER_BITS 48
#else
#define SC_POINTER_BITS (sizeof(void*) * CHAR_BIT)
#endif

	/// [EN] Cache line size, used to pad hot per-shard state apart to avoid
	///      false sharing between threads.
	/// [JP] キャッシュライン幅。スレッド間の偽共有を避けるため、シャードごとの
	///      ホットな状態を離して配置するのに使う。
	constexpr inline Size SC_CACHELINE_SIZE =
#ifdef __cpp_lib_hardware_interference_size
		std::hardware_destructive_interference_size;
#else
		64;
#endif

	/**
	* [EN]
	* Unpacked pointer+tag pair for a lock-free freelist head. The tag
	* is incremented on every update to guard against the ABA problem.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ロックフリーなフリーリストの先頭を表す、ポインタとタグの非圧縮ペア。
	* ABA問題を防ぐため、更新のたびにタグをインクリメントする。
	*/
	struct SynchronizedPointer
	{
		using PointerType = std::uintptr_t;
		using TagType     = std::uintptr_t;

		/// [EN] Address of the freelist head block (0 = empty).
		/// [JP] フリーリスト先頭ブロックのアドレス（0 = 空）。
		PointerType ptr_{ 0 };

		/// [EN] ABA-guard counter, incremented on every freelist mutation.
		/// [JP] フリーリスト更新のたびにインクリメントされる、ABA対策用カウンタ。
		TagType     tag_{ 0 };

		SynchronizedPointer() = default;

		SynchronizedPointer(PointerType ptr, TagType tag)noexcept;

		/**
		* [EN]
		* Returns the freelist head address.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フリーリスト先頭のアドレスを返す。
		*/
		inline SynchronizedPointer::PointerType GetPtr()const noexcept
		{
			return ptr_;
		}

		/**
		* [EN]
		* Returns the current ABA-guard tag.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のABA対策用タグを返す。
		*/
		inline SynchronizedPointer::TagType GetTag()const noexcept
		{
			return tag_;
		}
	};

	/**
	* [EN]
	* Pointer+tag pair packed into a single uintptr_t, so it can be
	* updated atomically without double-width CAS. PtrBits bits hold
	* the pointer, the remaining bits hold the ABA-guard tag.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ポインタとタグを単一の uintptr_t に詰め込んだペア。倍幅CASなしで
	* アトミックに更新できる。PtrBits ビットがポインタ、残りのビットが
	* ABA対策用タグを保持する。
	*/
	template <Int PtrBits = SC_POINTER_BITS>
	struct PackedSynchronizedPointer
	{
		using PointerType = std::uintptr_t;
		using TagType     = Uint16;

		static constexpr Int PTR_BITS = PtrBits;
		static constexpr Int TAG_BITS = 64 - PtrBits;
		static constexpr PointerType PTR_MASK = (PointerType{ 1 } << PTR_BITS) - 1;

		/// [EN] Packed pointer (low PTR_BITS bits) and tag (remaining bits).
		/// [JP] 詰め込まれたポインタ（下位 PTR_BITS ビット）とタグ（残りのビット）。
		std::uintptr_t bits_{ 0 };

		PackedSynchronizedPointer() = default;

		PackedSynchronizedPointer(PointerType ptr, TagType tag)noexcept :bits_((ptr& PTR_MASK) | (static_cast<std::uintptr_t>(tag) << PTR_BITS))
		{
			/// No Code
		}

		/**
		* [EN]
		* Unpacks and returns the pointer portion.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ポインタ部分を取り出して返す。
		*/
		inline PointerType GetPtr()const noexcept
		{
			return bits_ & PTR_MASK;
		}

		/**
		* [EN]
		* Unpacks and returns the ABA-guard tag portion.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ABA対策用タグ部分を取り出して返す。
		*/
		inline TagType GetTag()const noexcept
		{
			return static_cast<TagType>(bits_ >> PTR_BITS);
		}
	};

	/**
	* [EN]
	* Fixed-size storage slot for one T, plus the intrusive freelist
	* link and owning-shard ID used by ObjectPool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T 1個分の固定サイズストレージスロット。ObjectPool が使う
	* 侵入型フリーリストのリンクと、所属シャードIDを併せ持つ。
	*/
	template<typename T>
	struct ObjectBlock
	{
		/// [EN] Index of the ObjectPool shard that owns this block.
		/// [JP] このブロックを所有する ObjectPool シャードのインデックス。
		Uint16 poolID_;

		/// [EN] Intrusive link to the next free block in this shard's freelist.
		/// [JP] このシャードのフリーリスト内で次の空きブロックへの侵入型リンク。
		std::atomic<ObjectBlock*> nextFree_ = nullptr;

		/// [EN] Raw storage for the constructed T object.
		/// [JP] 構築済み T オブジェクトのための生ストレージ。
		alignas(T)std::byte storage_[sizeof(T)];

		/**
		* [EN]
		* Returns a pointer to the object living in storage_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* storage_ 上に存在するオブジェクトへのポインタを返す。
		*/
		T* object()noexcept
		{
			return std::launder(reinterpret_cast<T*>(storage_));
		}

		/**
		* [EN]
		* Const overload of object().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* object() の const オーバーロード。
		*/
		const T* object()const noexcept
		{
			return std::launder(reinterpret_cast<const T*>(storage_));
		}

		/**
		* [EN]
		* Recovers the owning ObjectBlock from a pointer previously
		* returned by object().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* object() が返したポインタから、所有元の ObjectBlock を復元する。
		*/
		static ObjectBlock<T>* from(T* obj)noexcept
		{
			return reinterpret_cast<ObjectBlock*>(reinterpret_cast<Char*>(obj) - offsetof(ObjectBlock, storage_));
		}
	};

	/**
	* [EN]
	* Thread-friendly, fixed-block-size object pool. Allocations are
	* spread across 2^LogSize shards (keyed by thread hash) to reduce
	* cross-thread contention; each shard is a lock-free freelist
	* backed by a synchronized_pool_resource.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スレッドフレンドリーな、固定ブロックサイズのオブジェクトプール。
	* スレッド間の競合を減らすため、割り当ては（スレッドハッシュで
	* 決まる）2^LogSize 個のシャードに分散される。各シャードは
	* synchronized_pool_resource を裏付けとするロックフリーな
	* フリーリストである。
	*/
	template<typename T, typename H = SynchronizedPointer, Size LogSize = 5>
	class ObjectPool
	{
	private:
		using Block = ObjectBlock<T>;

		static constexpr Size NumberPools = 1u << LogSize;

		/**
		* [EN]
		* Per-shard state: a lock-free freelist plus its own backing
		* memory resource. Padded to a cache line to avoid false
		* sharing between shards accessed from different threads.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シャードごとの状態: ロックフリーなフリーリストと、それ専用の
		* バッキングメモリリソース。異なるスレッドからアクセスされる
		* シャード間の偽共有を避けるため、キャッシュライン幅にパディングする。
		*/
		struct alignas(SC_CACHELINE_SIZE) Shard
		{
			/// [EN] Head of this shard's lock-free freelist.
			/// [JP] このシャードのロックフリーなフリーリストの先頭。
			std::atomic<H> freeHead_{ H{} };

			/// [EN] Backing memory resource for blocks allocated by this shard.
			/// [JP] このシャードが確保するブロックのためのバッキングメモリリソース。
			alignas(SC_CACHELINE_SIZE) std::pmr::synchronized_pool_resource backing_
			{
				std::pmr::pool_options
				{
					.max_blocks_per_chunk = 1024,
					.largest_required_pool_block = sizeof(Block)
				}
			};

			/**
			* [EN]
			* Pushes block onto this shard's freelist (lock-free).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* block をこのシャードのフリーリストに push する（ロックフリー）。
			*/
			void PushFree(Block* block)noexcept
			{
				H current = freeHead_.load(std::memory_order_relaxed);
				H next;
				do
				{
					block->nextFree_.store(reinterpret_cast<Block*>(current.GetPtr()), std::memory_order_relaxed);
					next = H(reinterpret_cast<typename H::PointerType>(block), static_cast<typename H::TagType>(current.GetTag() + 1));
				} while (!freeHead_.compare_exchange_weak(current, next, std::memory_order_release, std::memory_order_relaxed));
			}

			/**
			* [EN]
			* Pops and returns a block from this shard's freelist
			* (lock-free), or nullptr if the freelist is empty.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* このシャードのフリーリストからブロックを pop して返す
			* （ロックフリー）。フリーリストが空なら nullptr。
			*/
			Block* PopFree()noexcept
			{
				H current = freeHead_.load(std::memory_order_acquire);
				while (current.GetPtr())
				{
					auto* ptr = reinterpret_cast<Block*>(current.GetPtr());
					Block* nextBlock = ptr->nextFree_.load(std::memory_order_relaxed);
					H next(reinterpret_cast<typename H::PointerType>(nextBlock), static_cast<typename H::TagType>(current.GetTag() + 1));
					if (freeHead_.compare_exchange_weak(current, next, std::memory_order_acquire, std::memory_order_acquire))
					{
						return ptr;
					}
				}
				return nullptr;
			}

			/**
			* [EN]
			* Allocates a fresh block from backing_ when the freelist is empty.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* フリーリストが空のとき、backing_ から新しいブロックを確保する。
			*/
			Block* AllocateBacking()
			{
				return static_cast<Block*>(backing_.allocate(sizeof(Block), alignof(Block)));
			}

			/**
			* [EN]
			* Returns a block's storage to backing_ (used by Release).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* ブロックのストレージを backing_ に返却する（Release が使用）。
			*/
			void DeallocateBacking(Block* block)
			{
				backing_.deallocate(block, sizeof(Block), alignof(Block));
			}
		};

		/// [EN] The pool's shards.
		/// [JP] このプールのシャード群。
		StaticArray<Shard, NumberPools> shards_;

		/**
		* [EN]
		* Picks a shard index for the calling thread, round-robin per
		* thread starting from a thread-hash-derived offset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 呼び出しスレッド用のシャードインデックスを選ぶ。スレッドハッシュ
		* 由来のオフセットから開始し、スレッドごとにラウンドロビンする。
		*/
		static Size Next()noexcept
		{
			thread_local Size counter = std::hash<std::thread::id>{}(std::this_thread::get_id());
			return counter++ & (NumberPools - 1);
		}

	public:
		ObjectPool() = default;

		ObjectPool(const ObjectPool&) = delete;

		ObjectPool& operator=(const ObjectPool&) = delete;

		~ObjectPool() = default;

		/**
		* [EN]
		* Constructs a new T from args in a pooled block (reusing a
		* freed block if one is available on the chosen shard) and
		* returns a pointer to it. The caller must eventually pass the
		* pointer to Recycle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プールされたブロック内に args から新しい T を構築し
		* （選ばれたシャードに解放済みブロックがあれば再利用する）、
		* そのポインタを返す。呼び出し側は最終的にそのポインタを
		* Recycle に渡す必要がある。
		*/
		template<typename... Args>
		[[nodiscard]] T* Create(Args&&... args)
		{
			auto sid = Next();
			auto& shard = shards_[sid];

			Block* block = shard.PopFree();
			if (!block)
			{
				block = shard.AllocateBacking();
			}

			block->poolID_ = static_cast<Uint16>(sid);
			return std::construct_at(block->object(), std::forward<Args>(args)...);
		}

		/**
		* [EN]
		* Destroys the object pointed to by obj (created via Create)
		* and returns its block to its owning shard's freelist.
		* No-op if obj is nullptr.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* obj（Create で生成されたもの）が指すオブジェクトを破棄し、
		* そのブロックを所属シャードのフリーリストに返却する。
		* obj が nullptr の場合は何もしない。
		*/
		void Recycle(T* obj)
		{
			if (!obj)
			{
				return;
			}
			auto* block = Block::from(obj);
			std::destroy_at(block->object());
			shards_[block->poolID_].PushFree(block);
		}

		/**
		* [EN]
		* Releases all backing memory across every shard, invalidating
		* any outstanding pointers previously returned by Create.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全シャードのバッキングメモリを解放する。以前 Create が
		* 返した未解放のポインタは全て無効になる。
		*/
		void Release()
		{
			for (auto& shard : shards_)
			{
				shard.backing_.release();
				shard.freeHead_.store(H{}, std::memory_order_relaxed);
			}
		}
	};
}