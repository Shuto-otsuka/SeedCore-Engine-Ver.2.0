#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* The engine's master clock: accumulates real (unscaled, always-running)
	* delta and total time each frame. Gameplay-facing timers such as
	* GameTimer derive from its delta rather than measuring wall-clock time
	* themselves.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンのマスタークロック。実時間（タイムスケール未適用・常時進行）の
	* デルタと合計時間を毎フレーム累積する。GameTimer のようなゲームプレイ
	* 向けタイマーは、自ら実時間を計測するのではなく、このデルタを元に動作する。
	*/
	class SEEDCORE_API WorldTimer
	{
	public:
		/**
		* [EN]
		* Advances one frame by delta seconds, accumulating into total_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* delta 秒ぶん1フレーム進め、total_ に累積する。
		*/
		void Tick(Float delta);

		/**
		* [EN]
		* Returns the delta time passed to the last Tick().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Tick() に渡されたデルタタイムを返す。
		*/
		Float DeltaTime()const;

		/**
		* [EN]
		* Returns the total time accumulated since engine start.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンジン起動からの累積合計時間を返す。
		*/
		Float TotalTime()const;

		/**
		* [EN]
		* Returns the fixed timestep used for physics/FixedTick stepping.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理/FixedTick のステップに使う固定タイムステップを返す。
		*/
		Float FixedDeltaTime()const;

		/**
		* [EN]
		* Sets the fixed timestep used for physics/FixedTick stepping.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理/FixedTick のステップに使う固定タイムステップを設定する。
		*/
		void SetFixedDeltaTime(Float fixedDeltaTime);

		/**
		* [EN]
		* Consumes one FixedDeltaTime() worth of accumulated time, if
		* enough has built up since the last successful call. Intended to
		* be called in a loop after Tick() so the caller advances physics/
		* FixedTick exactly as many times as the accumulated real time
		* covers, regardless of the rendered frame rate.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の成功呼び出し以降に十分な時間が蓄積されていれば、
		* FixedDeltaTime() 分の蓄積時間を1回消費する。Tick() の後にループで
		* 呼ぶことを想定しており、描画フレームレートに関係なく、蓄積された
		* 実時間が賄う回数だけ呼び出し側が物理/FixedTick を進められるようにする。
		*/
		Bool Step();

	private:
		/// [EN] Delta time passed to the last Tick().
		/// [JP] 直近の Tick() に渡されたデルタタイム。
		Float delta_ = 0.0f;

		/// [EN] Total time accumulated since engine start.
		/// [JP] エンジン起動からの累積合計時間。
		Double total_ = 0.0;

		/// [EN] Fixed timestep used for physics/FixedTick stepping.
		/// [JP] 物理/FixedTick のステップに使う固定タイムステップ。
		Float fixedDeltaTime_ = 1.0f / 60.0f;

		/// [EN] Real time accumulated since the last successful Step(), consumed one FixedDeltaTime() at a time. Clamped in Tick() to avoid a spiral of death after a long stall.
		/// [JP] 直近の Step() 成功以降に蓄積された実時間。FixedDeltaTime() ずつ消費される。長時間の停止後にスパイラル・オブ・デスへ陥らないよう、Tick() 内でクランプされる。
		Float accumulator_ = 0.0f;
	};
}
