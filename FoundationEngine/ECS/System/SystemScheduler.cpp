#include <FoundationEngine/ECS/System/SystemScheduler.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Runs one frame: drives Awake/Start (if isPlaying), builds and runs
	* the archetype/sparse system flows, runs the built-in TransformSystem,
	* then drives Tick/LateTick (if isPlaying).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を
	* 駆動し、アーキタイプ/スパースのシステムフローを構築・実行し、
	* 組み込みの TransformSystem を実行し、（isPlaying であれば）
	* Tick/LateTick を駆動する。
	*/
	void SystemScheduler::Run(World& world, JobExecutor& executor, Float elapsedTime, Bool isPlaying)
	{
		if (isPlaying)
		{
			/// [EN] Fire Awake/Start exactly once per component, in that order, before any Tick runs.
			/// [JP] Tick が実行される前に、Awake/Start をコンポーネントごとにちょうど1回、その順序で発火する。
			for (auto& actor : world.GetActors())
			{
				Entity entity = actor->GetEntity();
				EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

				for (ComponentID id : actor->ComponentBaseIDList())
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

		JobTaskflow archetypeFlow;
		JobTaskflow sparseFlow;

		archetypeTasks_.clear();
		sparseTasks_.clear();

		BuildFlow(world, archetypeFlow, archetypeSystems_, archetypeTasks_);
		BuildFlow(world, sparseFlow, sparseSystems_, sparseTasks_);

		ResolveDependencies(archetypeSystems_, archetypeTasks_);
		ResolveDependencies(sparseSystems_, sparseTasks_);

		executor.Run(archetypeFlow).wait();
		executor.Run(sparseFlow).wait();

		transformSystem_.Execute(world);

		if (isPlaying)
		{
			for (auto& actor : world.GetActors())
			{
				Entity entity = actor->GetEntity();
				EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

				for (ComponentID id : actor->ComponentBaseIDList())
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

			for (auto& actor : world.GetActors())
			{
				Entity entity = actor->GetEntity();
				EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

				for (ComponentID id : actor->ComponentBaseIDList())
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
		for (auto& actor : world.GetActors())
		{
			Entity entity = actor->GetEntity();
			EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

			for (ComponentID id : actor->ComponentBaseIDList())
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
	* Emplaces one JobTask per entry in systems onto flow (each invoking
	* that system's execute_ trampoline), appending the resulting tasks
	* to tasks.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* systems の各エントリに対して1つの JobTask を flow へ追加する
	* （それぞれがそのシステムの execute_ トランポリンを呼び出す）。
	* 結果として得られたタスクを tasks へ追加する。
	*/
	void SystemScheduler::BuildFlow(World& world, JobTaskflow& flow, DynamicArray<Entry>& systems, DynamicArray<JobTask>& tasks)
	{
		for (Size index = 0; index < systems.size(); ++index)
		{
			JobTask task = flow.emplace([&, index]()
				{
					systems[index].execute_(systems[index].system_, world, flow);
				});

			tasks.push_back(task);
		}
	}

	/**
	* [EN]
	* Establishes precedence edges between tasks so that any pair of
	* systems with conflicting access (a write overlapping the other's
	* read or write) runs in registration order rather than concurrently.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクセスが競合する（一方の書き込みが、もう一方の読み取りまたは
	* 書き込みと重なる）システムのペアが、並行実行ではなく登録順に
	* 実行されるよう、tasks 間に先行関係エッジを設定する。
	*/
	void SystemScheduler::ResolveDependencies(DynamicArray<Entry>& systems, DynamicArray<JobTask>& tasks)
	{
		for (Size index = 0; index < systems.size(); ++index)
		{
			for (Size otherIndex = index + 1; otherIndex < systems.size(); ++otherIndex)
			{
				Bool conflict =
					(systems[index].writeSignature_ & systems[otherIndex].readSignature_).any() ||
					(systems[index].writeSignature_ & systems[otherIndex].writeSignature_).any() ||
					(systems[otherIndex].writeSignature_ & systems[index].readSignature_).any();

				if (conflict)
				{
					tasks[otherIndex].Succeed(tasks[index]);
				}
			}
		}
	}
}
