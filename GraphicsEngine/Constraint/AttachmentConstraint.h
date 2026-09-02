#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class SEEDCORE_API AttachmentConstraint :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		SC_PAYLOAD_FIELD_EX("ターゲット", Actor)
		Uint32 target_ = 0;

		SC_REFLECTION_CLAMPED_EX("重み", 0.0f, 1.0f)
		Float weight_ = 1.0f;

		SC_REFLECTION_FIELD_EX("位置オフセット")
		Vector3 positionOffset_ = { 0.0f, 0.0f, 0.0f };

		SC_REFLECTION_FIELD_EX("回転オフセット(オイラー角)")
		Vector3 rotationOffset_ = { 0.0f, 0.0f, 0.0f };

		SC_SERIALIZE_FIELD()
		String boneName_;

	public:
		void OnInspectorGUI();
	};
	REGISTER_COMPONENT(AttachmentConstraint, "Animation");
}
