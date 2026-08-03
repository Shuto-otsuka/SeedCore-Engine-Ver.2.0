#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Wall-clock stopwatch built on std::chrono::high_resolution_clock.
	* Tracks per-tick delta time and total elapsed time, excluding any
	* time spent stopped.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* std::chrono::high_resolution_clock を用いた実時間ストップウォッチ。
	* 毎ティックのデルタタイムと、停止していた時間を除いた合計経過時間を
	* 追跡する。
	*/
	class SEEDCORE_API Timer
	{
	public:
		/**
		* [EN]
		* (Re)starts the timer, adding any time spent stopped to paused_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイマーを（再）開始する。停止していた時間があれば paused_ に加算する。
		*/
		void Start();

		/**
		* [EN]
		* Stops the timer; elapsed time while stopped is excluded from Total().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイマーを停止する。停止中の経過時間は Total() から除外される。
		*/
		void Stop();

		/**
		* [EN]
		* Resets the timer to its initial state (start time, delta, pause time).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイマーを初期状態（開始時刻・デルタ・停止時間）にリセットする。
		*/
		void Reset();

		/**
		* [EN]
		* Advances one tick, updating delta_ from the time since the
		* last Tick()/Start(). No-op (delta becomes 0) while stopped.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1ティック進め、前回の Tick()/Start() からの経過時間で
		* delta_ を更新する。停止中は何もしない（デルタは0になる）。
		*/
		void Tick();

		/**
		* [EN]
		* Returns the delta time computed by the last Tick(), in seconds.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Tick() で計算されたデルタタイムを秒単位で返す。
		*/
		Float Delta()const;

		/**
		* [EN]
		* Returns the total elapsed time since Reset(), excluding stopped time.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Reset() からの合計経過時間を、停止時間を除いて秒単位で返す。
		*/
		Float Total()const;

	private:
		/// [EN] Timestamp of the last Reset().
		/// [JP] 直近の Reset() のタイムスタンプ。
		std::chrono::high_resolution_clock::time_point start_;

		/// [EN] Timestamp of the last Tick()/Start(), used to compute delta_.
		/// [JP] 直近の Tick()/Start() のタイムスタンプ。delta_ の計算に使う。
		std::chrono::high_resolution_clock::time_point last_;

		/// [EN] Timestamp at which Stop() was called.
		/// [JP] Stop() が呼ばれた時刻。
		std::chrono::high_resolution_clock::time_point stop_;

		/// [EN] Cumulative time spent stopped, subtracted from Total().
		/// [JP] 停止していた累積時間。Total() から差し引かれる。
		std::chrono::duration<Double> paused_ = std::chrono::duration<Double>::zero();

		/// [EN] Delta time computed by the last Tick(), in seconds.
		/// [JP] 直近の Tick() で計算されたデルタタイム（秒）。
		Double delta_ = 0.0f;

		/// [EN] Whether the timer is currently stopped.
		/// [JP] タイマーが現在停止中かどうか。
		Bool stopped_ = false;
	};
}