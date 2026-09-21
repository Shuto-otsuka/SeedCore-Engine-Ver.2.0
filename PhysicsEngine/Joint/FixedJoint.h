#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that rigidly locks its actor to another rigid body or the world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクターを別の剛体またはワールドへ固定するコンポーネント。
	*/
	class SEEDCORE_API FixedJoint :public SeedScript
	{
	public:
		/// [EN] Whether the joint is created when the component starts.
		/// [JP] コンポーネント開始時にジョイントを生成するか。
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		/// [EN] Entity ID of the actor connected to this joint; zero connects to the world.
		/// [JP] 接続先アクターのエンティティ ID。0 はワールドへ接続する。
		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

	public:
		/**
		* [EN]
		* Creates the fixed constraint between the available bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 利用可能なボディ間に固定拘束を生成する。
		*/
		void OnStart();

		/**
		* [EN]
		* Destroys the created constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生成した拘束を破棄する。
		*/
		void OnDestroy();

	private:
		/// [EN] Pool handle of the created Jolt constraint.
		/// [JP] 生成した Jolt 拘束のプールハンドル。
		Handle<JPH::Constraint> handle_;
	};
	REGISTER_COMPONENT(FixedJoint, "Physics");
}
