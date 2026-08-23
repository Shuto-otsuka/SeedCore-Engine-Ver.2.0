#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Spawner
	{
		SC_PAYLOAD_FIELD_EX("プレハブID", Prefab)
		Uint32 prefabID_ = 0;

		SC_REFLECTION_FIELD_EX("開始時に自動生成")
		Bool autoStart_ = false;

		SC_REFLECTION_CLAMPED_EX("生成間隔", 0.0f, 3600.0f)
		Float spawnInterval_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("最大生成数", 1, 1000)
		Int maxCount_ = 1;

		SC_REFLECTION_CLAMPED_EX("生存時間", 0.1f, 3600.0f)
		Float lifeTime_ = 5.0f;

		SC_REFLECTION_FIELD_EX("ランダムスポーン")
		Bool randomSpawn_ = false;

		SC_REFLECTION_FIELD_CONDITION(randomSpawn_)
		SC_REFLECTION_CLAMPED_EX("ランダム範囲", 0.0f, 1000.0f)
		Float randomRadius_ = 1.0f;
	};
	REGISTER_COMPONENT(Spawner, "Gameplay");
}
