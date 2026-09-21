#include <PhysicsEngine/Collider/SphereCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the sphere shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 球形状とコライダーボディを生成する。
	*/
	void SphereCollider::OnAwake()
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
	void SphereCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a sphere shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定から球形状を生成して返す。
	*/
	Handle<JPH::Shape> SphereCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateSphereShape(radius_);
	}
}
