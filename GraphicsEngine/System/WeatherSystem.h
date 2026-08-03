#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Renderer/VolumetricCloudScapesRenderer.h>

namespace SeedCore
{
	class World;
	struct Weather;
	enum class WeatherType;

	/// [EN] Snapshot of the scene's Weather runtime state for the GPU (see
	///      LightSystem::Gather's weather parameter). Zeroed when the scene
	///      has no Weather component.
	/// [JP] シーンの Weather 実行時状態のGPU向けスナップショット
	///      (LightSystem::Gather の weather 引数参照)。シーンに Weather
	///      コンポーネントが無ければ全て0。
	struct WeatherGpuState
	{
		Float wetness_ = 0.0f;
		Float snowCoverage_ = 0.0f;
		Float thunderFlash_ = 0.0f;
		Float snowIntensity_ = 0.0f;
		Float thunderSeed_ = 0.0f;
	};

	/**
	* [EN]
	* Hand-driven (not SystemScheduler-registered, same as SceneTransitionSystem):
	* finds the scene's Weather component (singleton-by-convention, see
	* Environment/Weather.h) and interpolates cloudSettings' existing fields
	* toward the current WeatherType's target values. Also advances the
	* seasonal auto-cycle (monthOfYear-weighted, force flags bypass the
	* seasonal weighting), the wetness_/snowCoverage_ ramps, and thunder
	* strike/flash timing. A no-op when the scene has no Weather component, so
	* manual cloud tuning in the editor is left alone.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ハンド駆動(SystemScheduler には登録しない、SceneTransitionSystem と同じ):
	* シーンの Weather コンポーネント(singleton的運用、Environment/Weather.h
	* 参照)を見つけ、cloudSettings の既存フィールドを現在の WeatherType の
	* 目標値へ補間する。季節による自動サイクル(monthOfYear で重み付け、
	* forceフラグは季節の重みを無視)、wetness_/snowCoverage_ の増減、
	* 雷の発生/閃光タイミングも進める。シーンに Weather コンポーネントが
	* 無ければ何もしないので、エディタでの手動雲チューニングには干渉しない。
	*/
	class SEEDCORE_API WeatherSystem
	{
	public:
		void Execute(World& world, Float deltaTime, Uint32 monthOfYear, VolumetricCloudScapesRayConstantBuffer& cloudSettings);

		/// [EN] Read-only snapshot for GPU consumption - queries the scene's
		///      Weather component without mutating anything. Returns defaults
		///      (all zero) if the scene has none.
		/// [JP] GPU向けの読み取り専用スナップショット - シーンの Weather
		///      コンポーネントを何も変更せず読む。無ければ既定値(全0)を返す。
		[[nodiscard]] static WeatherGpuState ReadGpuState(World& world);

	private:
		static void GetTarget(WeatherType type, VolumetricCloudScapesRayConstantBuffer& target);

		[[nodiscard]] WeatherType PickRandomType(Uint32 monthOfYear, const Weather& weather);

		[[nodiscard]] Float RandomRange(Float minValue, Float maxValue);

		std::mt19937 randomEngine_ = std::mt19937(std::random_device{}());
	};
}
