#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	class HingeJoint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

		SC_REFLECTION_FIELD_EX("アンカー(ローカル)")
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };

		SC_REFLECTION_FIELD_EX("ヒンジ軸(ローカル)")
		Vector3 axis_ = { 0.0f, 1.0f, 0.0f };

		SC_REFLECTION_FIELD_EX("角度制限を使う")
		Bool useLimits_ = false;

		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最小角(度)", -180.0f, 0.0f)
		Float minAngle_ = 0.0f;

		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最大角(度)", 0.0f, 180.0f)
		Float maxAngle_ = 0.0f;

	public:
		void OnStart();

		void OnDestroy();

	private:
		ConstraintHandle handle_;
	};
	REGISTER_COMPONENT(HingeJoint, "Physics");
}
