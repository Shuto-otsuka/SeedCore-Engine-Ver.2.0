#include <FoundationEngine/Time/Timer.h>

namespace SeedCore
{
	/**
	* [EN]
	* (Re)starts the timer, adding any time spent stopped to paused_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タイマーを（再）開始する。停止していた時間があれば paused_ に加算する。
	*/
	void Timer::Start()
	{
		if (stopped_)
		{
			paused_ += std::chrono::duration<Double>(std::chrono::high_resolution_clock::now() - stop_);
			stopped_ = false;
		}

		last_ = std::chrono::high_resolution_clock::now();
	}

	/**
	* [EN]
	* Stops the timer; elapsed time while stopped is excluded from Total().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タイマーを停止する。停止中の経過時間は Total() から除外される。
	*/
	void Timer::Stop()
	{
		if (!stopped_)
		{
			stop_ = std::chrono::high_resolution_clock::now();
			stopped_ = true;
		}
	}

	/**
	* [EN]
	* Resets the timer to its initial state (start time, delta, pause time).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タイマーを初期状態（開始時刻・デルタ・停止時間）にリセットする。
	*/
	void Timer::Reset()
	{
		start_ = std::chrono::high_resolution_clock::now();
		last_ = std::chrono::high_resolution_clock::now();

		paused_ = std::chrono::duration<Double>::zero();

		delta_ = 0.0f;
		stopped_ = false;
	}

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
	void Timer::Tick()
	{
		if (stopped_)
		{
			delta_ = 0.0f;
			return;
		}

		delta_ = std::chrono::duration<Double>(std::chrono::high_resolution_clock::now() - last_).count();
		last_ = std::chrono::high_resolution_clock::now();
	}

	/**
	* [EN]
	* Returns the delta time computed by the last Tick(), in seconds.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の Tick() で計算されたデルタタイムを秒単位で返す。
	*/
	Float Timer::Delta()const
	{
		return static_cast<Float>(delta_);
	}

	/**
	* [EN]
	* Returns the total elapsed time since Reset(), excluding stopped time.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Reset() からの合計経過時間を、停止時間を除いて秒単位で返す。
	*/
	Float Timer::Total()const
	{
		auto end = stopped_ ? stop_ : last_;
		return static_cast<Float>(std::chrono::duration<Double>(end - start_ - paused_).count());
	}
}