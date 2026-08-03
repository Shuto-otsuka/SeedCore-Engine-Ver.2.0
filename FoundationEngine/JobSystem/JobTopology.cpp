#include <FoundationEngine/JobSystem/JobTopology.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns whether this run has been flagged as cancelled.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この実行がキャンセル済みとしてフラグ付けされているかどうかを返す。
	*/
	Bool JobTopology::Cancelled()const
	{
		return estate_.load(std::memory_order_relaxed) & (JobExceptionState::CANCELLED | JobExceptionState::EXCEPTION);
	}

	/**
	* [EN]
	* Fulfills promise_, unblocking any JobFuture<void> observers waiting
	* on this run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* promise_ を満たし、この実行を待機している JobFuture<void>
	* オブザーバーのブロックを解除する。
	*/
	void JobTopology::CarryOutPromise()
	{
		if (exceptionPtr_)
		{
			/// [EN] An exception propagated up to this run: clear it first, then hand it to observers via the promise.
			/// [JP] この実行まで例外が伝播している: まずクリアしてから、promise 経由でオブザーバーへ渡す。
			auto exception = exceptionPtr_;
			exceptionPtr_ = nullptr;
			promise_.set_exception(exception);
		}
		else
		{
			/// [EN] No exception: report normal completion.
			/// [JP] 例外なし: 通常完了を報告する。
			promise_.set_value();
		}
	}
}
