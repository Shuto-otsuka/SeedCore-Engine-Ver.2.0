#include <FoundationEngine/Time/WorldTimer.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances one frame by delta seconds, accumulating into total_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* delta 秒ぶん1フレーム進め、total_ に累積する。
	*/
	void WorldTimer::Tick(Float delta)
	{
		delta_ = delta;
		total_ += static_cast<Double>(delta);

		constexpr Float maxAccumulatedSteps = 8.0f;
		accumulator_ = Min(accumulator_ + delta, fixedDeltaTime_ * maxAccumulatedSteps);
	}

	/**
	* [EN]
	* Returns the delta time passed to the last Tick().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の Tick() に渡されたデルタタイムを返す。
	*/
	Float WorldTimer::DeltaTime()const
	{
		return delta_;
	}

	/**
	* [EN]
	* Returns the total time accumulated since engine start.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジン起動からの累積合計時間を返す。
	*/
	Float WorldTimer::TotalTime()const
	{
		return static_cast<Float>(total_);
	}

	/**
	* [EN]
	* Returns the fixed timestep used for physics/FixedTick stepping.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理/FixedTick のステップに使う固定タイムステップを返す。
	*/
	Float WorldTimer::FixedDeltaTime()const
	{
		return fixedDeltaTime_;
	}

	/**
	* [EN]
	* Sets the fixed timestep used for physics/FixedTick stepping.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理/FixedTick のステップに使う固定タイムステップを設定する。
	*/
	void WorldTimer::SetFixedDeltaTime(Float fixedDeltaTime)
	{
		fixedDeltaTime_ = fixedDeltaTime;
	}

	/**
	* [EN]
	* Consumes one FixedDeltaTime() worth of accumulated time, if enough
	* has built up since the last successful call.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前回の成功呼び出し以降に十分な時間が蓄積されていれば、
	* FixedDeltaTime() 分の蓄積時間を1回消費する。
	*/
	Bool WorldTimer::Step()
	{
		if (accumulator_ < fixedDeltaTime_)
		{
			return false;
		}

		accumulator_ -= fixedDeltaTime_;
		return true;
	}
}
