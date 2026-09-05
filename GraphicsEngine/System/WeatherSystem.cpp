#include <GraphicsEngine/System/WeatherSystem.h>
#include <GraphicsEngine/Environment/Weather.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	namespace
	{
		Float Lerp(Float from, Float to, Float t)
		{
			return from + (to - from) * t;
		}

		/// [JP] 雨の季節重み: 5〜7月がピーク、それ以外の月も0にはしない(毎月降る)。
		Float RainSeasonalWeight(Uint32 monthOfYear)
		{
			if (monthOfYear == 5 || monthOfYear == 6 || monthOfYear == 7)
			{
				return 2.5f;
			}
			return 1.0f;
		}

		Bool IsSnowSeason(Uint32 monthOfYear)
		{
			return monthOfYear == 12 || monthOfYear == 1 || monthOfYear == 2;
		}
	}

	void WeatherSystem::GetTarget(WeatherType type, VolumetricCloudScapesRayConstantBuffer& target)
	{
		/// [EN] Deliberately does NOT touch skyZenithColor_/skyHorizonColor_:
		///      those are CelestialSystem's time-of-day gradient (see
		///      Engine.cpp), written every frame BEFORE this runs. If weather
		///      set them to fixed absolute colors, a night sky under Clear
		///      weather would get dragged back toward a fixed "daytime blue"
		///      here, and any progress made by the lerp below would be wiped
		///      out again next frame when Celestial rewrites them - weather
		///      would never visibly change the sky. skyBrightness_ (and
		///      coverage_/rain_, which darken the actual rendered clouds) are
		///      pure multipliers/amounts with no such conflict, so they compose
		///      correctly with the time-of-day color regardless of write order.
		/// [JP] 意図的に skyZenithColor_/skyHorizonColor_ には触れない:
		///      それらは CelestialSystem の時刻グラデーション(Engine.cpp 参照)
		///      であり、この関数が呼ばれる前に毎フレーム書き込まれる。もし
		///      天候側が固定の絶対色を設定すると、Clear天候の夜空がここで
		///      固定の「昼の青」へ引き戻され、下の補間が1フレーム分進んでも
		///      次のフレームで Celestial に再び上書きされて消えてしまう -
		///      天候が空の色に見た目上まったく反映されなくなる。
		///      skyBrightness_(と、実際に描かれる雲を暗くする coverage_/
		///      rain_)は純粋な乗数/量なので、書き込み順序に関係なく時刻の色と
		///      正しく合成される。
		switch (type)
		{
		case WeatherType::Clear:
			target.coverage_ = 0.15f;
			target.densityScale_ = 0.02f;
			target.rain_ = 0.0f;
			target.windSpeed_ = 0.01f;
			target.skyBrightness_ = 1.0f;
			break;

		case WeatherType::Cloudy:
			target.coverage_ = 0.45f;
			target.densityScale_ = 0.02f;
			target.rain_ = 0.0f;
			target.windSpeed_ = 0.02f;
			target.skyBrightness_ = 0.9f;
			break;

		case WeatherType::Overcast:
			target.coverage_ = 0.85f;
			target.densityScale_ = 0.03f;
			target.rain_ = 0.0f;
			target.windSpeed_ = 0.03f;
			target.skyBrightness_ = 0.6f;
			break;

		case WeatherType::Rain:
			target.coverage_ = 0.9f;
			target.densityScale_ = 0.035f;
			target.rain_ = 0.7f;
			target.windSpeed_ = 0.05f;
			target.skyBrightness_ = 0.45f;
			break;

		case WeatherType::Snow:
			target.coverage_ = 0.8f;
			target.densityScale_ = 0.03f;
			target.rain_ = 0.0f;
			target.windSpeed_ = 0.02f;
			target.skyBrightness_ = 0.55f;
			break;

		case WeatherType::Storm:
			target.coverage_ = 1.0f;
			target.densityScale_ = 0.045f;
			target.rain_ = 1.0f;
			target.windSpeed_ = 0.09f;
			target.skyBrightness_ = 0.3f;
			break;
		}
	}

	Float WeatherSystem::RandomRange(Float minValue, Float maxValue)
	{
		std::uniform_real_distribution<Float> distribution(minValue, maxValue);
		return distribution(randomEngine_);
	}

	WeatherType WeatherSystem::PickRandomType(Uint32 monthOfYear, const Weather& weather)
	{
		struct Entry
		{
			WeatherType type_;
			Float weight_;
		};

		/// [JP] forceXxx_ が true ならその天候の季節重みを無視して一定の重みにする。
		Entry entries[] =
		{
			{ WeatherType::Clear, 3.0f },
			{ WeatherType::Cloudy, 2.0f },
			{ WeatherType::Overcast, 1.0f },
			{ WeatherType::Rain, weather.forceRain_ ? 2.0f : RainSeasonalWeight(monthOfYear) },
			{ WeatherType::Snow, weather.forceSnow_ ? 1.5f : (IsSnowSeason(monthOfYear) ? 1.5f : 0.0f) },
			{ WeatherType::Storm, weather.forceStorm_ ? 1.0f : 0.15f },
		};

		Float totalWeight = 0.0f;
		for (const Entry& entry : entries)
		{
			totalWeight += entry.weight_;
		}

		Float roll = RandomRange(0.0f, Max(totalWeight, 0.0001f));
		Float cumulative = 0.0f;
		for (const Entry& entry : entries)
		{
			cumulative += entry.weight_;
			if (roll <= cumulative)
			{
				return entry.type_;
			}
		}

		return WeatherType::Clear;
	}

	void WeatherSystem::Execute(World& world, Float deltaTime, Uint32 monthOfYear, VolumetricCloudScapesRayConstantBuffer& cloudSettings)
	{
		Query<Write<Weather>> query(world);

		Bool found = false;

		query.ForEach([&](Weather& weather)
			{
				if (found)
				{
					return;
				}
				found = true;

				if (weather.autoCycle_)
				{
					weather.autoCycleTimer_ -= deltaTime;
					if (weather.autoCycleTimer_ <= 0.0f)
					{
						std::uniform_real_distribution<Float> intervalDistribution(weather.minIntervalMinutes_, weather.maxIntervalMinutes_);
						weather.autoCycleTimer_ = intervalDistribution(randomEngine_) * 60.0f;

						weather.type_ = PickRandomType(monthOfYear, weather);
					}
				}

				VolumetricCloudScapesRayConstantBuffer target = cloudSettings;
				GetTarget(weather.type_, target);

				Float t = weather.transitionSeconds_ > 0.0f ? std::clamp(deltaTime / weather.transitionSeconds_, 0.0f, 1.0f) : 1.0f;

				cloudSettings.coverage_ = Lerp(cloudSettings.coverage_, target.coverage_, t);
				cloudSettings.densityScale_ = Lerp(cloudSettings.densityScale_, target.densityScale_, t);
				cloudSettings.rain_ = Lerp(cloudSettings.rain_, target.rain_, t);
				cloudSettings.windSpeed_ = Lerp(cloudSettings.windSpeed_, target.windSpeed_, t);
				cloudSettings.skyBrightness_ = Lerp(cloudSettings.skyBrightness_, target.skyBrightness_, t);

				/// [JP] 濡れ具合: 雨/暴風(Storm)の間に上がり、止んだ後もゆっくり乾く。
				Bool raining = weather.type_ == WeatherType::Rain || weather.type_ == WeatherType::Storm;
				Float wetnessRampSpeed = 1.0f / 20.0f;
				Float wetnessDecaySpeed = 1.0f / 90.0f;
				weather.wetness_ += deltaTime * (raining ? wetnessRampSpeed : -wetnessDecaySpeed);
				weather.wetness_ = std::clamp(weather.wetness_, 0.0f, 1.0f);

				/// [JP] 積雪: 雪の間に積もり、止むと雪解け(季節外の強制の場合は速く溶ける)。
				Bool snowing = weather.type_ == WeatherType::Snow;
				Float snowRampSpeed = 1.0f / 60.0f;
				Float snowMeltSpeed = IsSnowSeason(monthOfYear) ? (1.0f / 180.0f) : (1.0f / 40.0f);
				weather.snowCoverage_ += deltaTime * (snowing ? snowRampSpeed : -snowMeltSpeed);
				weather.snowCoverage_ = std::clamp(weather.snowCoverage_, 0.0f, 1.0f);

				/// [JP] snowIntensity_: 「今降っているか」を素早く増減させ、画面の
				///      降雪オーバーレイ(見た目)を駆動する。地面の積雪
				///      (snowCoverage_)とは別の、速い方の信号。
				const Float snowIntensityRate = 1.0f / 2.0f;
				weather.snowIntensity_ += deltaTime * (snowing ? snowIntensityRate : -snowIntensityRate);
				weather.snowIntensity_ = std::clamp(weather.snowIntensity_, 0.0f, 1.0f);

				/// [JP] 雷: 天候ごとの1秒あたりの発生確率(ShootingStar と同じ
				///      「chancePerSecond」方式)。Storm=0.005、それ以外=0
				///      (Rainでは鳴らさない)。forceThunder_ が立っていれば
				///      それ以外の天候でも Rain 相当の確率で許可する。
				Float thunderChancePerSecond = 0.0f;
				if (weather.type_ == WeatherType::Storm)
				{
					thunderChancePerSecond = 0.005f;
				}

				if (weather.forceThunder_)
				{
					thunderChancePerSecond = Max(thunderChancePerSecond, 0.05f);
				}

				if (thunderChancePerSecond > 0.0f && RandomRange(0.0f, 1.0f) < thunderChancePerSecond * deltaTime)
				{
					weather.thunderFlash_ = 1.0f;
					weather.thunderSeed_ = RandomRange(0.0f, 1000.0f);
				}

				const Float thunderDecayRate = 4.0f;
				weather.thunderFlash_ = Max(weather.thunderFlash_ - deltaTime * thunderDecayRate, 0.0f);
			});
	}

	WeatherGpuState WeatherSystem::ReadGpuState(World& world)
	{
		WeatherGpuState state;

		Query<Read<Weather>> query(world);
		Bool found = false;

		query.ForEach([&](const Weather& weather)
			{
				if (found)
				{
					return;
				}
				found = true;

				state.wetness_ = weather.wetness_;
				state.snowCoverage_ = weather.snowCoverage_;
				state.thunderFlash_ = weather.thunderFlash_;
				state.snowIntensity_ = weather.snowIntensity_;
				state.thunderSeed_ = weather.thunderSeed_;
				state.rainEnabled_ = weather.rainEnabled_;
				state.rain_ = weather.rain_;
				state.snowEnabled_ = weather.snowEnabled_;
				state.snow_ = weather.snow_;
			});

		return state;
	}
}
