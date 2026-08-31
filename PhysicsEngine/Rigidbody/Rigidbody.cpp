#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>

namespace SeedCore
{
	void Rigidbody::OnAwake()
	{
		Actor actor = GetActor();

		shapeHandle_ = PhysicsSystem::FindColliderShape(actor);

		RigidbodyDesc desc;
		desc.shape_ = shapeHandle_;
		PhysicsSystem::ApplyActorTransform(actor, desc);
		desc.motionType_ = ToMotionType(bodyType_);
		desc.layer_ = ToObjectLayer(bodyType_, actor.GetLayer());
		desc.mass_ = mass_;
		desc.linearDamping_ = linearDrag_;
		desc.angularDamping_ = angularDrag_;
		desc.friction_ = friction_;
		desc.restitution_ = restitution_;
		desc.gravityFactor_ = useGravity_ ? gravityScale_ : 0.0f;
		desc.allowedDOFs_ = PhysicsSystem::ToAllowedDOFs(*this);
		desc.isSensor_ = isTrigger_;

		bodyID_ = actor.GetPhysics().CreateRigidbody(desc);
	}

	void Rigidbody::OnFixedTick(Float elapsedTime)
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		Actor actor = GetActor();
		World& world = actor.GetWorld();
		Entity entity = actor.GetEntity();

		Position* position = world.GetComponent<Position>(entity);
		Rotation* rotation = world.GetComponent<Rotation>(entity);
		if (!position && !rotation)
		{
			return;
		}

		Vector3 outPosition;
		Quaternion outRotation;
		actor.GetPhysics().GetBodyTransform(bodyID_, outPosition, outRotation);

		if (position)
		{
			position->x_ = outPosition.x;
			position->y_ = outPosition.y;
			position->z_ = outPosition.z;
		}

		if (rotation)
		{
			const Vector3 euler = outRotation.ToEuler();
			rotation->x_ = ToDegrees(euler.x);
			rotation->y_ = ToDegrees(euler.y);
			rotation->z_ = ToDegrees(euler.z);
		}
	}

	void Rigidbody::OnDestroy()
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		Actor actor = GetActor();
		actor.GetPhysics().DestroyBody(bodyID_);
		actor.GetPhysics().ReleaseShape(shapeHandle_);

		bodyID_ = JPH::BodyID();
	}

	JPH::BodyID Rigidbody::GetBodyID()const
	{
		return bodyID_;
	}
}
