#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class JobExecutor;

	/**
	* [EN]
	* Adapts Jolt's job-system interface to SeedCore's JobExecutor and submits
	* Jolt jobs as JobTaskflow work.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt のジョブシステムインターフェースを SeedCore の JobExecutor へ適合させ、
	* Jolt ジョブを JobTaskflow の処理として投入する。
	*/
	class JoltExecutorBridge final :public JPH::JobSystemWithBarrier
	{
	public:
		/**
		* [EN]
		* Creates a bridge backed by the supplied executor and barrier capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定されたエグゼキューターとバリア容量を使うブリッジを生成する。
		*/
		explicit JoltExecutorBridge(JobExecutor& executor, JPH::uint inMaxBarriers = 8);

		/**
		* [EN]
		* Destroys the bridge after its submitted jobs have released their references.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 投入済みジョブが参照を解放した後にブリッジを破棄する。
		*/
		~JoltExecutorBridge()override = default;

		/**
		* [EN]
		* Returns the hardware concurrency exposed to Jolt's job scheduler.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt のジョブスケジューラーへ公開するハードウェア並行数を返す。
		*/
		Int GetMaxConcurrency()const override;

		/**
		* [EN]
		* Allocates a Jolt job and queues it immediately when it has no dependencies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt ジョブを確保し、依存関係がなければ即座にキューへ投入する。
		*/
		JobHandle CreateJob(const Char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0)override;

	protected:
		/**
		* [EN]
		* Submits one Jolt job to the SeedCore executor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つの Jolt ジョブを SeedCore のエグゼキューターへ投入する。
		*/
		virtual void QueueJob(Job* inJob)override;

		/**
		* [EN]
		* Submits multiple Jolt jobs in one taskflow.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 複数の Jolt ジョブを1つのタスクフローへ投入する。
		*/
		virtual void QueueJobs(Job** inJobs, JPH::uint inNumJobs)override;

		/**
		* [EN]
		* Deletes a Jolt job whose reference count reached zero.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 参照カウントがゼロになった Jolt ジョブを削除する。
		*/
		virtual void FreeJob(Job* inJob)override;

	private:
		/// [EN] SeedCore executor that runs submitted Jolt work.
		/// [JP] 投入された Jolt 処理を実行する SeedCore エグゼキューター。
		JobExecutor& executor_;
	};
}

