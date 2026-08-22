#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] The day/night clock: current time of day and calendar date. CPU-only
	///      - never uploaded to the GPU directly, only the direction/color it
	///      derives (via CelestialSystem::Compute) reach the shaders through
	///      LightConstantData. Mirrors the *RayConstantBuffer structs' shape
	///      (default-constructed values, a Serialize() hook) even though it has
	///      no HLSL counterpart.
	/// [JP] 昼夜の時計: 現在時刻と暦日。CPU 専用 - GPU へ直接アップロードせず、
	///      そこから導出される方向/色(CelestialSystem::Compute 経由)だけが
	///      LightConstantData を通してシェーダへ届く。HLSL 側の対応は無いが、
	///      他の *RayConstantBuffer 構造体と同じ形(デフォルト値、Serialize()
	///      フック)にしてある。
	struct DaySystemConstantBuffer
	{
		/// [EN] Current time of day, 0-24. Advanced each frame by CelestialSystem::Advance.
		/// [JP] 現在時刻、0-24。CelestialSystem::Advance が毎フレーム進める。
		Float hourOfDay_ = 8.0f;

		/// [EN] Current day within the month, 1-daysPerMonth_.
		/// [JP] 月内の現在日、1-daysPerMonth_。
		Uint32 dayOfMonth_ = 1;

		/// [EN] Days per calendar month (the month cycle length).
		/// [JP] 1か月あたりの日数(暦のサイクル長)。
		Uint32 daysPerMonth_ = 30;

		/// [EN] Month of year, 1-12. Advances when dayOfMonth_ wraps, wrapping
		///      itself 12->1. Drives WeatherSystem's seasonal weighting
		///      (e.g. snow only in Dec-Feb).
		/// [JP] 年内の月、1-12。dayOfMonth_ が一周する時に進み、12から1へ
		///      戻る。WeatherSystem の季節による重み付け(例: 雪は12〜2月のみ)
		///      を駆動する。
		Uint32 monthOfYear_ = 1;

		/// [EN] Real-world minutes for one full 24h in-game day.
		/// [JP] ゲーム内24時間ぶんに相当する現実時間の分数。
		Float dayLengthMinutes_ = 20.0f;

		/// [EN] Multiplier applied on top of dayLengthMinutes_.
		/// [JP] dayLengthMinutes_ にさらに掛かる倍率。
		Float timeScale_ = 1.0f;

		/// [EN] Freezes hourOfDay_/dayOfMonth_ when true.
		/// [JP] true の間 hourOfDay_/dayOfMonth_ を止める。
		Bool paused_ = false;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("hourOfDay", hourOfDay_);
			archive.TryField("dayOfMonth", dayOfMonth_);
			archive.TryField("daysPerMonth", daysPerMonth_);
			archive.TryField("monthOfYear", monthOfYear_);
			archive.TryField("dayLengthMinutes", dayLengthMinutes_);
			archive.TryField("timeScale", timeScale_);
			archive.TryField("paused", paused_);
		}
	};

	/// [EN] Tuning for the sun light/disc driven by the DaySystem clock.
	/// [JP] DaySystem の時計から駆動される太陽ライト/ディスクの調整値。
	struct SunLightSettings
	{
		Float horizonColor_[3] = { 1.0f, 0.55f, 0.25f };
		Float zenithColor_[3] = { 1.0f, 0.98f, 0.92f };

		Float maxIntensity_ = 3.0f;

		/// [EN] Below this elevation (degrees) the sun contributes zero light.
		/// [JP] この仰角(度)未満では太陽の光量は0になる。
		Float minElevationDegrees_ = -6.0f;

		/// [EN] Angular radius of the sun disc.
		/// [JP] 太陽ディスクの視半径。
		Float angularRadius_ = 0.03f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("horizonColor", horizonColor_);
			archive.TryField("zenithColor", zenithColor_);
			archive.TryField("maxIntensity", maxIntensity_);
			archive.TryField("minElevationDegrees", minElevationDegrees_);
			archive.TryField("angularRadius", angularRadius_);
		}
	};

	/// [EN] Tuning for the moon light/disc driven by the DaySystem clock.
	/// [JP] DaySystem の時計から駆動される月ライト/ディスクの調整値。
	struct MoonLightSettings
	{
		Float color_[3] = { 0.65f, 0.72f, 0.85f };
		Float maxIntensity_ = 0.15f;
		Float angularRadius_ = 0.02f;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("color", color_);
			archive.TryField("maxIntensity", maxIntensity_);
			archive.TryField("angularRadius", angularRadius_);
		}
	};

	/// [EN] Output of CelestialSystem::Compute: everything downstream (LightSystem,
	///      VolumetricStarRenderer) needs for this frame's sun/moon/night state.
	/// [JP] CelestialSystem::Compute の出力: 下流(LightSystem、
	///      VolumetricStarRenderer)がこのフレームの太陽/月/夜状態のために必要な
	///      全てを持つ。
	struct CelestialResult
	{
		Vector3 sunDirection_ = { 0.0f, -1.0f, 0.0f };
		Color sunColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		Float sunIntensity_ = 0.0f;

		Vector3 moonDirection_ = { 0.0f, 1.0f, 0.0f };
		Color moonColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		Float moonIntensity_ = 0.0f;

		/// [EN] 0 = new moon, 0.5 = full moon, 1 = new moon again.
		/// [JP] 0=新月、0.5=満月、1=次の新月。
		Float moonPhase_ = 0.0f;

		Float moonAngularRadius_ = 0.02f;

		/// [EN] 0 = full day, 1 = full night. Drives star visibility/shooting-star chance.
		/// [JP] 0=完全な昼、1=完全な夜。星の見え方/流れ星の発生確率を駆動する。
		Float nightFactor_ = 0.0f;

		/// [EN] Realistic time-of-day background sky gradient (night -> twilight ->
		///      sunrise/sunset -> day), independent of sunLight_'s light color.
		///      Meant to drive VolumetricCloudScapesRayConstantBuffer's
		///      skyZenithColor_/skyHorizonColor_.
		/// [JP] 現実的な時刻依存の空グラデーション(夜→薄明→朝焼け/夕焼け→昼)。
		///      sunLight_ の光源色とは独立。VolumetricCloudScapesRayConstantBuffer
		///      の skyZenithColor_/skyHorizonColor_ を駆動する想定。
		Float skyZenithColor_[3] = { 0.15f, 0.35f, 0.75f };
		Float skyHorizonColor_[3] = { 0.65f, 0.75f, 0.9f };
	};

	/// [EN]
	/// Pure math driving the day/night cycle: no GPU resources, no ECS. Advance()
	/// moves the clock forward in real time; Compute() derives this frame's sun/
	/// moon direction, color and intensity from the clock plus tuning. Called
	/// from Editor::Engine (Advance, once before RaytracingContext is pushed to
	/// the renderer) and Renderer::Gather (Compute, right before LightSystem::
	/// Gather so the result can override the scene's DirectionalLight).
	///
	/// ---------------------------------------------------------------------
	///
	/// [JP]
	/// 昼夜サイクルを駆動する純粋な計算: GPU リソースも ECS も持たない。
	/// Advance() は時計を実時間で進め、Compute() は時計とチューニングから
	/// このフレームの太陽/月の方向・色・強度を導出する。Editor::Engine
	/// (Advance、RaytracingContext をレンダラへ渡す前に1回)と
	/// Renderer::Gather(Compute、LightSystem::Gather の直前 - 結果でシーンの
	/// DirectionalLight を上書きできるように)から呼ばれる。
	class SEEDCORE_API CelestialSystem
	{
	public:
		static void Advance(Float deltaTime, DaySystemConstantBuffer& day);

		[[nodiscard]] static CelestialResult Compute(const DaySystemConstantBuffer& day, const SunLightSettings& sun, const MoonLightSettings& moon);
	};
}
