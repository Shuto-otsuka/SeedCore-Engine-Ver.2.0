#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/System/SpawnerSystem.h>
#include <FoundationEngine/ECS/System/LifetimeSystem.h>

namespace SeedCore
{
	class World;
	class ResourceCache;

	/**
	* [EN]
	* Drives the built-in per-frame simulation step: dispatches the
	* ComponentBase lifecycle callbacks (Awake/Start via Run, Tick/
	* LateTick via Run, FixedTick via Step) and runs the built-in
	* MoveSystem, SpawnerSystem, LifetimeSystem and TransformSystem in
	* order. FixedTick is dispatched separately, through Step, since it
	* runs at a fixed timestep and may fire zero or several times per
	* rendered frame rather than exactly once like Run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 組み込みの毎フレームシミュレーションステップを駆動するクラス:
	* ComponentBase のライフサイクルコールバック（Awake/Start は Run、
	* Tick/LateTick も Run、FixedTick は Step）をディスパッチし、組み込みの
	* MoveSystem・SpawnerSystem・LifetimeSystem・TransformSystem を順に
	* 実行する。FixedTick は固定タイムステップで動作し、Run のように
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
		* Runs one frame: drives Awake/Start (if isPlaying), drives
		* MoveSystem then SpawnerSystem then LifetimeSystem (if isPlaying,
		* all before TransformSystem so this frame's motion and any newly
		* spawned actor's position are reflected in this same frame's
		* world matrix), runs the built-in TransformSystem, then drives
		* Tick/LateTick (if isPlaying). Shared by Runtime and Editor, so
		* cache is threaded through explicitly rather than assumed global.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1フレーム分を実行する: （isPlaying であれば）Awake/Start を
		* 駆動し、（isPlaying であれば、TransformSystem より前に — 今フレーム
		* の移動や新しく生成された actor の位置が同じフレームのワールド
		* 行列に反映されるように）MoveSystem、続けて SpawnerSystem、
		* LifetimeSystem を駆動し、組み込みの TransformSystem を実行し、
		* （isPlaying であれば）Tick/LateTick を駆動する。Runtime と Editor の
		* 両方から使われるため、cache はグローバル前提にせず明示的に受け渡す。
		*/
		void Run(World& world, ResourceCache& cache, Float elapsedTime, Bool isPlaying = true);

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

		LifetimeSystem lifetimeSystem_;
	};
}
