#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Initializes Jolt and connects it to the supplied job executor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt を初期化し、指定されたジョブエグゼキューターへ接続する。
	*/
	Bool JoltManager::Initialize(JobExecutor& executor)
	{
		/// [EN] Register Jolt's allocator, factory and runtime types before creating physics resources.
		/// [JP] 物理リソースの生成前に、Jolt のアロケーター、ファクトリー、実行時型を登録する。
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

	/**
	* [EN]
	* Advances the physics simulation and dispatches queued contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理シミュレーションを進め、キュー内の接触イベントを通知する。
	*/
	void JoltManager::Execute(Float elapsedTime)
	{
		physicsSystem_.Update(elapsedTime, 1, tempAllocator_.get(), executor_.get());
		contactListener_.DispatchEvent();
	}

	/**
	* [EN]
	* Releases physics resources and unregisters Jolt types.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理リソースを解放し、Jolt の型登録を解除する。
	*/
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

	/**
	* [EN]
	* Returns the shape pool owned by this manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このマネージャーが所有する形状プールを返す。
	*/
	JoltShapePool& JoltManager::ShapePool()
	{
		return shapePool_;
	}

	/**
	* [EN]
	* Returns the constraint pool owned by this manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このマネージャーが所有する拘束プールを返す。
	*/
	JoltConstraintPool& JoltManager::ConstraintPool()
	{
		return constraintPool_;
	}

	/**
	* [EN]
	* Returns Jolt's body interface.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt のボディインターフェースを返す。
	*/
	JPH::BodyInterface& JoltManager::BodyInterface()
	{
		return physicsSystem_.GetBodyInterface();
	}

	/**
	* [EN]
	* Returns the owned Jolt physics system.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 所有する Jolt 物理システムを返す。
	*/
	JPH::PhysicsSystem& JoltManager::PhysicsSystem()
	{
		return physicsSystem_;
	}

	/**
	* [EN]
	* Returns the temporary allocator used during physics updates.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理更新中に使う一時アロケーターを返す。
	*/
	JPH::TempAllocator& JoltManager::PhysicsAllocator()
	{
		return *tempAllocator_;
	}

	/**
	* [EN]
	* Sets the World that receives contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 接触イベントを受け取る World を設定する。
	*/
	void JoltManager::ActiveWorld(World* world)
	{
		contactListener_.ActiveWorld(world);
	}

	/**
	* [EN]
	* Returns the World that receives contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 接触イベントを受け取る World を返す。
	*/
	World* JoltManager::ActiveWorld()const
	{
		return contactListener_.ActiveWorld();
	}
}
