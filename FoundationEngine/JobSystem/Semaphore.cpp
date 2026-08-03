#include <FoundationEngine/JobSystem/Semaphore.h>
#include <FoundationEngine/Log/Exeption.h>

namespace SeedCore
{
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
	Semaphore::Semaphore(Size maxValue) :maxValue_(maxValue), currentValue_(maxValue)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns the current available value of the semaphore.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* セマフォの現在の利用可能な値を返す。
	*/
	Size Semaphore::value()const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return currentValue_;
	}

	/**
	* [EN]
	* Returns the maximum value the semaphore was configured with.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* セマフォに設定されている最大値を返す。
	*/
	Size Semaphore::max_value()const
	{
		return maxValue_;
	}

	/**
	* [EN]
	* Resets the semaphore's current value back to its existing maximum
	* value.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* セマフォの現在値を、既存の最大値にリセットする。
	*/
	void Semaphore::reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		currentValue_ = maxValue_;
		waiters_.clear();
	}

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
	void Semaphore::reset(Size newMaxValue)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		currentValue_ = (maxValue_ = newMaxValue);
		waiters_.clear();
	}

	/**
	* [EN]
	* Attempts to immediately acquire the semaphore for node. If
	* acquisition is not currently possible, registers node as a waiter
	* instead and returns accordingly.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node に対してセマフォを即座に獲得しようと試みる。現時点で
	* 獲得できない場合は、代わりに node を待機者として登録し、
	* それに応じた結果を返す。
	*/
	Bool Semaphore::try_acquire_or_wait(JobNode* node)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (currentValue_ > 0)
		{
			/// [EN] A slot is currently available: consume it and let the caller proceed immediately.
			/// [JP] 現在空きスロットがあるため、それを消費し、呼び出し元に即座に進行させる。
			--currentValue_;
			return true;
		}
		else
		{
			/// [EN] No slot is available: register node as a waiter to be released later, and tell the caller to wait.
			/// [JP] 空きスロットが無いため、node を後で解放される待機者として登録し、呼び出し元には待機するよう伝える。
			waiters_.push_back(node);
			return false;
		}
	}

	/**
	* [EN]
	* Releases the semaphore and collects any waiting jobs that can now
	* proceed into nodes, so the caller can schedule them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* セマフォを解放し、これにより実行可能になった待機中のジョブを
	* nodes へ収集する。呼び出し側はこれらをスケジューリングできる。
	*/
	void Semaphore::release(HybridArray<JobNode*>& nodes)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (currentValue_ >= maxValue_)
		{
			/// [EN] Releasing beyond the configured maximum indicates a logic error (an acquire/release mismatch), so abort with an exception.
			/// [JP] 設定された最大値を超えて release しようとした場合、acquire/release の不整合というロジックエラーを示すため、例外を投げて中断する。
			SC_THROW("Semaphore::release() が acquire 済みの回数を超えて呼び出されました。");
		}

		++currentValue_;

		if (nodes.empty())
		{
			/// [EN] nodes is empty: avoid copying by directly swapping in the waiter list.
			/// [JP] nodes が空の場合、コピーを避けるために待機者リストを直接スワップする。
			nodes.swap(waiters_);
		}
		else
		{
			/// [EN] nodes already has elements: append all current waiters and clear the waiter list.
			/// [JP] nodes に既に要素がある場合、現在の待機者を全て追加し、待機者リストをクリアする。
			nodes.reserve(nodes.size() + waiters_.size());
			nodes.insert(nodes.end(), waiters_.begin(), waiters_.end());
			waiters_.clear();
		}
	}
}
