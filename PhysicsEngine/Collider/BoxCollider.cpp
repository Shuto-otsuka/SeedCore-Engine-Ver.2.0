#include <PhysicsEngine/Collider/BoxCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the box shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 箱形状とコライダーボディを生成する。
	*/
	void BoxCollider::OnAwake()
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
	void BoxCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a box shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定から箱形状を生成して返す。
	*/
	Handle<JPH::Shape> BoxCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateBoxShape(size_, center_);
	}
}
