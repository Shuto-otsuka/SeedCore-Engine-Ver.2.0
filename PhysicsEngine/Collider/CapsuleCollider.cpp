#include <PhysicsEngine/Collider/CapsuleCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ShapeHandle CapsuleCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCapsuleShape(height_, radius_);
	}

	void CapsuleCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	void CapsuleCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}
}
