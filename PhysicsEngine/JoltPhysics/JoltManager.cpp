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

		physicsSystem_.SetSimCollideBodyVsBody([](const JPH::Body& body1, const JPH::Body& body2, JPH::Mat44Arg centerOfMassTransform1, JPH::Mat44Arg centerOfMassTransform2, JPH::CollideShapeSettings& collideShapeSettings, JPH::CollideShapeCollector& collector, const JPH::ShapeFilter& shapeFilter)
		{
			collideShapeSettings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
			JPH::PhysicsSystem::sDefaultSimCollideBodyVsBody(body1, body2, centerOfMassTransform1, centerOfMassTransform2, collideShapeSettings, collector, shapeFilter);
		});

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
		constraintPool_.Clear(physicsSystem_);
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

	JoltConstraintPool& JoltManager::GetConstraintPool()
	{
		return constraintPool_;
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