#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/JoltPhysics/JoltExecutorBridge.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class JobExecutor;

	class SEEDCORE_API JoltManager
	{
	public:
		JoltManager() = default;
		~JoltManager() = default;

		Bool Initialize(JobExecutor& executor);

		void Execute(Float elapsedTime);

		void Finalize();

		JoltShapePool& GetShapePool();

		JPH::BodyInterface& GetBodyInterface();

		JPH::PhysicsSystem& GetPhysicsSystem();

	private:
		JoltShapePool shapePool_;

		ResourcePtr<JoltExecutorBridge> executor_;

		ResourcePtr<JPH::TempAllocatorImpl> tempAllocator_;

		JPH::PhysicsSystem physicsSystem_;
	};
}