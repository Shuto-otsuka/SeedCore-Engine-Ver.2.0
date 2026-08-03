#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Gameplay clock derived from WorldTimer's delta, with play/pause/stop
	* state and an adjustable time scale (slow motion, fast forward, etc.).
	* Unlike WorldTimer, this clock can be paused independently of the
	* rest of the engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WorldTimer のデルタタイムを元にした、ゲームプレイ用クロック。
	* 再生/一時停止/停止の状態と、調整可能なタイムスケール（スローモーション、
	* 早送りなど）を持つ。WorldTimer と異なり、エンジンの他の部分とは
	* 独立に一時停止できる。
	*/
	class SEEDCORE_API GameTimer
	{
	public:
		/**
		* [EN]
		* Starts playing (or resumes from a paused state via Pause/Resume).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 再生を開始する（Pause/Resume による一時停止からの復帰は含まない）。
		*/
		void Play();

		/**
		* [EN]
		* Stops playback entirely, resetting both total-time accumulators.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 再生を完全に停止し、2つの合計時間の累積値をリセットする。
		*/
		void Stop();

		/**
		* [EN]
		* Pauses playback; Tick() becomes a no-op until Resume().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 再生を一時停止する。Resume() されるまで Tick() は何もしない。
		*/
		void Pause();

		/**
		* [EN]
		* Resumes playback after Pause().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Pause() の後、再生を再開する。
		*/
		void Resume();

		/**
		* [EN]
		* Advances one tick using worldDelta (typically WorldTimer::DeltaTime()).
		* Updates both the time-scaled and unscaled delta/total. No-op
		* while stopped or paused.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worldDelta（通常は WorldTimer::DeltaTime()）を用いて1ティック
		* 進める。タイムスケール適用後と未適用の両方のデルタ/合計を
		* 更新する。停止中または一時停止中は何もしない。
		*/
		void Tick(Float worldDelta);

		/**
		* [EN]
		* Returns the time-scaled delta time from the last Tick().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Tick() によるタイムスケール適用後のデルタタイムを返す。
		*/
		Float DeltaTime()const;

		/**
		* [EN]
		* Returns the unscaled delta time from the last Tick().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Tick() によるタイムスケール未適用のデルタタイムを返す。
		*/
		Float UnscaledDeltaTime()const;

		/**
		* [EN]
		* Returns the time-scaled total time accumulated since Play()/Stop().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Play()/Stop() 以降に累積された、タイムスケール適用後の合計時間を返す。
		*/
		Float TotalTime()const;

		/**
		* [EN]
		* Returns the unscaled total time accumulated since Play()/Stop().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Play()/Stop() 以降に累積された、タイムスケール未適用の合計時間を返す。
		*/
		Float UnscaledTotalTime()const;

		/**
		* [EN]
		* Returns the current time scale multiplier.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のタイムスケール倍率を返す。
		*/
		Float TimeScale()const;

		/**
		* [EN]
		* Sets the time scale multiplier applied to worldDelta in Tick().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Tick() で worldDelta に適用されるタイムスケール倍率を設定する。
		*/
		void SetTimeScale(Float scale);

		/**
		* [EN]
		* Returns whether the timer is currently playing (regardless of pause state).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイマーが現在再生中かどうかを返す（一時停止状態は問わない）。
		*/
		Bool IsPlaying()const;

		/**
		* [EN]
		* Returns whether the timer is currently paused.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイマーが現在一時停止中かどうかを返す。
		*/
		Bool IsPaused()const;

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

	private:
		/// [EN] Time-scaled delta time from the last Tick().
		/// [JP] 直近の Tick() によるタイムスケール適用後のデルタタイム。
		Float delta_ = 0.0f;

		/// [EN] Unscaled delta time from the last Tick().
		/// [JP] 直近の Tick() によるタイムスケール未適用のデルタタイム。
		Float unscaledDelta_ = 0.0f;

		/// [EN] Time-scaled total time accumulated since Play()/Stop().
		/// [JP] Play()/Stop() 以降に累積された、タイムスケール適用後の合計時間。
		Double scaledTotal_ = 0.0;

		/// [EN] Unscaled total time accumulated since Play()/Stop().
		/// [JP] Play()/Stop() 以降に累積された、タイムスケール未適用の合計時間。
		Double unscaledTotal_ = 0.0;

		/// [EN] Multiplier applied to worldDelta in Tick().
		/// [JP] Tick() で worldDelta に適用される倍率。
		Float timeScale_ = 1.0f;

		/// [EN] Whether the timer is currently playing.
		/// [JP] タイマーが現在再生中かどうか。
		Bool playing_ = false;

		/// [EN] Whether the timer is currently paused.
		/// [JP] タイマーが現在一時停止中かどうか。
		Bool paused_ = false;

		/// [EN] Fixed timestep used for physics/FixedTick stepping.
		/// [JP] 物理/FixedTick のステップに使う固定タイムステップ。
		Float fixedDeltaTime_ = 1.0f / 60.0f;
	};
}
