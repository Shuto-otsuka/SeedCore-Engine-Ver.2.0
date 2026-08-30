#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/JoltPhysics/JoltExecutorBridge.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>
#include <PhysicsEngine/JoltPhysics/JoltContactListener.h>

namespace SeedCore
{
	class JobExecutor;
	class World;

	class SEEDCORE_API JoltManager
	{
	public:
		JoltManager() = default;
		~JoltManager() = default;

		Bool Initialize(JobExecutor& executor);

		void Execute(Float elapsedTime);

		void Finalize();

		JoltShapePool& GetShapePool();

		JoltConstraintPool& GetConstraintPool();

		JPH::BodyInterface& GetBodyInterface();

		JPH::PhysicsSystem& GetPhysicsSystem();

		JPH::TempAllocator& GetPhysicsAllocator();

		void SetActiveWorld(World* world);

		World* GetActiveWorld()const;

	private:
		JoltShapePool shapePool_;

		JoltConstraintPool constraintPool_;

		ResourcePtr<JoltExecutorBridge> executor_;

		ResourcePtr<JPH::TempAllocatorImpl> tempAllocator_;

		JPH::PhysicsSystem physicsSystem_;

		JoltContactListener contactListener_;
	};
}