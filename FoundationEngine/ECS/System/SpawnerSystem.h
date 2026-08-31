#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class World;
	class ResourceCache;

	/**
	* [EN]
	* Drives every Spawner component in a World: while a Spawner's
	* autoStart_ is set and its currently-alive instance count hasn't
	* reached maxCount_, accumulates elapsed time against spawnInterval_
	* and, once due, instantiates its referenced prefab as a root-level
	* actor at the Spawner's own world position (optionally jittered
	* within randomRadius_ when randomSpawn_ is set) - not parented under
	* the Spawner's own actor, since PhysicsSystem places a Rigidbody's
	* JPH body from local Position directly, ignoring any parent
	* transform. Every spawned instance is destroyed after lifeTime_
	* seconds, freeing its slot so spawning can resume up to maxCount_
	* again. Update must be called every frame to advance each Spawner's
	* timers. Runtime progress (elapsed time, live instances) is tracked
	* here per entity rather than on Spawner itself, since Spawner only
	* holds reflected/serialized configuration.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の全 Spawner コンポーネントを駆動する: Spawner の
	* autoStart_ が有効で、かつ現在生存中のインスタンス数が maxCount_ に
	* 達していない間、経過時間を spawnInterval_ に対して積算し、期限が
	* 来たら参照先のプレハブを、Spawner 自身のワールド位置
	* (randomSpawn_ が有効なら randomRadius_ 内でランダムにずらす)へ、
	* ルートレベルの actor として生成する - Spawner 自身の actor の子には
	* しない。PhysicsSystem は Rigidbody の JPH ボディをローカル Position
	* からそのまま配置し、親の変換を考慮しないため。生成された各
	* インスタンスは lifeTime_ 秒後に破棄され、その枠が空いて再び
	* maxCount_ まで生成できるようになる。各 Spawner の
	* タイマーを進めるには、Update を毎フレーム呼び出す必要がある。
	* ランタイムの進行状況（経過時間、生存中インスタンス）は Spawner
	* 自身ではなくこちらでエンティティごとに追跡する。Spawner は
	* リフレクション/シリアライズされる設定値のみを保持するため。
	*/
	class SpawnerSystem
	{
	public:
		/**
		* [EN]
		* Advances every Spawner component's timers by deltaTime:
		* destroys any spawned instance whose lifeTime_ has elapsed
		* (freeing its slot), then instantiates its prefab whenever
		* spawnInterval_ elapses and the live instance count hasn't yet
		* reached maxCount_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全 Spawner コンポーネントのタイマーを deltaTime だけ進める:
		* lifeTime_ が経過した生成済みインスタンスを破棄し(その枠を空け)、
		* その上で spawnInterval_ が経過し生存中インスタンス数が
		* maxCount_ に達していない度に、そのプレハブを生成する。
		*/
		void Update(World& world, ResourceCache& cache, Float deltaTime);

		/**
		* [EN]
		* Forgets every Spawner's runtime progress (elapsed time, tracked
		* live instances) without touching the actual Actors - call this
		* whenever the World itself was reset out from under this system
		* (e.g. WorldSnapshot::Restore on Editor Stop), so stale
		* EntityIDs from the previous session aren't carried forward.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全 Spawner のランタイム進行状況（経過時間、追跡中の生存
		* インスタンス）を、実際の Actor には触れずに忘れる - このシステム
		* の下で World 自体がリセットされた時(例: Editor の Stop 時の
		* WorldSnapshot::Restore)に呼ぶこと。前回セッションの古い
		* EntityID を持ち越さないようにするため。
		*/
		void Reset();

	private:
		/**
		* [EN]
		* A single prefab instance spawned by a Spawner, counting down
		* toward its own destruction.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Spawner が生成した単一のプレハブインスタンス。自身の破棄までの
		* 時間をカウントダウンする。
		*/
		struct SpawnedInstance
		{
			/// [EN] EntityID of the instantiated prefab's root actor.
			/// [JP] 生成されたプレハブのルート actor の EntityID。
			EntityID entityID_;

			/// [EN] Time remaining before this instance is destroyed.
			/// [JP] このインスタンスが破棄されるまでの残り時間。
			Float remainingLifeTime_ = 0.0f;
		};

		/**
		* [EN]
		* Per-entity runtime progress for a Spawner, tracked outside the
		* component itself.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Spawner のエンティティごとのランタイム進行状況。コンポーネント
		* 自体の外側で追跡する。
		*/
		struct RuntimeState
		{
			/// [EN] Time accumulated since the last spawn.
			/// [JP] 直近の生成からの経過時間。
			Float elapsedTime_ = 0.0f;

			/// [EN] Every instance this Spawner has spawned that hasn't been destroyed yet.
			/// [JP] この Spawner が生成した、まだ破棄されていない全インスタンス。
			DynamicArray<SpawnedInstance> instances_;
		};

		/// [EN] Runtime progress for every entity currently holding a Spawner, keyed by EntityID.
		/// [JP] 現在 Spawner を持つ全エンティティのランタイム進行状況。EntityID をキーとする。
		FlatMap<EntityID, RuntimeState> runtimeState_;

		/// [EN] Random engine used for randomSpawn_'s position jitter.
		/// [JP] randomSpawn_ の位置ジッターに使う乱数エンジン。
		std::mt19937 randomEngine_ = std::mt19937(std::random_device{}());
	};
}
