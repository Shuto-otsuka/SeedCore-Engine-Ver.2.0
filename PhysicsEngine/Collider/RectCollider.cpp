#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the rectangle shape and collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 矩形状とコライダーボディを生成する。
	*/
	void RectCollider::OnAwake()
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
	void RectCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}

	/**
	* [EN]
	* Creates and returns a rectangle shape from the current settings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の設定から矩形状を生成して返す。
	*/
	Handle<JPH::Shape> RectCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateRectShape(size_, center_);
	}
}
