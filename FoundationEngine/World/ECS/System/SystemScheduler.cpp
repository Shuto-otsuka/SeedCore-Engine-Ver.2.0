#include <FoundationEngine/World/ECS/System/SystemScheduler.h>
#include <FoundationEngine/World/ECS/System/TransformSystem.h>
#include <FoundationEngine/World/ECS/System/MoveSystem.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/ECS/Query/Query.h>
#include <FoundationEngine/World/ECS/Component/ComponentBehaviour.h>
#include <FoundationEngine/World/ECS/Component/Position.h>
#include <FoundationEngine/World/ECS/Component/Velocity.h>
#include <FoundationEngine/World/ECS/Component/Spawner.h>
#include <FoundationEngine/World/ECS/Component/Lifetime.h>

namespace SeedCore
{
	/**
	* [EN]
	* Runs one frame: drives Awake/Start (if isPlaying), then (if
	* isPlaying) runs MoveSystem and the structural systems (Spawner +
	* Lifetime) through SystemGraph on executor - MoveSystem in parallel
	* with the structural pair since their component accesses do not
	* conflict - flushes the recorded structural changes, runs the
	* built-in TransformSystem (all before Tick/LateTick so this frame's
	* motion and any newly spawned actor's position are in this frame's
	* world matrix), then drives Tick/LateTick (if isPlaying). Inactive
	* actors receive none of Awake/Start/Tick/LateTick.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を駆動し、
	* （isPlaying であれば）MoveSystem と構造系システム（Spawner +
	* Lifetime）を SystemGraph 経由で executor 上で実行する - MoveSystem は
	* コンポーネントアクセスが衝突しないため構造系ペアと並列に走る -
	* 記録された構造変更を flush し、組み込みの TransformSystem を実行し
	* （すべて Tick/LateTick より前 — 今フレームの移動や新しく生成された
	* actor の位置が同じフレームのワールド行列に入るように）、
	* （isPlaying であれば）Tick/LateTick を駆動する。非アクティブな
	* actor には Awake/Start/Tick/LateTick のどれも送らない。
	*/
	void SystemScheduler::Run(World& world, ResourceCache& cache, JobExecutor& executor, Float elapsedTime, Bool isPlaying)
	{
		if (isPlaying)
		{
			for (Actor actor : world.GetActors())
			{
				if (!actor.Active())
				{
					continue;
				}

				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
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
				if (!actor.Active())
				{
					continue;
				}

				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
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

		/// [EN] Spawn before TransformSystem::Execute so a new actor's world matrix is built from its spawn Position this frame;
		///      Rigidbody::OnAwake places its body from the world matrix, not from Position.
		/// [JP] TransformSystem::Execute より前にスポーンし、新しい actor のワールド行列をこのフレームのうちにスポーン位置から作る。
		///      Rigidbody::OnAwake はボディを Position ではなくワールド行列から配置するため。
		if (isPlaying)
		{
			/// [EN] MoveSystem (reads Velocity, writes Position) has no component-access conflict with the structural systems, so SystemGraph runs them in parallel on the executor. SpawnerSystem and LifetimeSystem share one CommandBuffer, so they are registered as a single serial task rather than two.
			/// [JP] MoveSystem(Velocity を読み Position を書く)は構造系システムとコンポーネントアクセスが衝突しないため、SystemGraph が executor 上で並列に走らせる。SpawnerSystem と LifetimeSystem は1つの CommandBuffer を共有するので、2つではなく1つの直列タスクとして登録する。
			systemGraph_.Clear();
			systemGraph_.Add(
				Query<Read<Velocity>, Write<Position>>::GetReadSignature(),
				Query<Read<Velocity>, Write<Position>>::GetWriteSignature(),
				[this, &world, elapsedTime]() { moveSystem_.Execute(world, elapsedTime); });
			systemGraph_.Add(
				Query<Read<Spawner>, Read<Lifetime>>::GetReadSignature(),
				Query<Read<Spawner>, Read<Lifetime>>::GetWriteSignature(),
				[this, &world, elapsedTime]()
				{
					spawnerSystem_.Execute(commandBuffer_, world, elapsedTime);
					lifetimeSystem_.Execute(commandBuffer_, world, elapsedTime);
				});
			systemGraph_.Run(executor);
		}

		/// [EN] Flush the structural changes systems recorded (spawns, destroys) before TransformSystem, so this frame's world matrices already reflect them.
		/// [JP] システムが記録した構造変更(スポーン・破棄)を TransformSystem より前に flush し、今フレームのワールド行列に反映させる。
		commandBuffer_.Flush(world, cache);

		transformSystem_.Execute(world);

		if (isPlaying)
		{
			for (Actor actor : world.GetActors())
			{
				if (!actor.Active())
				{
					continue;
				}

				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
					if (component->tick_)
					{
						component->tick_(component, elapsedTime);
					}
				}
			}

			for (Actor actor : world.GetActors())
			{
				if (!actor.Active())
				{
					continue;
				}

				Entity entity = actor.GetEntity();
				EntityID entityID = entity.GetID();

				for (ComponentID id : actor.ComponentIDList())
				{
					void* data = world.GetComponent(entityID, id);
					if (!data)
					{
						continue;
					}

					ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
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
	* ComponentBehaviour-derived component of an active actor that
	* implements it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 固定タイムステップぶん1ステップ進める: アクティブな actor が持つ、
	* FixedTick を実装している全ての ComponentBehaviour 派生コンポーネント
	* へディスパッチする。
	*/
	void SystemScheduler::Step(World& world, Float fixedTime)
	{
		for (Actor actor : world.GetActors())
		{
			if (!actor.Active())
			{
				continue;
			}

			Entity entity = actor.GetEntity();
			EntityID entityID = entity.GetID();

			for (ComponentID id : actor.ComponentIDList())
			{
				void* data = world.GetComponent(entityID, id);
				if (!data)
				{
					continue;
				}

				ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
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
		commandBuffer_.Clear();
	}
}
