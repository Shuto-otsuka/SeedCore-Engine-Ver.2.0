#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that periodically instantiates a prefab. Holds only the
	* configuration; SpawnerSystem drives it, tracking per entity the
	* elapsed time and the set of live instances. While autoStart_ is
	* set and fewer than maxCount_ instances are alive, it spawns the
	* referenced prefab every spawnInterval_ seconds as a root-level
	* actor at this Spawner's world position (jittered within
	* randomRadius_ when randomSpawn_ is set). Each spawned instance is
	* destroyed after lifeTime_ seconds, freeing a slot. Pairs with the
	* Lifetime component, which spawned instances effectively carry.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 一定間隔でプレハブを生成するコンポーネント。設定値のみを保持し、
	* 駆動は SpawnerSystem が行う（経過時間と生存中インスタンス集合を
	* エンティティごとに追跡する）。autoStart_ が有効で生存インスタンス
	* 数が maxCount_ 未満の間、参照先のプレハブを spawnInterval_ 秒ごとに、
	* この Spawner のワールド位置（randomSpawn_ が有効なら randomRadius_
	* 内でランダムにずらす）へルートレベルの actor として生成する。
	* 生成された各インスタンスは lifeTime_ 秒後に破棄され、枠が空く。
	* 生成インスタンスが実質的に持つ Lifetime コンポーネントと対になる。
	*/
	struct Spawner
	{
		/// [EN] Asset ID of the prefab to spawn.
		/// [JP] 生成するプレハブのアセット ID。
		SC_PAYLOAD_FIELD_EX("プレハブID", Prefab)
		Uint32 prefabID_ = 0;

		/// [EN] When set, SpawnerSystem spawns automatically; otherwise spawning must be triggered externally.
		/// [JP] 有効なら SpawnerSystem が自動で生成する。無効なら外部から生成をトリガーする必要がある。
		SC_REFLECTION_FIELD_EX("開始時に自動生成")
		Bool autoStart_ = false;

		/// [EN] Seconds between spawns.
		/// [JP] 生成と生成の間隔（秒）。
		SC_REFLECTION_CLAMPED_EX("生成間隔", 0.0f, 3600.0f)
		Float spawnInterval_ = 1.0f;

		/// [EN] Maximum number of live instances at once; spawning pauses while this many are alive.
		/// [JP] 同時に生存できるインスタンスの最大数。この数だけ生存している間は生成が止まる。
		SC_REFLECTION_CLAMPED_EX("最大生成数", 1, 1000)
		Int maxCount_ = 1;

		/// [EN] Seconds each spawned instance lives before it is destroyed.
		/// [JP] 生成された各インスタンスが破棄されるまで存在する秒数。
		SC_REFLECTION_CLAMPED_EX("生存時間", 0.1f, 3600.0f)
		Float lifeTime_ = 5.0f;

		/// [EN] When set, each spawn position is offset by a random vector within randomRadius_ of the Spawner.
		/// [JP] 有効なら、各生成位置を Spawner から randomRadius_ 以内のランダムなベクトルだけずらす。
		SC_REFLECTION_FIELD_EX("ランダムスポーン")
		Bool randomSpawn_ = false;

		/// [EN] Radius of the random spawn offset applied per axis when randomSpawn_ is set.
		/// [JP] randomSpawn_ が有効なとき、軸ごとに適用されるランダム生成オフセットの範囲。
		SC_REFLECTION_FIELD_CONDITION(randomSpawn_)
		SC_REFLECTION_CLAMPED_EX("ランダム範囲", 0.0f, 1000.0f)
		Float randomRadius_ = 1.0f;
	};
	REGISTER_COMPONENT(Spawner, "Gameplay");
}