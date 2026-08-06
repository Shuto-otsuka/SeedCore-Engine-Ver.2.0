#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class SkyLight :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("スカイマップ使用")
		Bool useSkymap_ = false;

		SC_REFLECTION_FIELD_CONDITION(useSkymap_)
		SC_PAYLOAD_FIELD_EX("スカイマップID", Sky)
		Uint32 skymapID_ = 0;

		SC_REFLECTION_CLAMPED_EX("強度", 0.0f, 1.0f)
		Float intensity_ = 1.0f;
	};
	REGISTER_COMPONENT(SkyLight, "Light");
}
