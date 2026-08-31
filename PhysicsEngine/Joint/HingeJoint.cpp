#include <PhysicsEngine/Joint/HingeJoint.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	void HingeJoint::OnStart()
	{
		if (!enabled_)
		{
			return;
		}

		Actor actor = GetActor();

		Rigidbody* selfBody = actor.GetComponent<Rigidbody>();
		if (!selfBody)
		{
			SC_LOG_WARNING("HingeJoint: 同じアクターに Rigidbody がありません。");
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

		HingeJointDesc desc;
		desc.anchor_ = anchor_;
		desc.axis_ = axis_;
		desc.useLimits_ = useLimits_;
		desc.minAngle_ = minAngle_;
		desc.maxAngle_ = maxAngle_;

		handle_ = actor.GetPhysics().CreateHingeJoint(selfBody->GetBodyID(), connectedBodyID, desc);
	}

	void HingeJoint::OnDestroy()
	{
		if (handle_.exists())
		{
			GetActor().GetPhysics().DestroyJoint(handle_);
			handle_ = ConstraintHandle::null();
		}
	}
}
