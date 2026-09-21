#include <PhysicsEngine/Collider/CylinderCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the cylinder shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 円柱形状とコライダーボディを生成する。
	*/
	void CylinderCollider::OnAwake()
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
	void CylinderCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a cylinder shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定から円柱形状を生成して返す。
	*/
	Handle<JPH::Shape> CylinderCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCylinderShape(height_, radius_);
	}
}
