#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/System/SpawnerSystem.h>
#include <FoundationEngine/ECS/System/LifetimeSystem.h>
#include <FoundationEngine/ECS/System/SystemGraph.h>
#include <FoundationEngine/ECS/CommandBuffer.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	class JobExecutor;

	/**
	* [EN]
	* Drives the built-in per-frame simulation step: dispatches the
	* ComponentBase lifecycle callbacks (Awake/Start via Run, Tick/
	* LateTick via Run, FixedTick via Step) and runs the built-in
	* MoveSystem plus the structural systems (Spawner, Lifetime) through
	* a SystemGraph - so non-conflicting ones run in parallel on a
	* JobExecutor - then the built-in TransformSystem. FixedTick is
	* dispatched separately, through Step, since it
	* runs at a fixed timestep and may fire zero or several times per
	* rendered frame rather than exactly once like Run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 組み込みの毎フレームシミュレーションステップを駆動するクラス:
	* ComponentBase のライフサイクルコールバック（Awake/Start は Run、
	* Tick/LateTick も Run、FixedTick は Step）をディスパッチし、組み込みの
	* MoveSystem と構造系システム（Spawner、Lifetime）を SystemGraph 経由で
	* 実行し（衝突しないものは JobExecutor 上で並列に走る）、続けて
	* 組み込みの TransformSystem を実行する。FixedTick は固定タイムステップで
	* 動作し、Run のように
	* 毎フレーム必ず1回ではなく0回や複数回発火し得るため、Step として
	* 別に配線する。
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
		* Runs one frame: drives Awake/Start (if isPlaying), then (if
		* isPlaying) runs MoveSystem and the structural systems (Spawner +
		* Lifetime) through a SystemGraph on executor - MoveSystem in
		* parallel with the structural pair - flushes the recorded
		* structural changes, and runs the built-in TransformSystem (all
		* before Tick/LateTick so this frame's motion and any newly
		* spawned actor's position are in this frame's world matrix), then
		* drives Tick/LateTick (if isPlaying). Shared by Runtime and
		* Editor, so cache and executor are threaded through explicitly
		* rather than assumed global.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を駆動し、
		* （isPlaying であれば）MoveSystem と構造系システム（Spawner +
		* Lifetime）を SystemGraph 経由で executor 上で実行し（MoveSystem は
		* 構造系ペアと並列）、記録された構造変更を flush し、組み込みの
		* TransformSystem を実行する（すべて Tick/LateTick より前 — 今フレーム
		* の移動や新しく生成された actor の位置が同じフレームのワールド行列に
		* 入るように）。その後（isPlaying であれば）Tick/LateTick を駆動する。
		* Runtime と Editor の両方から使われるため、cache と executor は
		* グローバル前提にせず明示的に受け渡す。
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
		/// [EN] Built-in system that recomputes world-space transform matrices from local transforms and the parent hierarchy.
		/// [JP] ローカルトランスフォームと親階層から、ワールド空間変換行列を再計算する組み込みシステム。
		TransformSystem transformSystem_;

		/// [EN] Built-in system that integrates every Velocity-holding actor's Position each frame.
		/// [JP] 毎フレーム、Velocity を持つ全 actor の Position を積分する組み込みシステム。
		MoveSystem moveSystem_;

		/// [EN] Built-in system that drives every Spawner component's periodic prefab instantiation.
		/// [JP] 全 Spawner コンポーネントの周期的なプレハブ生成を駆動する組み込みシステム。
		SpawnerSystem spawnerSystem_;

		/// [EN] Built-in system that counts down every Lifetime component and records its owning actor's destruction once it hits zero.
		/// [JP] 全 Lifetime コンポーネントをカウントダウンし、0 に達した時点で所有アクターの破棄を記録する組み込みシステム。
		LifetimeSystem lifetimeSystem_;

		/// [EN] Collects the structural World changes SpawnerSystem/LifetimeSystem record each frame; flushed between the system-update block and TransformSystem.
		/// [JP] SpawnerSystem/LifetimeSystem が毎フレーム記録する World の構造変更を集める。システム更新ブロックと TransformSystem の間で flush される。
		CommandBuffer commandBuffer_;

		/// [EN] Schedules the pre-TransformSystem systems by their component access, running non-conflicting ones in parallel on the frame's JobExecutor; rebuilt each Run.
		/// [JP] TransformSystem 前のシステムをコンポーネントアクセスに基づいてスケジュールし、衝突しないものをそのフレームの JobExecutor 上で並列に走らせる。Run ごとに再構築する。
		SystemGraph systemGraph_;
	};
}
