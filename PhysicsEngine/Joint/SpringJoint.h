#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	class SEEDCORE_API SpringJoint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

		SC_REFLECTION_FIELD_EX("アンカー(ローカル)")
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };

		SC_REFLECTION_CLAMPED_EX("最小距離", 0.0f, 1000.0f)
		Float minDistance_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("最大距離", 0.0f, 1000.0f)
		Float maxDistance_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("剛性(Hz)", 0.0f, 30.0f)
		Float frequency_ = 2.0f;

		SC_REFLECTION_CLAMPED_EX("減衰", 0.0f, 1.0f)
		Float damping_ = 0.5f;

	public:
		void OnStart();

		void OnDestroy();

	private:
		ConstraintHandle handle_;
	};
	REGISTER_COMPONENT(SpringJoint, "Physics");
}
