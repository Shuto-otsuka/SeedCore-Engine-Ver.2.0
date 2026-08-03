#include <GraphicsEngine/System/CelestialSystem.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] One keyframe of the day/night sky gradient.
		/// [JP] 昼夜の空グラデーションのキーフレーム1つ分。
		struct SkyGradientStop
		{
			Float elevationDegrees_;
			Color zenithColor_;
			Color horizonColor_;
		};

		/// [EN] Night -> twilight -> sunrise/sunset -> day, keyed by sun elevation.
		///      Independent of SunLightSettings (which colors the LIGHT hitting
		///      surfaces, not the sky background itself).
		/// [JP] 夜→薄明→朝焼け/夕焼け→昼、太陽の仰角でキー付け。SunLightSettings
		///      (物体に当たる光の色)とは独立 - こちらは空背景そのものの色。
		const SkyGradientStop skyGradientStops_[] =
		{
			{ -90.0f, Color(0.01f, 0.015f, 0.03f), Color(0.02f, 0.02f, 0.05f) },
			{ -12.0f, Color(0.01f, 0.015f, 0.03f), Color(0.02f, 0.02f, 0.05f) },
			{  -6.0f, Color(0.04f, 0.05f,  0.14f), Color(0.25f, 0.12f, 0.18f) },
			{  -1.0f, Color(0.10f, 0.16f,  0.38f), Color(0.95f, 0.42f, 0.20f) },
			{   4.0f, Color(0.14f, 0.28f,  0.60f), Color(0.85f, 0.55f, 0.35f) },
			{  15.0f, Color(0.15f, 0.35f,  0.75f), Color(0.65f, 0.75f, 0.90f) },
			{  90.0f, Color(0.15f, 0.35f,  0.75f), Color(0.65f, 0.75f, 0.90f) },
		};

		void ComputeSkyGradient(Float sunElevationDegrees, Color& outZenithColor, Color& outHorizonColor)
		{
			const Int stopCount = static_cast<Int>(std::size(skyGradientStops_));

			if (sunElevationDegrees <= skyGradientStops_[0].elevationDegrees_)
			{
				outZenithColor = skyGradientStops_[0].zenithColor_;
				outHorizonColor = skyGradientStops_[0].horizonColor_;
				return;
			}

			for (Int index = 0; index < stopCount - 1; index++)
			{
				const SkyGradientStop& from = skyGradientStops_[index];
				const SkyGradientStop& to = skyGradientStops_[index + 1];

				if (sunElevationDegrees <= to.elevationDegrees_)
				{
					Float span = Max(to.elevationDegrees_ - from.elevationDegrees_, 0.0001f);
					Float t = std::clamp((sunElevationDegrees - from.elevationDegrees_) / span, 0.0f, 1.0f);

					outZenithColor = Color::Lerp(from.zenithColor_, to.zenithColor_, t);
					outHorizonColor = Color::Lerp(from.horizonColor_, to.horizonColor_, t);
					return;
				}
			}

			outZenithColor = skyGradientStops_[stopCount - 1].zenithColor_;
			outHorizonColor = skyGradientStops_[stopCount - 1].horizonColor_;
		}
	}

	void CelestialSystem::Advance(Float deltaTime, DaySystemConstantBuffer& day)
	{
		if (day.paused_)
		{
			return;
		}

		Float minutesPerDay = Max(day.dayLengthMinutes_, 0.001f);
		Float hoursPerSecond = (24.0f / (minutesPerDay * 60.0f)) * day.timeScale_;

		day.hourOfDay_ += deltaTime * hoursPerSecond;

		while (day.hourOfDay_ >= 24.0f)
		{
			day.hourOfDay_ -= 24.0f;

			day.dayOfMonth_ += 1;
			if (day.dayOfMonth_ > day.daysPerMonth_)
			{
				day.dayOfMonth_ = 1;

				day.monthOfYear_ += 1;
				if (day.monthOfYear_ > 12)
				{
					day.monthOfYear_ = 1;
				}
			}
		}

		while (day.hourOfDay_ < 0.0f)
		{
			day.hourOfDay_ += 24.0f;
		}
	}

	CelestialResult CelestialSystem::Compute(const DaySystemConstantBuffer& day, const SunLightSettings& sun, const MoonLightSettings& moon)
	{
		CelestialResult result;

		/// [JP] 太陽の軌道角: 6時に地平線から昇り、18時に地平線へ沈む単純な円軌道。
		Float sunAngle = ((day.hourOfDay_ - 6.0f) / 24.0f) * DirectX::XM_2PI;
		Float sunElevation = std::sin(sunAngle);
		Float sunHorizontal = std::cos(sunAngle);

		Vector3 sunDirection = Vector3(sunHorizontal, sunElevation, 0.0f);
		sunDirection.Normalize();

		/// [JP] 光の進行方向(ライトが指す向き)は、太陽の方向と反対。
		result.sunDirection_ = -sunDirection;

		Float sunElevationDegrees = DirectX::XMConvertToDegrees(std::asin(std::clamp(sunElevation, -1.0f, 1.0f)));
		Float sunHeightFactor = std::clamp((sunElevationDegrees - sun.minElevationDegrees_) / (90.0f - sun.minElevationDegrees_), 0.0f, 1.0f);

		Color horizonColor(sun.horizonColor_[0], sun.horizonColor_[1], sun.horizonColor_[2], 1.0f);
		Color zenithColor(sun.zenithColor_[0], sun.zenithColor_[1], sun.zenithColor_[2], 1.0f);
		result.sunColor_ = Color::Lerp(horizonColor, zenithColor, std::clamp(sunElevationDegrees / 45.0f, 0.0f, 1.0f));
		result.sunIntensity_ = sun.maxIntensity_ * sunHeightFactor;

		/// [JP] 月は太陽の反対側(180度オフセット)。
		Vector3 moonDirection = -sunDirection;
		result.moonDirection_ = -moonDirection;

		Float moonElevation = moonDirection.y;
		Float moonHeightFactor = std::clamp(moonElevation, 0.0f, 1.0f);

		result.moonColor_ = Color(moon.color_[0], moon.color_[1], moon.color_[2], 1.0f);
		result.moonIntensity_ = moon.maxIntensity_ * moonHeightFactor;
		result.moonAngularRadius_ = moon.angularRadius_;

		/// [JP] 月齢: 月内の日数の進み具合をそのまま位相(0=新月, 0.5=満月, 1=新月)に写す。
		Float daysPerMonth = Max<Float>(static_cast<Float>(day.daysPerMonth_), 1.0f);
		Float monthProgress = (static_cast<Float>(day.dayOfMonth_ - 1) + (day.hourOfDay_ / 24.0f)) / daysPerMonth;
		result.moonPhase_ = std::fmod(monthProgress, 1.0f);

		/// [JP] 夜の強さ: sunHeightFactor(強度用、90度かけてなだらかに明るくなる)
		///      とは別に、星は薄明が終わってすぐ見えなくなるべきなので、
		///      minElevationDegrees_ から数度の間で急速にフェードさせる。
		Float starFadeDegrees = 8.0f;
		Float starHeightFactor = std::clamp((sunElevationDegrees - sun.minElevationDegrees_) / starFadeDegrees, 0.0f, 1.0f);
		result.nightFactor_ = 1.0f - starHeightFactor;

		Color skyZenith, skyHorizon;
		ComputeSkyGradient(sunElevationDegrees, skyZenith, skyHorizon);
		result.skyZenithColor_[0] = skyZenith.R(); result.skyZenithColor_[1] = skyZenith.G(); result.skyZenithColor_[2] = skyZenith.B();
		result.skyHorizonColor_[0] = skyHorizon.R(); result.skyHorizonColor_[1] = skyHorizon.G(); result.skyHorizonColor_[2] = skyHorizon.B();

		return result;
	}
}
