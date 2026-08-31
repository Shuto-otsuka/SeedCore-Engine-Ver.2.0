#include <FoundationEngine/ECS/System/SystemScheduler.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Velocity.h>
#include <FoundationEngine/ECS/Component/Spawner.h>
#include <FoundationEngine/ECS/Component/Lifetime.h>

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
	* world matrix), then drives Tick/LateTick (if isPlaying).
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
	* （isPlaying であれば）Tick/LateTick を駆動する。
	*/
	void SystemScheduler::Run(World& world, ResourceCache& cache, JobExecutor& executor, Float elapsedTime, Bool isPlaying)
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
		commandBuffer_.Clear();
	}
}
