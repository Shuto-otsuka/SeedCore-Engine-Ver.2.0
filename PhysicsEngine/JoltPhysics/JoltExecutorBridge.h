#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class JobExecutor;

	class JoltExecutorBridge final :public JPH::JobSystemWithBarrier
	{
	public:
		explicit JoltExecutorBridge(JobExecutor& executor, JPH::uint inMaxBarriers = 8);

		~JoltExecutorBridge()override = default;

		Int GetMaxConcurrency()const override;

		JobHandle CreateJob(const Char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0)override;

	protected:
		virtual void QueueJob(Job* inJob)override;

		virtual void QueueJobs(Job** inJobs, JPH::uint inNumJobs)override;

		virtual void FreeJob(Job* inJob)override;

	private:
		JobExecutor& executor_;
	};
}

