#include <PhysicsEngine/Collider/CircleCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ShapeHandle CircleCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCircleShape(radius_, center_);
	}

	void CircleCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	void CircleCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}
}
