#include <FoundationEngine/ECS/System/SpawnerSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Spawner.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/PrefabPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every Spawner component's timers by deltaTime: destroys
	* any spawned instance whose lifeTime_ has elapsed (freeing its
	* slot), then instantiates its prefab whenever spawnInterval_
	* elapses and the live instance count hasn't yet reached maxCount_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Spawner コンポーネントのタイマーを deltaTime だけ進める:
	* lifeTime_ が経過した生成済みインスタンスを破棄し(その枠を空け)、
	* その上で spawnInterval_ が経過し生存中インスタンス数が maxCount_ に
	* 達していない度に、そのプレハブを生成する。
	*/
	void SpawnerSystem::Update(World& world, ResourceCache& cache, Float deltaTime)
	{
		for (EntityID entityID : world.GetComponents<Spawner>())
		{
			Actor* actor = world.GetActor(entityID);
			if (actor == nullptr)
			{
				continue;
			}

			Spawner* spawner = world.GetComponent<Spawner>(actor->GetEntity());
			if (spawner == nullptr)
			{
				continue;
			}

			RuntimeState& state = runtimeState_[entityID];

			for (Size instanceIndex = 0; instanceIndex < state.instances_.size(); )
			{
				SpawnedInstance& instance = state.instances_[instanceIndex];
				instance.remainingLifeTime_ -= deltaTime;
				if (instance.remainingLifeTime_ > 0.0f)
				{
					instanceIndex++;
					continue;
				}

				Actor* spawnedActor = world.GetActor(instance.entityID_);
				if (spawnedActor != nullptr)
				{
					world.DestroyActor(spawnedActor);
				}

				state.instances_[instanceIndex] = state.instances_.back();
				state.instances_.pop_back();
			}

			if (!spawner->autoStart_)
			{
				continue;
			}

			if (static_cast<Int>(state.instances_.size()) >= spawner->maxCount_)
			{
				continue;
			}

			state.elapsedTime_ += deltaTime;
			if (state.elapsedTime_ < spawner->spawnInterval_)
			{
				continue;
			}

			state.elapsedTime_ -= spawner->spawnInterval_;

			Handle<Prefab> handle = cache.GetPrefabPool().Load(spawner->prefabID_, cache);
			Prefab* prefab = cache.GetPrefabPool().Get(handle);
			if (prefab == nullptr)
			{
				continue;
			}

			/// [EN] Spawn as a root-level actor (no parent) rather than as a
			///      child of the Spawner's actor: Rigidbody/PhysicsSystem
			///      places a JPH body directly from an actor's local
			///      Position (PhysicsSystem::ApplyActorTransform), ignoring
			///      any parent transform - a parented spawn would render at
			///      the correct world position via TransformSystem but get
			///      its collider placed as if that local offset were a
			///      world position instead.
			/// [JP] Spawner の actor の子としてではなく、ルートレベルの
			///      actor(親無し)として生成する - Rigidbody/PhysicsSystem
			///      は actor のローカル Position をそのまま JPH ボディの
			///      配置に使う(PhysicsSystem::ApplyActorTransform)ため、
			///      親の変換を考慮しない。親付きで生成すると、見た目は
			///      TransformSystem 経由で正しいワールド位置に描画される
			///      一方、コライダーはそのローカルオフセットをワールド
			///      位置と誤認したまま配置されてしまう。
			Actor* spawnedActor = prefab->Instantiate(world, cache, nullptr);
			if (spawnedActor == nullptr)
			{
				continue;
			}

			Vector3 spawnPosition = actor->GetWorldMatrix().Translation();

			Position* position = world.GetComponent<Position>(spawnedActor->GetEntity());
			if (position != nullptr)
			{
				if (spawner->randomSpawn_)
				{
					std::uniform_real_distribution<Float> jitter(-spawner->randomRadius_, spawner->randomRadius_);
					position->x_ = spawnPosition.x + jitter(randomEngine_);
					position->y_ = spawnPosition.y + jitter(randomEngine_);
					position->z_ = spawnPosition.z + jitter(randomEngine_);
				}
				else
				{
					position->x_ = spawnPosition.x;
					position->y_ = spawnPosition.y;
					position->z_ = spawnPosition.z;
				}
			}

			SpawnedInstance instance;
			instance.entityID_ = static_cast<Uint32>(spawnedActor->GetEntity().GetHandle().index_);
			instance.remainingLifeTime_ = spawner->lifeTime_;
			state.instances_.push_back(instance);
		}
	}

	/**
	* [EN]
	* Forgets every Spawner's runtime progress (elapsed time, tracked
	* live instances) without touching the actual Actors.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Spawner のランタイム進行状況（経過時間、追跡中の生存
	* インスタンス）を、実際の Actor には触れずに忘れる。
	*/
	void SpawnerSystem::Reset()
	{
		runtimeState_.clear();
	}
}
