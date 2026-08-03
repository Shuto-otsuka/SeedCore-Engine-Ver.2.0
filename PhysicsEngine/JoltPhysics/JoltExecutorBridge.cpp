#include <PhysicsEngine/JoltPhysics/JoltExecutorBridge.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	JoltExecutorBridge::JoltExecutorBridge(JobExecutor& executor, JPH::uint inMaxBarriers) :JPH::JobSystemWithBarrier(inMaxBarriers), executor_(executor)
	{
		/// No Code
	}

	Int JoltExecutorBridge::GetMaxConcurrency()const
	{
		return static_cast<Int>(std::thread::hardware_concurrency());
	}

	JoltExecutorBridge::JobHandle JoltExecutorBridge::CreateJob(const Char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies)
	{
		Job* job = new Job(inName, inColor, this, inJobFunction, inNumDependencies);
		JobHandle handle(job);

		if (inNumDependencies == 0)
		{
			QueueJob(job);
		}

		return handle;
	}

	void JoltExecutorBridge::QueueJob(Job* inJob)
	{
		inJob->AddRef();

		ResourceRef<JobTaskflow> flow = MakeRef<JobTaskflow>();
		flow->emplace([inJob]() {
			inJob->Execute();
			inJob->Release();
			});

		executor_.Run(*flow, [flow]() { /// No Code
			});
	}

	void JoltExecutorBridge::QueueJobs(Job** inJobs, JPH::uint inNumJobs)
	{
		ResourceRef<JobTaskflow> flow = MakeRef<JobTaskflow>();

		for (JPH::uint index = 0; index < inNumJobs; ++index)
		{
			Job* inJob = inJobs[index];
			inJob->AddRef();
			flow->emplace([inJob]() {
				inJob->Execute();
				inJob->Release();
				});
		}

		executor_.Run(*flow, [flow]() { /// No Code
			});
	}

	void JoltExecutorBridge::FreeJob(Job* inJob)
	{
		delete inJob;
	}
}