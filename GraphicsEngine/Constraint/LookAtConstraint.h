#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct LookAtConstraint :public SeedScript
	{
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("ターゲット", Actor)
		Uint32 target_ = 0;

		SC_REFLECTION_CLAMPED_EX("重み", 0.0f, 1.0f)
		Float weight_ = 1.0f;

		SC_REFLECTION_FIELD_EX("上方向")
		Vector3 upVector_ = { 0.0f, 1.0f, 0.0f };
	};
	REGISTER_COMPONENT(LookAtConstraint, "Animation");
}
