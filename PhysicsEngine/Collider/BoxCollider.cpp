#include <PhysicsEngine/Collider/BoxCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	ShapeHandle BoxCollider::GetShapeHandle()const
	{
		return GetActor().GetPhysics().CreateBoxShape(size_, center_);
	}

	void BoxCollider::OnAwake()
	{
		shapeHandle_ = GetShapeHandle();
		bodyID_ = PhysicsSystem::CreateColliderBody(GetActor(), shapeHandle_, isTrigger_);
	}

	void BoxCollider::OnDestroy()
	{
		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);
	}
}
