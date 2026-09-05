#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Effector
	{
		SC_PAYLOAD_FIELD_EX("ターゲット", Actor)
		Uint32 target_ = 0;

		SC_PAYLOAD_FIELD_EX("ポール", Actor)
		Uint32 pole_ = 0;

		SC_REFLECTION_CLAMPED_EX("重み", 0.0f, 1.0f)
		Float weight_ = 1.0f;

		SC_SERIALIZE_FIELD()
		String effectorBoneName_;
	};

	class SEEDCORE_API IKConstraint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_REFLECTION_FIELD_EX("エフェクタ")
		DynamicArray<Effector> entries_;

	public:
		void OnInspectorGUI();
	};
	REGISTER_COMPONENT(IKConstraint, "Animation");
}
