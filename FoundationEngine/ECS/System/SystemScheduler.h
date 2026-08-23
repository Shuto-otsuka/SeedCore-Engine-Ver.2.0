#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/System/QuerySystem.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/System/SpawnerSystem.h>
#include <FoundationEngine/JobSystem/JobTask.h>

namespace SeedCore
{
	class World;
	class JobExecutor;
	class ResourceCache;

	/**
	* [EN]
	* Drives per-frame execution of every registered QuerySystem: splits
	* registered systems into archetype-driven vs sparse-driven groups,
	* builds a JobTaskflow per group with dependency edges inferred from
	* each system's read/write signatures (so systems with disjoint
	* access can run in parallel), runs both flows on a JobExecutor, and
	* also drives ComponentBase lifecycle callbacks (Awake/Start/Tick/
	* LateTick) and the built-in TransformSystem. FixedTick is dispatched
	* separately, through Step, since it runs at a fixed timestep and may
	* fire zero or several times per rendered frame rather than exactly
	* once like Run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録済みの全 QuerySystem の毎フレーム実行を統括するクラス:
	* 登録済みシステムをアーキタイプ駆動とスパース駆動のグループへ
	* 分割し、各システムの読み取り/書き込みシグネチャから推論された
	* 依存関係エッジを持つ JobTaskflow をグループごとに構築し
	* （アクセスが重ならないシステムは並列実行できる）、両方のフローを
	* JobExecutor 上で実行する。また、ComponentBase のライフサイクル
	* コールバック（Awake/Start/Tick/LateTick）と組み込みの
	* TransformSystem も駆動する。FixedTick は固定タイムステップで
	* 動作し、Run のように毎フレーム必ず1回ではなく0回や複数回
	* 発火し得るため、Step として別に配線する。
	*/
	class SEEDCORE_API SystemScheduler
	{
	public:
		/**
		* [EN]
		* Default constructor: starts with no registered systems.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 登録済みシステムが無い状態から開始する。
		*/
		SystemScheduler() = default;

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~SystemScheduler() = default;

		/**
		* [EN]
		* Constructs an instance of T and registers it, sorting it into
		* archetypeSystems_ or sparseSystems_ based on whether its query
		* touches any archetype-stored component.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T のインスタンスを構築して登録する。そのクエリがアーキタイプ
		* 格納コンポーネントに触れるかどうかに基づいて、archetypeSystems_
		* または sparseSystems_ へ振り分ける。
		*/
		template<typename T>
		void Register()
		{
			auto system = MakePtr<T>();

			Entry entry;
			entry.system_ = system.get();
			entry.execute_ = [](void* self, World& world, JobTaskflow& flow)
				{
					static_cast<T*>(self)->Execute(world, flow);
				};

			entry.readSignature_ = T::ReadSignature();
			entry.writeSignature_ = T::WriteSignature();

			if (T::Signature().any())
			{
				archetypeSystems_.push_back(entry);
			}
			else
			{
				sparseSystems_.push_back(entry);
			}
		}

		/**
		* [EN]
		* Runs one frame: drives Awake/Start (if isPlaying), builds and
		* runs the archetype/sparse system flows, drives MoveSystem then
		* SpawnerSystem (if isPlaying, both before TransformSystem so
		* this frame's motion and any newly spawned actor's position are
		* reflected in this same frame's world matrix), runs the
		* built-in TransformSystem, then drives Tick/LateTick (if
		* isPlaying). Shared by Runtime and Editor, so cache is threaded
		* through explicitly rather than assumed global.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を
		* 駆動し、アーキタイプ/スパースのシステムフローを構築・実行し、
		* （isPlaying であれば、TransformSystem より前に — 今フレームの
		* 移動や新しく生成された actor の位置が同じフレームのワールド
		* 行列に反映されるように）MoveSystem、続けて SpawnerSystem を
		* 駆動し、組み込みの TransformSystem を実行し、（isPlaying
		* であれば）Tick/LateTick を駆動する。Runtime と Editor の両方から
		* 使われるため、cache はグローバル前提にせず明示的に受け渡す。
		*/
		void Run(World& world, ResourceCache& cache, JobExecutor& executor, Float elapsedTime, Bool isPlaying = true);

		/**
		* [EN]
		* Advances one fixed timestep: dispatches FixedTick to every
		* ComponentBase-derived component that implements it. The caller
		* is expected to invoke this once per fixed timestep (e.g. from an
		* accumulator loop, alongside stepping the physics simulation by
		* the same fixedTime), independently of how many times Run fires
		* per rendered frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 固定タイムステップぶん1ステップ進める: FixedTick を実装している
		* 全ての ComponentBase 派生コンポーネントへディスパッチする。
		* 呼び出し側は、Run が1フレームに何回発火するかとは無関係に、
		* 固定タイムステップごとに（例えばアキュムレータループの中で、
		* 同じ fixedTime 分だけ物理シミュレーションをステップするのと
		* あわせて）1回ずつ呼び出すことを想定している。
		*/
		void Step(World& world, Float fixedTime);

		/**
		* [EN]
		* Forgets SpawnerSystem's runtime progress for every Spawner -
		* call this whenever the World was reset out from under this
		* scheduler (e.g. WorldSnapshot::Restore on Editor Stop), so
		* stale EntityIDs from the previous session aren't carried
		* forward into the next Play.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SpawnerSystem が持つ、全 Spawner のランタイム進行状況を忘れる -
		* この scheduler の下で World がリセットされた時(例: Editor の
		* Stop 時の WorldSnapshot::Restore)に呼ぶこと。前回セッションの
		* 古い EntityID が次の Play へ持ち越されないようにするため。
		*/
		void Reset();

	private:
		/**
		* [EN]
		* Type-erased registration record for a single QuerySystem:
		* holds its instance, a type-erased execute trampoline, and its
		* read/write archetype signatures for dependency analysis.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単一の QuerySystem に対する、型消去された登録レコード:
		* そのインスタンス、型消去された実行用トランポリン、および
		* 依存関係解析用の読み取り/書き込みアーキタイプシグネチャを
		* 保持する。
		*/
		struct Entry
		{
			/// [EN] Type-erased trampoline calling the concrete system's Execute(World&, JobTaskflow&).
			/// [JP] 具体的なシステムの Execute(World&, JobTaskflow&) を呼び出す、型消去されたトランポリン。
			void (*execute_)(void*, World&, JobTaskflow&);

			/// [EN] Type-erased pointer to the system instance.
			/// [JP] システムインスタンスへの型消去されたポインタ。
			void* system_;

			/// [EN] Archetype signature of the components this system reads.
			/// [JP] このシステムが読み取るコンポーネントのアーキタイプシグネチャ。
			Bitset readSignature_;

			/// [EN] Archetype signature of the components this system writes.
			/// [JP] このシステムが書き込むコンポーネントのアーキタイプシグネチャ。
			Bitset writeSignature_;
		};

	private:
		/**
		* [EN]
		* Emplaces one JobTask per entry in systems onto flow (each
		* invoking that system's execute_ trampoline), appending the
		* resulting tasks to tasks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* systems の各エントリに対して1つの JobTask を flow へ追加する
		* （それぞれがそのシステムの execute_ トランポリンを呼び出す）。
		* 結果として得られたタスクを tasks へ追加する。
		*/
		void BuildFlow(World& world, JobTaskflow& flow, DynamicArray<Entry>& systems, DynamicArray<JobTask>& tasks);

		/**
		* [EN]
		* Establishes precedence edges between tasks so that any pair of
		* systems with conflicting access (a write overlapping the
		* other's read or write) runs in registration order rather than concurrently.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アクセスが競合する（一方の書き込みが、もう一方の読み取りまたは
		* 書き込みと重なる）システムのペアが、並行実行ではなく登録順に
		* 実行されるよう、tasks 間に先行関係エッジを設定する。
		*/
		void ResolveDependencies(DynamicArray<Entry>& systems, DynamicArray<JobTask>& tasks);

	private:
		/// [EN] Built-in system that recomputes world-space transform matrices from local transforms and the parent hierarchy.
		/// [JP] ローカルトランスフォームと親階層から、ワールド空間変換行列を再計算する組み込みシステム。
		TransformSystem transformSystem_;

		/// [EN] Built-in system that integrates every Velocity-holding actor's Position each frame.
		/// [JP] 毎フレーム、Velocity を持つ全 actor の Position を積分する組み込みシステム。
		MoveSystem moveSystem_;

		/// [EN] Built-in system that drives every Spawner component's periodic prefab instantiation.
		/// [JP] 全 Spawner コンポーネントの周期的なプレハブ生成を駆動する組み込みシステム。
		SpawnerSystem spawnerSystem_;

		/// [EN] Registered systems whose query touches at least one archetype-stored component.
		/// [JP] クエリが少なくとも1つのアーキタイプ格納コンポーネントに触れる、登録済みシステム群。
		DynamicArray<Entry> archetypeSystems_;

		/// [EN] Registered systems whose query touches only sparse-set-stored components.
		/// [JP] クエリがスパースセット格納コンポーネントのみに触れる、登録済みシステム群。
		DynamicArray<Entry> sparseSystems_;

		/// [EN] JobTasks built for archetypeSystems_ this frame, reused across frames as scratch storage.
		/// [JP] このフレームで archetypeSystems_ 用に構築された JobTask 群。フレーム間で作業用ストレージとして再利用される。
		DynamicArray<JobTask> archetypeTasks_;

		/// [EN] JobTasks built for sparseSystems_ this frame, reused across frames as scratch storage.
		/// [JP] このフレームで sparseSystems_ 用に構築された JobTask 群。フレーム間で作業用ストレージとして再利用される。
		DynamicArray<JobTask> sparseTasks_;
	};
}
