#include <FoundationEngine/JobSystem/JobWorker.h>
#include <FoundationEngine/JobSystem/JobNode.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns this worker's index within the owning executor's pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 所有元のエグゼキュータのプール内における、このワーカーの
	* インデックスを返す。
	*/
	inline Size JobWorker::ID()const
	{
		return id_;
	}

	/**
	* [EN]
	* Returns the current number of nodes queued on this worker.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このワーカーに現在キューイングされているノードの数を返す。
	*/
	inline Size JobWorker::QueueSize()const
	{
		return wsq_.size();
	}

	/**
	* [EN]
	* Returns this worker's queue capacity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このワーカーのキュー容量を返す。
	*/
	inline Size JobWorker::QueueCapacity()const
	{
		return static_cast<Size>(wsq_.capacity());
	}

	/**
	* [EN]
	* Returns a reference to the underlying OS thread.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部の OS スレッドへの参照を返す。
	*/
	std::thread& JobWorker::Thread()
	{
		return thread_;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a view referring to worker directly.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* worker を直接参照するビューを構築する。
	*/
	JobWorkerView::JobWorkerView(const JobWorker& worker) :worker_(worker)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns the underlying worker's index within its executor's pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部のワーカーの、所属エグゼキュータのプール内でのインデックスを
	* 返す。
	*/
	Size JobWorkerView::ID()const
	{
		return worker_.id_;
	}

	/**
	* [EN]
	* Returns the underlying worker's current queue size.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部のワーカーの、現在のキューサイズを返す。
	*/
	Size JobWorkerView::QueueSize()const
	{
		return worker_.wsq_.size();
	}

	/**
	* [EN]
	* Returns the underlying worker's queue capacity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部のワーカーのキュー容量を返す。
	*/
	Size JobWorkerView::QueueCapacity()const
	{
		return static_cast<Size>(worker_.wsq_.capacity());
	}
}
