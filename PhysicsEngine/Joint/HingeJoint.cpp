#include <PhysicsEngine/Joint/HingeJoint.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the configured hinge constraint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 設定されたヒンジ拘束を生成する。
	*/
	void HingeJoint::OnStart()
	{
		if (!enabled_)
		{
			return;
		}

		Actor actor = GetActor();

		/// [EN] Resolve the owning body and optional connected body before creating the constraint.
		/// [JP] 拘束の生成前に、所有ボディと任意の接続先ボディを解決する。
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
				connectedBodyID = connectedBody->BodyID();
			}
		}

		HingeJointDesc desc;
		desc.anchor_ = anchor_;
		desc.axis_ = axis_;
		desc.useLimits_ = useLimits_;
		desc.minAngle_ = minAngle_;
		desc.maxAngle_ = maxAngle_;

		handle_ = actor.GetPhysics().CreateHingeJoint(selfBody->BodyID(), connectedBodyID, desc);
	}

	/**
	* [EN]
	* Destroys the created constraint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生成した拘束を破棄する。
	*/
	void HingeJoint::OnDestroy()
	{
		if (handle_.exists())
		{
			GetActor().GetPhysics().DestroyJoint(handle_);
			handle_ = Handle<JPH::Constraint>::null();
		}
	}
}
