#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobNodeBase.h>

namespace SeedCore
{
	/**
	* [EN]
	* Represents a single submitted run of a JobTaskflow's graph. Holds
	* the completion predicate (checked after each pass to decide
	* whether to loop/repeat the graph), the on-finish callback, and the
	* promise that JobFuture<void> observers wait on.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobTaskflow のグラフに対する、投入済みの単一の実行を表すクラス。
	* 完了述語（各パス後にチェックされ、グラフをループ/再実行するかを
	* 判断する）、完了時コールバック、および JobFuture<void>
	* オブザーバーが待機する promise を保持する。
	*/
	class JobTopology :public JobNodeBase
	{
	private:
		friend class JobExecutor;
		friend class Subflow;
		friend class JobPreemptiveRuntime;
		friend class JobNonpreemptiveRuntime;
		friend class JobNode;

		template<typename T>
		friend class JobFuture;

	public:
		/**
		* [EN]
		* Constructs a topology bound to taskflow, storing predicate
		* (re-run condition) and onFinish (completion callback).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskflow に紐づいたトポロジーを構築し、predicate（再実行条件）と
		* onFinish（完了時コールバック）を保持する。
		*/
		template<typename Predicate, typename OnFinish>
		JobTopology(JobTaskflow& taskflow, Predicate&& predicate, OnFinish&& onFinish) :JobNodeBase(JobNodeState::NONE, JobExceptionState::NONE, nullptr, 0), taskflow_(taskflow), predicate_(std::forward<Predicate>(predicate)), onFinish_(std::forward<OnFinish>(onFinish))
		{
			/// No Code
		}

		/**
		* [EN]
		* Returns whether this run has been flagged as cancelled.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この実行がキャンセル済みとしてフラグ付けされているかどうかを返す。
		*/
		Bool Cancelled()const;

	private:
		/// [EN] The taskflow whose graph this topology is a run of.
		/// [JP] このトポロジーがその実行インスタンスとなっている、対象のタスクフロー。
		JobTaskflow& taskflow_;

		/// [EN] Fulfilled when this run finishes, allowing JobFuture<void> observers to unblock.
		/// [JP] この実行が完了した時点で満たされ、JobFuture<void> オブザーバーのブロックを解除する。
		std::promise<void> promise_;

		/// [EN] Evaluated after each pass over the graph; returning true stops the run, false re-runs it.
		/// [JP] グラフの各パス後に評価される。true を返すと実行を停止し、false であれば再実行する。
		std::function<Bool()> predicate_;

		/// [EN] Invoked once this run has fully finished (predicate_ returned true or the run was cancelled).
		/// [JP] この実行が完全に終了した時点（predicate_ が true を返した、またはキャンセルされた場合）で呼び出される。
		std::function<void()> onFinish_;

		/**
		* [EN]
		* Fulfills promise_, unblocking any JobFuture<void> observers
		* waiting on this run.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* promise_ を満たし、この実行を待機している JobFuture<void>
		* オブザーバーのブロックを解除する。
		*/
		void CarryOutPromise();
	};
}
