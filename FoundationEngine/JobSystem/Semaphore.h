#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* A counting semaphore used to limit concurrent access to a resource
	* within the job system. Jobs (represented by JobNode) that cannot
	* immediately acquire the semaphore are queued as waiters and are
	* released later when the semaphore's value allows it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブシステム内でリソースへの同時アクセスを制限するための、
	* カウンティングセマフォ。即座にセマフォを獲得できないジョブ
	* （JobNode で表される）は待機者として登録され、セマフォの値が
	* 許す状態になった時点で後から解放される。
	*/
	class Semaphore
	{
	private:
		friend class JobNode;

	public:
		/**
		* [EN]
		* Default constructor: constructs a semaphore with a maximum
		* (and initial available) value of zero.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 最大値（および初期の利用可能値）が
		* 0 のセマフォを構築する。
		*/
		Semaphore() = default;

		/**
		* [EN]
		* Constructs the semaphore with the given maximum (and initial
		* available) value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された最大値（および初期の利用可能値）でセマフォを構築する。
		*/
		explicit Semaphore(Size maxValue);

		/**
		* [EN]
		* Returns the current available value of the semaphore.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* セマフォの現在の利用可能な値を返す。
		*/
		Size value()const;

		/**
		* [EN]
		* Returns the maximum value the semaphore was configured with.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* セマフォに設定されている最大値を返す。
		*/
		Size max_value()const;

		/**
		* [EN]
		* Resets the semaphore's current value back to its existing
		* maximum value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* セマフォの現在値を、既存の最大値にリセットする。
		*/
		void reset();

		/**
		* [EN]
		* Resets the semaphore with a new maximum value, replacing the
		* previous maximum and current value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新しい最大値でセマフォをリセットし、以前の最大値と現在値を
		* 置き換える。
		*/
		void reset(Size newMaxValue);

	private:
		/// [EN] Protects all mutable state of the semaphore (maxValue_, currentValue_, waiters_) from concurrent access.
		/// [JP] セマフォの可変な状態（maxValue_, currentValue_, waiters_）を並行アクセスから保護する。
		mutable std::mutex mutex_;

		/// [EN] The configured maximum value of the semaphore.
		/// [JP] セマフォに設定された最大値。
		Size maxValue_ = 0;

		/// [EN] The current available value of the semaphore; decremented on acquire and incremented on release.
		/// [JP] セマフォの現在の利用可能値。acquire で減少し、release で増加する。
		Size currentValue_ = 0;

		/// [EN] Jobs that failed to immediately acquire the semaphore and are waiting to be released.
		/// [JP] セマフォを即座に獲得できず、解放されるのを待っているジョブの一覧。
		HybridArray<JobNode*> waiters_;

		/**
		* [EN]
		* Attempts to immediately acquire the semaphore for node.
		* If acquisition is not currently possible, registers node
		* as a waiter instead and returns accordingly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node に対してセマフォを即座に獲得しようと試みる。現時点で
		* 獲得できない場合は、代わりに node を待機者として登録し、
		* それに応じた結果を返す。
		*/
		Bool try_acquire_or_wait(JobNode* node);

		/**
		* [EN]
		* Releases the semaphore and collects any waiting jobs that can
		* now proceed into nodes, so the caller can schedule them.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* セマフォを解放し、これにより実行可能になった待機中のジョブを
		* nodes へ収集する。呼び出し側はこれらをスケジューリングできる。
		*/
		void release(HybridArray<JobNode*>& nodes);
	};
}