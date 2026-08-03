#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/// [EN] Weather state driven by WeatherSystem::Execute. Storm doubles as
	///      "暴風" (gale) - heavy rain plus very high wind, see
	///      WeatherSystem::GetTarget.
	/// [JP] WeatherSystem::Execute が駆動する天候の状態。Storm は「暴風」も
	///      兼ねる - 大雨+強風、WeatherSystem::GetTarget 参照。
	enum class WeatherType
	{
		Clear,
		Cloudy,
		Overcast,
		Rain,
		Snow,
		Storm,
	};

	/// [EN]
	/// Singleton-by-convention component: add to exactly one Actor in the scene.
	/// WeatherSystem finds it via a Query<Write<Weather>> and interpolates
	/// VolumetricCloudScapesRayConstantBuffer's existing fields toward the
	/// current type_'s target values, and drives wetness_/snowCoverage_/
	/// thunder from the runtime fields below. No Weather component in the
	/// scene means no interference with manual cloud tuning.
	///
	/// ---------------------------------------------------------------------
	///
	/// [JP]
	/// singleton的に運用するコンポーネント: シーン内のちょうど1体の Actor に
	/// 付ける。WeatherSystem が Query<Write<Weather>> 経由で見つけ、
	/// VolumetricCloudScapesRayConstantBuffer の既存フィールドを現在の type_
	/// の目標値へ補間し、以下の実行時フィールドから wetness_/snowCoverage_/
	/// 雷を駆動する。シーンに Weather コンポーネントが無ければ手動の雲
	/// チューニングに一切干渉しない。
	struct Weather
	{
		SC_REFLECTION_FIELD_EX("天候")
		WeatherType type_ = WeatherType::Clear;

		SC_REFLECTION_FIELD_EX("自動サイクル")
		Bool autoCycle_ = false;

		SC_REFLECTION_CLAMPED_EX("最短間隔(分)", 0.1f, 180.0f)
		Float minIntervalMinutes_ = 5.0f;

		SC_REFLECTION_CLAMPED_EX("最長間隔(分)", 0.1f, 180.0f)
		Float maxIntervalMinutes_ = 15.0f;

		SC_REFLECTION_CLAMPED_EX("遷移時間(秒)", 0.1f, 300.0f)
		Float transitionSeconds_ = 20.0f;

		/// [EN] When true, this type ignores the seasonal weighting below
		///      (e.g. forces Snow to be selectable outside Dec-Feb) during
		///      autoCycle_'s random pick. Has no effect when autoCycle_ is off
		///      (type_ is then just whatever the user/scene set directly).
		/// [JP] true の間、autoCycle_ のランダム選択でその天候が季節による
		///      重み付け(例: 雪は通常12〜2月のみ)を無視して選ばれ得るように
		///      なる(強制的に含める)。autoCycle_ が無効なら影響しない
		///      (type_ はユーザー/シーンが直接設定した値のまま)。
		SC_REFLECTION_FIELD_EX("雨を季節無視で許可")
		Bool forceRain_ = false;

		SC_REFLECTION_FIELD_EX("雪を季節無視で許可")
		Bool forceSnow_ = false;

		SC_REFLECTION_FIELD_EX("雷を強制的に許可")
		Bool forceThunder_ = false;

		SC_REFLECTION_FIELD_EX("暴風を強制的に許可")
		Bool forceStorm_ = false;

		/// [EN] Countdown to the next auto-cycle change. Runtime-only, not user-facing.
		/// [JP] 次の自動切替までのカウントダウン。実行時専用、ユーザー設定ではない。
		Float autoCycleTimer_ = 0.0f;

		/// [EN] 0-1 wet-surface amount: rises while raining/storming, decays
		///      slowly afterward (a lingering "just rained" look).
		/// [JP] 0-1の濡れ具合: 雨/暴風の間に上がり、止んだ後もゆっくり
		///      減衰する(「降った直後」の名残を表現)。
		Float wetness_ = 0.0f;

		/// [EN] 0-1 snow accumulation: rises while snowing, melts slowly once
		///      it stops (or fast if forced out of season).
		/// [JP] 0-1の積雪量: 雪の間に上がり、止むとゆっくり(季節外に
		///      強制された場合は速く)溶ける。
		Float snowCoverage_ = 0.0f;

		/// [EN] 0-1 current lightning flash brightness (rises instantly on a
		///      strike, decays over a fraction of a second).
		/// [JP] 0-1の現在の雷閃光の明るさ(発生時に瞬時に上がり、0.数秒で減衰)。
		Float thunderFlash_ = 0.0f;

		/// [EN] Random seed re-rolled on each strike, so the bolt shape drawn by
		///      DeferredCompositePS.hlsl differs strike to strike.
		/// [JP] 発生ごとに引き直す乱数シード。DeferredCompositePS.hlsl が描く
		///      稲妻の形が発生ごとに変わるようにする。
		Float thunderSeed_ = 0.0f;

		/// [EN] 0-1 "is it snowing right now" - fast ramp (unlike the slow
		///      snowCoverage_ ground accumulation), drives the falling-snow
		///      screen overlay.
		/// [JP] 0-1「今まさに雪が降っているか」- 素早く増減する(積雪の
		///      snowCoverage_ とは別)。画面の降雪オーバーレイを駆動する。
		Float snowIntensity_ = 0.0f;
	};
	REGISTER_COMPONENT(Weather, "Environment", ComponentStorage::SparseSet);
}
