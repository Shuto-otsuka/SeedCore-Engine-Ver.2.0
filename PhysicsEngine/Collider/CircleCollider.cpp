#include <PhysicsEngine/Collider/CircleCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the circle shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 円形状とコライダーボディを生成する。
	*/
	void CircleCollider::OnAwake()
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
	void CircleCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a circle shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定から円形状を生成して返す。
	*/
	Handle<JPH::Shape> CircleCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCircleShape(radius_, center_);
	}
}
