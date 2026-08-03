#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct RectangleLight
	{
		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 1,1,1,1 };

		SC_REFLECTION_CLAMPED_EX("正面の向き", -1.0f, 1.0f)
		Vector3 direction_ = { 0.0f, 0.0f, -1.0f };

		SC_REFLECTION_CLAMPED_EX("上方向", -1.0f, 1.0f)
		Vector3 up_ = { 0.0f, 1.0f, 0.0f };

		SC_REFLECTION_CLAMPED_EX("幅", 0.01f, 100.0f)
		Float width_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("高さ", 0.01f, 100.0f)
		Float height_ = 1.0f;

		SC_REFLECTION_FIELD_EX("範囲")
		Float range_ = 10.0f;

		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 200.0f)
		Float intensity_ = 1.0f;
	};
	REGISTER_COMPONENT(RectangleLight, "Light", ComponentStorage::Archetype);
}
