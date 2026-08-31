#include <FoundationEngine/ECS/System/SpawnerSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/CommandBuffer.h>
#include <FoundationEngine/ECS/Component/Spawner.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every Spawner component's timers by deltaTime: records, on
	* cmd, the destruction of any spawned instance whose lifeTime_ has
	* elapsed (freeing its slot), then records a deferred prefab spawn
	* whenever spawnInterval_ elapses and the live instance count hasn't
	* yet reached maxCount_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Spawner コンポーネントのタイマーを deltaTime だけ進める:
	* lifeTime_ が経過した生成済みインスタンスの破棄を cmd へ記録し(その
	* 枠を空け)、その上で spawnInterval_ が経過し生存中インスタンス数が
	* maxCount_ に達していない度に、プレハブの遅延生成を記録する。
	*/
	void SpawnerSystem::Execute(CommandBuffer& cmd, World& world, Float deltaTime)
	{
		for (EntityID entityID : world.GetComponents<Spawner>())
		{
			Actor actor = world.GetActor(entityID);
			if (!actor)
			{
				continue;
			}

			Spawner* spawner = world.GetComponent<Spawner>(actor.GetEntity());
			if (spawner == nullptr)
			{
				continue;
			}

			RuntimeState& state = runtimeState_[entityID];

			/// [EN] Instances spawned last frame were tracked by the provisional EntityID cmd handed back; the flush that ran since then resolved it to a real one.
			/// [JP] 前フレームに生成したインスタンスは cmd が返した暫定 EntityID で追跡していた。その後の flush で実際の EntityID へ解決済み。
			for (SpawnedInstance& instance : state.instances_)
			{
				if (CommandBuffer::IsProvisional(instance.entityID_))
				{
					EntityID resolved = cmd.Resolved(instance.entityID_);
					if (resolved != EntityID{})
					{
						instance.entityID_ = resolved;
					}
				}
			}

			for (Size instanceIndex = 0; instanceIndex < state.instances_.size(); )
			{
				SpawnedInstance& instance = state.instances_[instanceIndex];
				instance.remainingLifeTime_ -= deltaTime;

				Bool stillProvisional = CommandBuffer::IsProvisional(instance.entityID_);
				Bool destroyedElsewhere = !stillProvisional && !world.GetActor(instance.entityID_);

				if (instance.remainingLifeTime_ > 0.0f && !destroyedElsewhere)
				{
					instanceIndex++;
					continue;
				}

				if (instance.remainingLifeTime_ <= 0.0f && !stillProvisional && !destroyedElsewhere)
				{
					cmd.DestroyEntity(instance.entityID_);
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

			if (spawner->prefabID_ == 0)
			{
				continue;
			}

			/// [EN] Spawn as a root-level actor (no parent): PhysicsSystem places a Rigidbody's JPH body from the actor's local Position directly (PhysicsSystem::ApplyActorTransform), ignoring any parent transform, so a parented spawn would render correctly via TransformSystem but get its collider placed as if the local offset were a world position.
			/// [JP] ルートレベルの actor(親無し)として生成する: PhysicsSystem は Rigidbody の JPH ボディを actor のローカル Position からそのまま配置し(PhysicsSystem::ApplyActorTransform)、親の変換を考慮しないため、親付きで生成すると見た目は TransformSystem 経由で正しくてもコライダーがローカルオフセットをワールド位置と誤認して配置される。
			Vector3 spawnPosition = actor.GetWorldMatrix().Translation();
			if (spawner->randomSpawn_)
			{
				std::uniform_real_distribution<Float> jitter(-spawner->randomRadius_, spawner->randomRadius_);
				spawnPosition.x += jitter(randomEngine_);
				spawnPosition.y += jitter(randomEngine_);
				spawnPosition.z += jitter(randomEngine_);
			}

			SpawnedInstance instance;
			instance.entityID_ = cmd.SpawnPrefab(spawner->prefabID_, spawnPosition);
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
