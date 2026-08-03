#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct PointLight
	{
		SC_REFLECTION_FIELD_EX("色")
		Color color_ = { 1,1,1,1 };

		SC_REFLECTION_FIELD_EX("範囲")
		Float range_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 200.0f)
		Float intensity_ = 1.0f;
	};
	REGISTER_COMPONENT(PointLight, "Light", ComponentStorage::Archetype);
}