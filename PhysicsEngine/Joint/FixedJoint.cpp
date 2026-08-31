#include <PhysicsEngine/Joint/FixedJoint.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	void FixedJoint::OnStart()
	{
		if (!enabled_)
		{
			return;
		}

		Actor actor = GetActor();

		Rigidbody* selfBody = actor.GetComponent<Rigidbody>();
		if (!selfBody)
		{
			SC_LOG_WARNING("FixedJoint: 同じアクターに Rigidbody がありません。");
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

		FixedJointDesc desc;

		handle_ = actor.GetPhysics().CreateFixedJoint(selfBody->GetBodyID(), connectedBodyID, desc);
	}

	void FixedJoint::OnDestroy()
	{
		if (handle_.exists())
		{
			GetActor().GetPhysics().DestroyJoint(handle_);
			handle_ = ConstraintHandle::null();
		}
	}
}
