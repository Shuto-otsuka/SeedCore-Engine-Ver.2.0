#include <FoundationEngine/ECS/System/SystemScheduler.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/ComponentBase.h>

namespace SeedCore
{
	/**
	* [EN]
	* Runs one frame: drives Awake/Start (if isPlaying), drives
	* MoveSystem then SpawnerSystem then LifetimeSystem (if isPlaying,
	* all before TransformSystem so this frame's motion and any newly
	* spawned actor's position are reflected in this same frame's world
	* matrix), runs the built-in TransformSystem, then drives Tick/
	* LateTick (if isPlaying).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を
	* 駆動し、（isPlaying であれば、TransformSystem より前に — 今フレームの
	* 移動や新しく生成された actor の位置が同じフレームのワールド行列に
	* 反映されるように）MoveSystem、続けて SpawnerSystem、LifetimeSystem を
	* 駆動し、組み込みの TransformSystem を実行し、（isPlaying であれば）
	* Tick/LateTick を駆動する。
	*/
	void SystemScheduler::Run(World& world, ResourceCache& cache, Float elapsedTime, Bool isPlaying)
	{
		if (isPlaying)
		{
			for (Actor actor : world.GetActors())
			{
				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentBaseIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBase* component = static_cast<ComponentBase*>(data);
					if (!component->awoken_)
					{
						component->awoken_ = true;
						if (component->awake_)
						{
							component->awake_(component);
						}
					}
				}
			}

			for (Actor actor : world.GetActors())
			{
				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentBaseIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBase* component = static_cast<ComponentBase*>(data);
					if (!component->started_)
					{
						component->started_ = true;
						if (component->start_)
						{
							component->start_(component);
						}
					}
				}
			}
		}

		/// [EN] Spawn before TransformSystem::Execute (not alongside
		///      Tick/LateTick below) so a newly spawned actor's world
		///      matrix is computed from its post-spawn Position this same
		///      frame - otherwise it would still hold whatever matrix
		///      Prefab::Instantiate initialized it with when next frame's
		///      Awake dispatch runs, and Rigidbody::OnAwake (which reads
		///      the actor's world matrix, not Position, to place its JPH
		///      body) would create the collider at the wrong spot.
		/// [JP] TransformSystem::Execute より前に(下の Tick/LateTick とは
		///      別に)スポーンする - こうしないと、スポーン後に設定した
		///      Position が反映される前の、Prefab::Instantiate が初期化
		///      した時点のワールド行列のまま次フレームの Awake ディスパッチ
		///      を迎えてしまい、Rigidbody::OnAwake(JPH ボディの配置に
		///      Position ではなく actor のワールド行列を読む)が誤った
		///      位置でコライダーを作ってしまう。
		if (isPlaying)
		{
			moveSystem_.Execute(world, elapsedTime);
			spawnerSystem_.Update(world, cache, elapsedTime);
			lifetimeSystem_.Update(world, elapsedTime);
		}

		transformSystem_.Execute(world);

		if (isPlaying)
		{
			for (Actor actor : world.GetActors())
			{
				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentBaseIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBase* component = static_cast<ComponentBase*>(data);
					if (component->tick_)
					{
						component->tick_(component, elapsedTime);
					}
				}
			}

			for (Actor actor : world.GetActors())
			{
				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentBaseIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBase* component = static_cast<ComponentBase*>(data);
					if (component->lateTick_)
					{
						component->lateTick_(component, elapsedTime);
					}
				}
			}
		}
	}

	/**
	* [EN]
	* Advances one fixed timestep: dispatches FixedTick to every
	* ComponentBase-derived component that implements it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 固定タイムステップぶん1ステップ進める: FixedTick を実装している
	* 全ての ComponentBase 派生コンポーネントへディスパッチする。
	*/
	void SystemScheduler::Step(World& world, Float fixedTime)
	{
		for (Actor actor : world.GetActors())
		{
			Entity entity = actor.GetEntity();
			EntityID entityID = entity.GetID();

			for (ComponentID id : actor.ComponentBaseIDList())
			{
				void* data = world.GetComponent(entityID, id);
				if (!data)
				{
					continue;
				}

				ComponentBase* component = static_cast<ComponentBase*>(data);
				if (component->fixedTick_)
				{
					component->fixedTick_(component, fixedTime);
				}
			}
		}
	}

	/**
	* [EN]
	* Forgets SpawnerSystem's runtime progress for every Spawner.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SpawnerSystem が持つ、全 Spawner のランタイム進行状況を忘れる。
	*/
	void SystemScheduler::Reset()
	{
		spawnerSystem_.Reset();
		lifetimeSystem_.Reset();
	}
}
