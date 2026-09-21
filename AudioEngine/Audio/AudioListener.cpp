#include <AudioEngine/Audio/AudioListener.h>
#include <AudioEngine/Audio/Audio.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
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
	void AudioListener::Apply(Float deltaTime)
	{
		/// [EN] Build the listener position and orientation from the actor's resolved world transform.
		/// [JP] アクターの解決済みワールド変換から、リスナーの位置と向きを構成する。
		const Matrix& worldMatrix = GetActor().GetWorldMatrix();

		Vector3 position = worldMatrix.Translation();

		Vector3 front = Vector3::TransformNormal(Vector3::Forward, worldMatrix);
		front.Normalize();

		Vector3 up = Vector3::TransformNormal(Vector3::Up, worldMatrix);
		up.Normalize();

		/// [EN] Calculate world-space velocity from movement since the preceding update.
		/// [JP] 前回の更新からの移動量をもとに、ワールド空間上の速度を求める。
		Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		if (deltaTime > 0.0f)
		{
			velocity = (position - previousPosition_) / deltaTime;
		}
		previousPosition_ = position;

		/// [EN] Apply the Doppler scale and listener state to the World's audio context.
		/// [JP] ドップラー倍率とリスナー状態を World の音響環境へ反映する。
		Audio& audio = GetActor().GetAudio();
		audio.DopplerMultiplier(dopplerMultiplier_);
		audio.UpdateListener(position, front, up, velocity);
	}
}
