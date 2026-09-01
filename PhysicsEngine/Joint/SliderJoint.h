#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	class SEEDCORE_API SliderJoint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

		SC_REFLECTION_FIELD_EX("スライド軸(ローカル)")
		Vector3 axis_ = { 1.0f, 0.0f, 0.0f };

		SC_REFLECTION_FIELD_EX("可動範囲制限を使う")
		Bool useLimits_ = false;

		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最小距離", -1000.0f, 0.0f)
		Float minDistance_ = 0.0f;

		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最大距離", 0.0f, 1000.0f)
		Float maxDistance_ = 0.0f;

	public:
		void OnStart();

		void OnDestroy();

	private:
		ConstraintHandle handle_;
	};
	REGISTER_COMPONENT(SliderJoint, "Physics");
}
