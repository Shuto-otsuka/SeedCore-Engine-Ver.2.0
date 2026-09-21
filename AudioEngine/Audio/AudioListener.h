#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that maps an actor's world-space position and orientation to
	* the shared 3D audio listener. Its movement between updates supplies the
	* listener velocity used for Doppler effects.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクターのワールド空間上の位置と向きを、共有の 3D オーディオリスナー
	* へ反映するコンポーネント。更新間の移動から、ドップラー効果に使う
	* リスナー速度を求める。
	*/
	class SEEDCORE_API AudioListener :public SeedScript
	{
	private:
		friend class AudioSystem;

	public:
		/// [EN] Scale applied to the Doppler effect of the shared listener.
		/// [JP] 共有リスナーのドップラー効果に適用する倍率。
		SC_REFLECTION_CLAMPED_EX("ドップラー倍率", 0.0f, 10.0f)
		Float dopplerMultiplier_ = 1.0f;

	private:
		/**
		* [EN]
		* Updates the shared listener from the actor's current world transform
		* and the velocity calculated over deltaTime.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アクターの現在のワールド変換と deltaTime から求めた速度で、共有
		* リスナーを更新する。
		*/
		void Apply(Float deltaTime);

	private:
		/// [EN] World-space position retained from the preceding listener update.
		/// [JP] 前回のリスナー更新時から保持するワールド空間上の位置。
		Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
	};
	REGISTER_COMPONENT(AudioListener, "Audio");
}
