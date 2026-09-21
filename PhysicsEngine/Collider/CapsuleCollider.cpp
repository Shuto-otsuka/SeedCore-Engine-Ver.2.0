#include <PhysicsEngine/Collider/CapsuleCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the capsule shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カプセル形状とコライダーボディを生成する。
	*/
	void CapsuleCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	/**
	* [EN]
	* Destroys the collider body and releases its shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コライダーボディを破棄し、形状を解放する。
	*/
	void CapsuleCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a capsule shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定からカプセル形状を生成して返す。
	*/
	Handle<JPH::Shape> CapsuleCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCapsuleShape(height_, radius_);
	}
}
