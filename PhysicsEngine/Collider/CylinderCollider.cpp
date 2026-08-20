#include <PhysicsEngine/Collider/CylinderCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ShapeHandle CylinderCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateCylinderShape(height_, radius_);
	}

	void CylinderCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	void CylinderCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}
}
