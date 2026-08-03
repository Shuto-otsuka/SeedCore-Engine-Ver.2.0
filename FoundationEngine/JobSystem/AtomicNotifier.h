#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/WorkerCommon.h>
#include <FoundationEngine/Log/Assert.h>
#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Lock-free "eventcount" wait/notify primitive (std::atomic<Uint64>::wait
	* based) used to park and wake worker threads without missing wakeups.
	* A single 64-bit state_ packs an epoch (upper 32 bits) and the current
	* waiter count (lower 32 bits): a waiter records the epoch it observed
	* in prepare_wait, then only actually blocks in commit_wait if the
	* epoch hasn't since changed — any notify_one/notify_all in between
	* bumps the epoch first, so the race where a wakeup fires before the
	* waiter starts sleeping can never be missed. Selected as the job
	* system's notifier when SC_ENABLE_ATOMIC_NOTIFIER is set (see
	* WorkerCommon.h); NonblockingNotifier is used otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ロックフリーな「eventcount」待機/通知プリミティブ
	* （std::atomic<Uint64>::wait ベース）。ワーカースレッドを、起床見逃し
	* なく待機・起床させるために使う。単一の64ビット state_ に、エポック
	* （上位32ビット）と現在の待機者数（下位32ビット）を詰め込む。
	* 待機者は prepare_wait で観測したエポックを記録し、commit_wait では
	* そのエポックが変化していない場合にのみ実際にブロックする — 途中の
	* notify_one/notify_all は先にエポックを進めるため、待機者が眠りに
	* 就く前に起床が発生する競合を見逃すことはない。WorkerCommon.h の
	* SC_ENABLE_ATOMIC_NOTIFIER が設定されている場合にジョブシステムの
	* notifier として選ばれる。それ以外では NonblockingNotifier が使われる。
	*/
	class AtomicNotifier :public NonTransferable
	{
	private:
		friend class Executor;

	public:
		/**
		* [EN]
		* Per-waiter slot: the epoch observed at prepare_wait, cache-line
		* padded so concurrent waiters don't false-share.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 待機者ごとのスロット。prepare_wait で観測したエポックを保持し、
		* 並行する待機者同士が偽共有しないようキャッシュライン単位で
		* パディングされている。
		*/
		struct Waiter
		{
			/// [EN] Epoch observed when this waiter called prepare_wait.
			/// [JP] この待機者が prepare_wait を呼んだ時点で観測したエポック。
			alignas(2 * SC_CACHELINE_SIZE) Uint32 epoch_;
		};

		/**
		* [EN]
		* Constructs a notifier with n waiter slots (index range [0, n)
		* passed to prepare_wait/commit_wait/cancel_wait).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n個の待機者スロットを持つ notifier を構築する
		* （prepare_wait/commit_wait/cancel_wait に渡すインデックス範囲は [0, n)）。
		*/
		AtomicNotifier(Size n)noexcept :state_(0), waiters_(n)
		{
			/// No Code
		}

		/**
		* [EN]
		* Asserts that no waiter is currently registered before destruction
		* (a live waiter here would indicate a use-after-free elsewhere).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 破棄前に、現在登録中の待機者がいないことをアサートする
		* （ここで待機者が残っていれば、他所での use-after-free を示す）。
		*/
		~AtomicNotifier()
		{
			SC_ASSERT((state_.load(std::memory_order_relaxed) & WAITER_MASK) == 0, "通知処理の開始時に待機中のスレッドが残っています（State: {:#x}）", state_.load(std::memory_order_relaxed));
		}

		/**
		* [EN]
		* Returns the current number of registered waiters.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在登録されている待機者数を返す。
		*/
		Size count()const noexcept
		{
			return state_.load(std::memory_order_relaxed) & WAITER_MASK;
		}

		/**
		* [EN]
		* Registers waiter as about to wait: increments the waiter count
		* and records the current epoch. Must be paired with a later
		* commit_wait or cancel_wait on the same waiter index. Call this
		* before re-checking whatever condition you're waiting on, so a
		* notify that races with the check is still observed via the
		* epoch bump.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* waiter がこれから待機することを登録する: 待機者数をインクリメントし、
		* 現在のエポックを記録する。同じ waiter インデックスに対して、後で
		* commit_wait または cancel_wait と対にする必要がある。待機対象の
		* 条件を再チェックする前にこれを呼ぶことで、チェックと競合する
		* notify があってもエポックの進行によって見逃さないようにする。
		*/
		void prepare_wait(Size waiter)noexcept
		{
			auto previous = state_.fetch_add(WAITER_INC, std::memory_order_relaxed);
			waiters_[waiter].epoch_ = (previous >> EPOCH_SHIFT);
			std::atomic_thread_fence(std::memory_order_seq_cst);
		}

		/**
		* [EN]
		* Actually blocks waiter until the epoch it recorded in
		* prepare_wait changes (i.e. until some notify occurs), then
		* decrements the waiter count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* waiter が prepare_wait で記録したエポックが変化するまで
		* （すなわち何らかの notify が発生するまで）実際にブロックし、
		* その後待機者数をデクリメントする。
		*/
		void commit_wait(Size waiter)noexcept
		{
			Uint64 previous = state_.load(std::memory_order_relaxed);
			while ((previous >> EPOCH_SHIFT) == waiters_[waiter].epoch_)
			{
				state_.wait(previous, std::memory_order_relaxed);
				previous = state_.load(std::memory_order_relaxed);
			}
			state_.fetch_sub(WAITER_INC, std::memory_order_relaxed);
		}

		/**
		* [EN]
		* Cancels a pending prepare_wait without actually blocking (e.g.
		* the caller found work to do before calling commit_wait):
		* decrements the waiter count back down.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 実際にはブロックせずに、保留中の prepare_wait を取り消す
		* （例: commit_wait を呼ぶ前に呼び出し側が実行すべき仕事を見つけた
		* 場合）: 待機者数を元に戻す。
		*/
		void cancel_wait(Size /* No Argument */)noexcept
		{
			state_.fetch_sub(WAITER_INC, std::memory_order_relaxed);
		}

		/**
		* [EN]
		* Wakes at most one waiting thread, if any are currently waiting
		* (bumps the epoch first so the wakeup can't be missed).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在待機中のスレッドがあれば、最大1つを起床させる
		* （起床を見逃さないよう、先にエポックを進める）。
		*/
		void notify_one()noexcept
		{
			std::atomic_thread_fence(std::memory_order_seq_cst);
			for (Uint64 state = state_.load(std::memory_order_relaxed);state & WAITER_MASK;)
			{
				if (state_.compare_exchange_weak(state, state + EPOCH_INC, std::memory_order_relaxed))
				{
					state_.notify_one();
					break;
				}
			}
		}

		/**
		* [EN]
		* Wakes every currently waiting thread (bumps the epoch first so
		* the wakeup can't be missed).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在待機中の全スレッドを起床させる（起床を見逃さないよう、
		* 先にエポックを進める）。
		*/
		void notify_all()noexcept
		{
			std::atomic_thread_fence(std::memory_order_seq_cst);
			for (Uint64 state = state_.load(std::memory_order_relaxed);state & WAITER_MASK;)
			{
				if (state_.compare_exchange_weak(state, state + EPOCH_INC, std::memory_order_relaxed))
				{
					state_.notify_all();
					break;
				}
			}
		}

		/**
		* [EN]
		* Wakes up to n waiting threads (falls back to notify_all if n
		* covers every configured slot).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最大 n 個の待機中スレッドを起床させる（n が設定済みの全スロットを
		* 覆う場合は notify_all にフォールバックする）。
		*/
		void notify_count(Size n)noexcept
		{
			if (n >= waiters_.size())
			{
				notify_all();
			}
			else
			{
				for (Size waiterIndex = 0;waiterIndex < n;++waiterIndex)
				{
					notify_one();
				}
			}
		}

		/**
		* [EN]
		* Returns the total number of configured waiter slots.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されている待機者スロットの総数を返す。
		*/
		Size size()const noexcept
		{
			return waiters_.size();
		}

	private:
		/// [EN] Sanity checks for the bit-packing scheme used by state_ below.
		/// [JP] 以下の state_ で使うビット詰め込み方式に対する健全性チェック。
		SC_STATIC_ASSERT(sizeof(Int) == 4, "Int型は4バイトでなければなりません");
		SC_STATIC_ASSERT(sizeof(Uint32) == 4, "Uint32型は4バイトでなければなりません");
		SC_STATIC_ASSERT(sizeof(Uint64) == 8, "Uint64型は8バイトでなければなりません");
		SC_STATIC_ASSERT(sizeof(std::atomic<Uint64>) == 8, "std::atomic<Uint64>型は8バイトでなければなりません");

		/// [EN] Packed state: epoch in the upper 32 bits (WAITER_MASK's
		///      complement), waiter count in the lower 32 bits (WAITER_MASK).
		/// [JP] 詰め込まれた状態: 上位32ビット（WAITER_MASKの補数）がエポック、
		///      下位32ビット（WAITER_MASK）が待機者数。
		std::atomic<Uint64> state_;

		/// [EN] Per-slot epoch snapshots, indexed by the waiter index passed to prepare_wait/commit_wait.
		/// [JP] スロットごとのエポックスナップショット。prepare_wait/commit_wait に渡す待機者インデックスでアクセスする。
		DynamicArray<Waiter> waiters_;

		/// [EN] Bit position where the epoch field begins within state_.
		/// [JP] state_ 内でエポックフィールドが始まるビット位置。
		static constexpr Uint64 EPOCH_SHIFT = 32;

		/// [EN] Amount to add to state_ to advance the epoch by one.
		/// [JP] エポックを1進めるために state_ に加算する量。
		static constexpr Uint64 EPOCH_INC = Uint64(1) << EPOCH_SHIFT;

		/// [EN] Amount to add to/subtract from state_ to change the waiter count by one.
		/// [JP] 待機者数を1増減させるために state_ に加減算する量。
		static constexpr Uint64 WAITER_INC = 1;

		/// [EN] Mask isolating the waiter-count field (also the max representable waiter count).
		/// [JP] 待機者数フィールドを取り出すマスク（表現可能な最大待機者数でもある）。
		static constexpr Uint64 WAITER_MASK = EPOCH_INC - 1;
	};
}