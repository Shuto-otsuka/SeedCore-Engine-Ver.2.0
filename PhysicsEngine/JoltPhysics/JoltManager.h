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

	/**
	* [EN]
	* Owns the Jolt physics system, its allocators, job bridge, contact listener
	* and pooled shapes and constraints.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt 物理システム、アロケーター、ジョブブリッジ、接触リスナー、
	* 形状・拘束プールを所有する。
	*/
	class SEEDCORE_API JoltManager
	{
	public:
		/**
		* [EN]
		* Creates an uninitialized Jolt manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 未初期化の Jolt マネージャーを生成する。
		*/
		JoltManager() = default;

		/**
		* [EN]
		* Destroys the manager after explicit finalization.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 明示的な終了処理後にマネージャーを破棄する。
		*/
		~JoltManager() = default;

		/**
		* [EN]
		* Initializes Jolt and connects it to the supplied job executor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt を初期化し、指定されたジョブエグゼキューターへ接続する。
		*/
		Bool Initialize(JobExecutor& executor);

		/**
		* [EN]
		* Advances the physics simulation and dispatches queued contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理シミュレーションを進め、キュー内の接触イベントを通知する。
		*/
		void Execute(Float elapsedTime);

		/**
		* [EN]
		* Releases physics resources and unregisters Jolt types.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理リソースを解放し、Jolt の型登録を解除する。
		*/
		void Finalize();

		/**
		* [EN]
		* Returns the shape pool owned by this manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このマネージャーが所有する形状プールを返す。
		*/
		JoltShapePool& ShapePool();

		/**
		* [EN]
		* Returns the constraint pool owned by this manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このマネージャーが所有する拘束プールを返す。
		*/
		JoltConstraintPool& ConstraintPool();

		/**
		* [EN]
		* Returns Jolt's body interface.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt のボディインターフェースを返す。
		*/
		JPH::BodyInterface& BodyInterface();

		/**
		* [EN]
		* Returns the owned Jolt physics system.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 所有する Jolt 物理システムを返す。
		*/
		JPH::PhysicsSystem& PhysicsSystem();

		/**
		* [EN]
		* Returns the temporary allocator used during physics updates.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理更新中に使う一時アロケーターを返す。
		*/
		JPH::TempAllocator& PhysicsAllocator();

		/**
		* [EN]
		* Sets the World that receives contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接触イベントを受け取る World を設定する。
		*/
		void ActiveWorld(World* world);

		/**
		* [EN]
		* Returns the World that receives contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接触イベントを受け取る World を返す。
		*/
		World* ActiveWorld()const;

	private:
		/// [EN] Pool that owns reusable Jolt collision shapes.
		/// [JP] 再利用可能な Jolt 衝突形状を所有するプール。
		JoltShapePool shapePool_;

		/// [EN] Pool that owns registered Jolt constraints.
		/// [JP] 登録済み Jolt 拘束を所有するプール。
		JoltConstraintPool constraintPool_;

		/// [EN] Bridge that submits Jolt jobs to SeedCore's executor.
		/// [JP] Jolt ジョブを SeedCore のエグゼキューターへ投入するブリッジ。
		ResourcePtr<JoltExecutorBridge> executor_;

		/// [EN] Temporary memory allocator used by Jolt updates.
		/// [JP] Jolt の更新で使う一時メモリアロケーター。
		ResourcePtr<JPH::TempAllocatorImpl> tempAllocator_;

		/// [EN] Jolt simulation world owned by this manager.
		/// [JP] このマネージャーが所有する Jolt シミュレーションワールド。
		JPH::PhysicsSystem physicsSystem_;

		/// [EN] Listener that queues and dispatches body-contact events.
		/// [JP] ボディ接触イベントをキューへ積み通知するリスナー。
		JoltContactListener contactListener_;
	};
}
