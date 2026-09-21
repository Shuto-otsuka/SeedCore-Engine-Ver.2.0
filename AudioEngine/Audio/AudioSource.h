#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class Sound;

	/**
	* [EN]
	* Component that owns a playback voice and optionally positions it as a
	* 3D sound source. It supports cue sheets, wave data and automatic playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再生ボイスを所有し、必要に応じて 3D 音源として配置するコンポーネント。
	* キューシート、波形データ、自動再生に対応する。
	*/
	class SEEDCORE_API AudioSource :public SeedScript
	{
	private:
		friend class AudioSystem;

	public:
		/// [EN] Asset ID of the sound assigned to this source.
		/// [JP] この音源へ割り当てるサウンドのアセット ID。
		SC_PAYLOAD_FIELD_EX("サウンド", Audio)
		Uint32 soundID_ = 0;

		/// [EN] Cue name selected when the assigned sound is a cue sheet.
		/// [JP] 割り当てたサウンドがキューシートの場合に選択するキュー名。
		SC_SERIALIZE_FIELD()
		String cueName_;

		/// [EN] Whether playback starts automatically after the sound is resolved.
		/// [JP] サウンドの解決後に再生を自動で開始するか。
		SC_REFLECTION_FIELD_EX("自動再生")
		Bool autoPlay_ = false;

		/// [EN] Whether playback repeats continuously.
		/// [JP] 再生を繰り返すか。
		SC_REFLECTION_FIELD_EX("ループ再生")
		Bool loop_ = false;

		/// [EN] Playback volume multiplier.
		/// [JP] 再生音量の倍率。
		SC_REFLECTION_CLAMPED_EX("音量", 0.0f, 5.0f)
		Float volume_ = 1.0f;

		/// [EN] Playback pitch offset in semitones.
		/// [JP] 半音単位の再生ピッチ差分。
		SC_REFLECTION_CLAMPED_EX("ピッチ(半音)", -12.0f, 12.0f)
		Float pitch_ = 0.0f;

		/// [EN] Whether the playback voice is attached to a 3D source.
		/// [JP] 再生ボイスを 3D 音源へ接続するか。
		SC_REFLECTION_FIELD_EX("立体音響")
		Bool spatial_ = true;

		/// [EN] Distance at which spatial attenuation begins.
		/// [JP] 空間減衰が始まる距離。
		SC_REFLECTION_FIELD_CONDITION(spatial_)
		SC_REFLECTION_CLAMPED_EX("減衰開始距離", 0.0f, 10000.0f)
		Float minDistance_ = 1.0f;

		/// [EN] Distance at which spatial attenuation reaches its far limit.
		/// [JP] 空間減衰が遠距離側の限界へ達する距離。
		SC_REFLECTION_FIELD_CONDITION(spatial_)
		SC_REFLECTION_CLAMPED_EX("減衰終了距離", 0.0f, 10000.0f)
		Float maxDistance_ = 50.0f;

	public:
		/**
		* [EN]
		* Creates the playback voice and 3D source used by this component.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このコンポーネントが使う再生ボイスと 3D 音源を生成する。
		*/
		void OnAwake();

		/**
		* [EN]
		* Destroys the playback voice and 3D source.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 再生ボイスと 3D 音源を破棄する。
		*/
		void OnDestroy();

		/**
		* [EN]
		* Draws the cue selector for a loaded cue-sheet sound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 読み込み済みのキューシート用にキュー選択 UI を描画する。
		*/
		void OnInspectorGUI();

	public:
		/**
		* [EN]
		* Starts playback using the configured sound and cue name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定済みのサウンドとキュー名を使って再生を開始する。
		*/
		void Play();

		/**
		* [EN]
		* Selects a cue name and starts playback.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キュー名を選択して再生を開始する。
		*/
		void Play(const String& cueName);

		/**
		* [EN]
		* Stops the current playback.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の再生を停止する。
		*/
		void Stop();

		/**
		* [EN]
		* Pauses the current playback.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の再生を一時停止する。
		*/
		void Pause();

		/**
		* [EN]
		* Resumes paused playback.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 一時停止中の再生を再開する。
		*/
		void Resume();

	public:
		/**
		* [EN]
		* Reports whether this source is currently playing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この音源が現在再生中かを返す。
		*/
		Bool Playing()const;

	private:
		/**
		* [EN]
		* Replaces the resolved Sound and records the current asset ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 解決済み Sound を置き換え、現在のアセット ID を記録する。
		*/
		void Build(Sound* sound);

		/**
		* [EN]
		* Applies the current transform and playback settings to the audio handles.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の変換と再生設定をオーディオハンドルへ反映する。
		*/
		void Apply(Float deltaTime);

		/**
		* [EN]
		* Reports whether the assigned sound ID still needs to be resolved.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 割り当てられたサウンド ID をまだ解決する必要があるかを返す。
		*/
		Bool Pending()const;

	private:
		/// [EN] CRI playback voice controlled by this component.
		/// [JP] このコンポーネントが制御する CRI 再生ボイス。
		CriAtomExPlayerHn player_ = nullptr;

		/// [EN] CRI 3D source used for spatial playback.
		/// [JP] 空間再生に使う CRI 3D 音源。
		CriAtomEx3dSourceHn source_ = nullptr;

		/// [EN] Sound currently resolved from soundID_.
		/// [JP] soundID_ から現在解決されている Sound。
		Sound* sound_ = nullptr;

		/// [EN] World-space position retained from the preceding source update.
		/// [JP] 前回の音源更新時から保持するワールド空間上の位置。
		Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };

		/// [EN] Asset ID represented by sound_.
		/// [JP] sound_ が表すアセット ID。
		Uint32 resolvedSoundID_ = 0;

		/// [EN] Whether the player is currently attached to the 3D source.
		/// [JP] プレイヤーが現在 3D 音源へ接続されているか。
		Bool attached_ = false;

		/// [EN] Whether automatic playback has already been requested for the current sound.
		/// [JP] 現在のサウンドに対して自動再生を既に要求したか。
		Bool autoPlayStarted_ = false;
	};
	REGISTER_COMPONENT(AudioSource, "Audio");
}
