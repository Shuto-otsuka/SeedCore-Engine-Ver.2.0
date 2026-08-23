#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>

namespace SeedCore
{
	Bool JoltManager::Initialize(JobExecutor& executor)
	{
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		constexpr JPH::uint tempAllocatorSize = 10 * 1024 * 1024;
		tempAllocator_ = MakePtr<JPH::TempAllocatorImpl>(tempAllocatorSize);

		executor_ = MakePtr<JoltExecutorBridge>(executor);

		static BPLayerInterfaceImplementation bpLayerInterface;
		static ObjVsBPFilterImplementation objVsBPFilter;
		static ObjLayerPairFilterImplementation objLayerPairFilter;

		physicsSystem_.Init(1024, 0, 65536, 1024, bpLayerInterface, objVsBPFilter, objLayerPairFilter);
		physicsSystem_.SetContactListener(&contactListener_);

		return true;
	}

	void JoltManager::Execute(Float elapsedTime)
	{
		physicsSystem_.Update(elapsedTime, 1, tempAllocator_.get(), executor_.get());
		contactListener_.DispatchPendingEvents();
	}

	void JoltManager::Finalize()
	{
		/// [EN] Shapes must be released before UnregisterTypes.
		/// [JP] Shapeの解放はUnregisterTypesより前に行う必要がある。
		shapePool_.Clear();

		if (executor_)
		{
			executor_.reset();
			executor_ = nullptr;
		}

		if (tempAllocator_)
		{
			tempAllocator_.reset();
			tempAllocator_ = nullptr;
		}

		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	JoltShapePool& JoltManager::GetShapePool()
	{
		return shapePool_;
	}

	JPH::BodyInterface& JoltManager::GetBodyInterface()
	{
		return physicsSystem_.GetBodyInterface();
	}

	JPH::PhysicsSystem& JoltManager::GetPhysicsSystem()
	{
		return physicsSystem_;
	}

	JPH::TempAllocator& JoltManager::GetPhysicsAllocator()
	{
		return *tempAllocator_;
	}

	void JoltManager::SetActiveWorld(World* world)
	{
		contactListener_.SetActiveWorld(world);
	}

	World* JoltManager::GetActiveWorld()const
	{
		return contactListener_.GetActiveWorld();
	}
}