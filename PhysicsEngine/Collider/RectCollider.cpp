#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ShapeHandle RectCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateRectShape(size_, center_);
	}

	void RectCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	void RectCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}
}
