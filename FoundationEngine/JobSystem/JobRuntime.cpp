#include <FoundationEngine/JobSystem/JobRuntime.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a runtime handle bound to node, running under executor/worker.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* executor/worker のもとで実行される、node に紐づいたランタイム
	* ハンドルを構築する。
	*/
	JobPreemptiveRuntime::JobPreemptiveRuntime(JobExecutor& executor, JobWorker& worker, JobNode* node) : executor_(executor), worker_(worker), node_(node)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns a reference to the executor running this task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクを実行しているエグゼキュータへの参照を返す。
	*/
	JobExecutor& JobPreemptiveRuntime::Executor()
	{
		return executor_;
	}

	/**
	* [EN]
	* Returns a reference to the worker thread currently executing this task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクを現在実行しているワーカースレッドへの参照を返す。
	*/
	inline JobWorker& JobPreemptiveRuntime::Worker()
	{
		return worker_;
	}

	/**
	* [EN]
	* Schedules task for execution alongside this one.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* task をこのタスクと並行して実行されるようスケジューリングする。
	*/
	void JobPreemptiveRuntime::Schedule(JobTask task)
	{
		auto node = task.node_;

		/// [EN] Reset the join counter so the task starts with no outstanding dependencies.
		/// [JP] join カウンタをリセットし、タスクが未完了の依存関係を持たない状態から開始するようにする。
		node->joinCounter_.store(0, std::memory_order_relaxed);

		/// [EN] Register the new task as an outstanding dependent of its parent node (or the owning topology, if it has no parent), so completion is tracked correctly.
		/// [JP] 新しいタスクを、その親ノード（親が無ければ所有元のトポロジー）の未完了の依存先として登録し、完了が正しく追跡されるようにする。
		auto& join = node->parent_ ? node->parent_->joinCounter_ : node->topology_->joinCounter_;
		join.fetch_add(1, std::memory_order_relaxed);
		executor_.Schedule(worker_, node);
	}

	/**
	* [EN]
	* Suspends this task and helps process other scheduled work until
	* this task's own outstanding dependents complete.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクを中断し、このタスク自身の未完了の依存先が完了する
	* まで、他のスケジュール済み処理の実行を手伝う。
	*/
	void JobPreemptiveRuntime::Corun()
	{
		executor_.CorunUntil(worker_, [this]()->Bool {return node_->joinCounter_.load(std::memory_order_acquire) == 1;});
	}

	/**
	* [EN]
	* Suspends this task and helps process other scheduled work until
	* every outstanding topology on the executor completes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクを中断し、エグゼキュータ上のすべての未完了トポロジーが
	* 完了するまで、他のスケジュール済み処理の実行を手伝う。
	*/
	void JobPreemptiveRuntime::CorunAll()
	{
		Corun();
	}

	/**
	* [EN]
	* Returns whether this task's run has been flagged as cancelled.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクの実行がキャンセル済みとしてフラグ付けされているか
	* どうかを返す。
	*/
	Bool JobPreemptiveRuntime::IsCancelled()
	{
		return true;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a runtime handle running under executor/worker.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* executor/worker のもとで実行されるランタイムハンドルを構築する。
	*/
	JobNonpreemptiveRuntime::JobNonpreemptiveRuntime(JobExecutor& executor, JobWorker& worker) :executor_(executor), worker_(worker)
	{
		/// No Code
	}

	/**
	* [EN]
	* Schedules task for execution alongside this one.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* task をこのタスクと並行して実行されるようスケジューリングする。
	*/
	void JobNonpreemptiveRuntime::Schedule(JobTask task)
	{
		auto node = task.node_;

		/// [EN] Reset the join counter so the task starts with no outstanding dependencies.
		/// [JP] join カウンタをリセットし、タスクが未完了の依存関係を持たない状態から開始するようにする。
		node->joinCounter_.store(0, std::memory_order_relaxed);

		/// [EN] Register the new task as an outstanding dependent of its parent node (or the owning topology, if it has no parent), so completion is tracked correctly.
		/// [JP] 新しいタスクを、その親ノード（親が無ければ所有元のトポロジー）の未完了の依存先として登録し、完了が正しく追跡されるようにする。
		auto& join = node->parent_ ? node->parent_->joinCounter_ : node->topology_->joinCounter_;
		join.fetch_add(1, std::memory_order_relaxed);
		executor_.Schedule(worker_, node);
	}
}
