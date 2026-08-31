#include <PhysicsEngine/Joint/SpringJoint.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	void SpringJoint::OnStart()
	{
		if (!enabled_)
		{
			return;
		}

		Actor actor = GetActor();

		Rigidbody* selfBody = actor.GetComponent<Rigidbody>();
		if (!selfBody)
		{
			SC_LOG_WARNING("SpringJoint: 同じアクターに Rigidbody がありません。");
			return;
		}

		JPH::BodyID connectedBodyID;
		if (connectedActor_ != 0)
		{
			Actor connected = actor.GetWorld().FindActor(connectedActor_);
			Rigidbody* connectedBody = connected ? connected.GetComponent<Rigidbody>() : nullptr;
			if (connectedBody)
			{
				connectedBodyID = connectedBody->GetBodyID();
			}
		}

		SpringJointDesc desc;
		desc.anchor_ = anchor_;
		desc.minDistance_ = minDistance_;
		desc.maxDistance_ = maxDistance_;
		desc.frequency_ = frequency_;
		desc.damping_ = damping_;

		handle_ = actor.GetPhysics().CreateSpringJoint(selfBody->GetBodyID(), connectedBodyID, desc);
	}

	void SpringJoint::OnDestroy()
	{
		if (handle_.exists())
		{
			GetActor().GetPhysics().DestroyJoint(handle_);
			handle_ = ConstraintHandle::null();
		}
	}
}
