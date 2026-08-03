#pragma once
#include <FoundationEngine/JobSystem/JobTask.h>

namespace SeedCore
{
	/**
	* [EN]
	* Handle passed to a PreemptiveRuntime task's callable, letting it
	* interact with the scheduler while running: schedule extra tasks,
	* yield (Corun/CorunAll) to help process other work while suspended,
	* and check whether the run has been cancelled.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* PreemptiveRuntime タスクの呼び出し可能オブジェクトへ渡される
	* ハンドル。実行中にスケジューラと連携できるようにする: 追加タスクの
	* スケジューリング、中断中に他の処理を手伝うための yield
	* （Corun/CorunAll）、実行がキャンセルされたかどうかの確認。
	*/
	class JobPreemptiveRuntime
	{
	private:
		friend class JobExecutor;
		friend class FlowBuilder;

	public:
		/**
		* [EN]
		* Returns a reference to the executor running this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクを実行しているエグゼキュータへの参照を返す。
		*/
		JobExecutor& Executor();

		/**
		* [EN]
		* Returns a reference to the worker thread currently executing this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクを現在実行しているワーカースレッドへの参照を返す。
		*/
		JobWorker& Worker();

		/**
		* [EN]
		* Schedules task for execution alongside this one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* task をこのタスクと並行して実行されるようスケジューリングする。
		*/
		void Schedule(JobTask task);

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
		void Corun();

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
		void CorunAll();

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
		Bool IsCancelled();

	private:
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
		explicit JobPreemptiveRuntime(JobExecutor& executor, JobWorker& worker, JobNode* node);

		/// [EN] The executor running this task.
		/// [JP] このタスクを実行しているエグゼキュータ。
		JobExecutor& executor_;

		/// [EN] The worker thread currently executing this task.
		/// [JP] このタスクを現在実行しているワーカースレッド。
		JobWorker& worker_;

		/// [EN] The node this runtime handle is bound to.
		/// [JP] このランタイムハンドルが紐づいているノード。
		JobNode* node_;
	};

	/**
	* [EN]
	* Handle passed to a NonpreemptiveRuntime task's callable, letting it
	* schedule extra tasks without the ability to suspend/yield (unlike
	* JobPreemptiveRuntime, a non-preemptive task always runs to completion).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* NonpreemptiveRuntime タスクの呼び出し可能オブジェクトへ渡される
	* ハンドル。追加タスクのスケジューリングは可能だが、中断/yield は
	* できない（JobPreemptiveRuntime と異なり、非プリエンプティブな
	* タスクは常に完了まで実行される）。
	*/
	class JobNonpreemptiveRuntime
	{
	private:
		friend class JobExecutor;

	public:
		/**
		* [EN]
		* Schedules task for execution alongside this one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* task をこのタスクと並行して実行されるようスケジューリングする。
		*/
		void Schedule(JobTask task);

	private:
		/**
		* [EN]
		* Constructs a runtime handle running under executor/worker.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* executor/worker のもとで実行されるランタイムハンドルを構築する。
		*/
		explicit JobNonpreemptiveRuntime(JobExecutor& executor, JobWorker& worker);

		/// [EN] The executor running this task.
		/// [JP] このタスクを実行しているエグゼキュータ。
		JobExecutor& executor_;

		/// [EN] The worker thread currently executing this task.
		/// [JP] このタスクを現在実行しているワーカースレッド。
		JobWorker& worker_;
	};
}
