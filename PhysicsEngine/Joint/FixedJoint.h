#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	class FixedJoint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

	public:
		void OnStart();

		void OnDestroy();

	private:
		ConstraintHandle handle_;
	};
	REGISTER_COMPONENT(FixedJoint, "Physics");
}
