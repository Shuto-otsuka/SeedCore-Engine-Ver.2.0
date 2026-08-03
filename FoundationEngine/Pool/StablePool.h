#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Memory/ChunkAllocator.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Concept that constrains types usable with StablePool.
	* Requires nothrow destructibility and nothrow move constructibility
	* to ensure safe internal management without exception handling overhead.
	* 
	* ---------------------------------------------------------------------
	* 
	* [JP]
	* StablePool で使用できる型を制約するコンセプト。
	* 例外処理のオーバーヘッドなく安全に内部管理できるよう、
	* nothrow で破棄・ムーブ構築できる型のみを許可する。
	*/
	template <typename T>
	concept Poolable = std::is_nothrow_destructible_v<T> && std::is_nothrow_move_constructible_v<T>;

	/**
	* [EN]
	* A pool that guarantees pointer stability for managed objects.
	* Objects are allocated via ChunkAllocator and accessed through
	* versioned Handles. Stale handles (pointing to destroyed objects)
	* are detected via a per-slot generation counter.
	* 
	* ---------------------------------------------------------------------
	* 
	* [JP]
	* 管理オブジェクトのポインタ安定性を保証するプール。
	* オブジェクトは ChunkAllocator で確保され、バージョン付き Handle を通してアクセスする。
	* スロットごとの generation カウンタにより、破棄済みオブジェクトへの
	* 古い Handle を検出できる。
	*/
	template <Poolable T>
	class StablePool
	{
	public:
		StablePool() = default;

		StablePool(const StablePool&) = delete;
		StablePool& operator=(const StablePool&) = delete;

		~StablePool()
		{
			for (auto& slot : slots_)
			{
				if (slot.alive_ && slot.obj_)
				{
					slot.obj_->~T();
					slot.obj_ = nullptr;
					slot.alive_ = false;
				}
			}
		}

		/**
		* [EN]
		* Creates a new object in the pool.
		* Reuses a slot from the freelist if available, otherwise appends a new slot.
		* The object is constructed in-place via placement new.
		* Returns a versioned Handle that uniquely identifies this object instance.
		* 
		* ---------------------------------------------------------------------
		* 
		* [JP]
		* プール内に新しいオブジェクトを生成する。
		* フリーリストに空きスロットがあれば再利用し、なければ新しいスロットを追加する。
		* オブジェクトは placement new でその場に構築される。
		* このオブジェクトのインスタンスを一意に識別する、バージョン付き Handle を返す。
		*/
		Handle<T> Create()
		{
			Uint32 index;

			if (!freelist_.empty())
			{
				index = freelist_.back();
				freelist_.pop_back();
			}
			else
			{
				index = static_cast<Uint32>(slots_.size());
				slots_.emplace_back();
			}

			Slot& slot = slots_[index];

			T* obj = allocator_.allocate();
			new(obj)T();

			slot.obj_ = obj;
			slot.alive_ = true;

			return Handle<T>{index, slot.generation_};
		}

		/**
		* [EN]
		* Retrieves a raw pointer to the object identified by the given Handle.
		* Returns nullptr if the Handle is invalid, out of range,
		* the slot is inactive, or the generation does not match (stale handle).
		* 
		* ---------------------------------------------------------------------
		* 
		* [JP]
		* 指定した Handle が示すオブジェクトへの生ポインタを取得する。
		* Handle が無効・範囲外・スロットが非アクティブ・
		* generation 不一致（古い Handle）の場合は nullptr を返す。
		*/
		T* Get(const Handle<T>& handle)
		{
			if (!handle.exists())
			{
				return nullptr;
			}

			if (handle.index_ >= slots_.size())
			{
				return nullptr;
			}

			Slot& slot = slots_[handle.index_];

			if (!slot.alive_)
			{
				return nullptr;
			}

			if (slot.generation_ != handle.generation_)
			{
				return nullptr;
			}

			return slot.obj_;
		}

		/**
		* [EN]
		* Const version of Get. Retrieves a const raw pointer to the object.
		* Allows access to the pool from const contexts.
		* * ---------------------------------------------------------------------
		* * [JP]
		* Get の const 版。オブジェクトへの const 生ポインタを取得する。
		* const コンテキストからプールにアクセスできるようにする。
		*/
		const T* Get(const Handle<T>& handle)const
		{
			if (!handle.exists())
			{
				return nullptr;
			}

			if (handle.index_ >= slots_.size())
			{
				return nullptr;
			}

			const Slot& slot = slots_[handle.index_];

			if (!slot.alive_)
			{
				return nullptr;
			}

			if (slot.generation_ != handle.generation_)
			{
				return nullptr;
			}

			return slot.obj_;
		}

		/**
		* [EN]
		* Destroys the object identified by the given Handle.
		* Calls the destructor explicitly, marks the slot as inactive,
		* increments the generation counter to invalidate any existing Handles,
		* and returns the slot index to the freelist for reuse.
		* No-op if the Handle is invalid or already stale.
		* 
		* ---------------------------------------------------------------------
		* 
		* [JP]
		* 指定した Handle が示すオブジェクトを破棄する。
		* デストラクタを明示的に呼び出し、スロットを非アクティブとしてマークし、
		* generation カウンタをインクリメントして既存の Handle を無効化したうえで、
		* スロットのインデックスをフリーリストに戻して再利用可能にする。
		* Handle が無効または既に古い場合は何もしない。
		*/
		void Destroy(const Handle<T>& handle)
		{
			if (!handle.exists())
			{
				return;
			}

			if (handle.index_ >= slots_.size())
			{
				return;
			}

			Slot& slot = slots_[handle.index_];

			if (!slot.alive_)
			{
				return;
			}

			if (slot.generation_ != handle.generation_)
			{
				return;
			}

			slot.obj_->~T();

			slot.obj_ = nullptr;
			slot.alive_ = false;
			slot.generation_++;

			freelist_.push_back(static_cast<Uint32>(handle.index_));
		}

	private:
		/**
		* [EN]
		* Internal slot that tracks a single managed object.
		* The generation counter is incremented on each Destroy call,
		* allowing Handles issued before the destroy to be detected as stale.
		* 
		* ---------------------------------------------------------------------
		* 
		* [JP]
		* 管理オブジェクト1つを追跡する内部スロット。
		* generation カウンタは Destroy のたびにインクリメントされ、
		* 破棄前に発行された Handle を古いものとして検出できる。
		*/
		struct Slot
		{
			T* obj_ = nullptr;
			Uint64 generation_ = 1;
			Bool alive_ = false;
		};
		DynamicArray<Slot> slots_;
		DynamicArray<Uint32> freelist_;

		ChunkAllocator<T> allocator_;
	};
}