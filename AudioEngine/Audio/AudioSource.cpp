#include <AudioEngine/Audio/AudioSource.h>
#include <AudioEngine/Audio/Audio.h>
#include <AudioEngine/Audio/Sound.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the playback voice and 3D source used by this component.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このコンポーネントが使う再生ボイスと 3D 音源を生成する。
	*/
	void AudioSource::OnAwake()
	{
		Audio& audio = GetActor().GetAudio();

		/// [EN] Create the CRI handles and apply the initial playback settings.
		/// [JP] CRI ハンドルを生成し、初期の再生設定を反映する。
		player_ = audio.CreatePlayer();
		source_ = audio.CreateSource();

		audio.Volume(player_, volume_);
		audio.Pitch(player_, pitch_ * 100.0f);

		attached_ = spatial_;
		if (attached_)
		{
			audio.AttachSource(player_, source_);
			audio.AttenuationDistance(source_, minDistance_, maxDistance_);
		}

		previousPosition_ = GetActor().GetWorldMatrix().Translation();
	}

	/**
	* [EN]
	* Destroys the playback voice and 3D source.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再生ボイスと 3D 音源を破棄する。
	*/
	void AudioSource::OnDestroy()
	{
		Audio& audio = GetActor().GetAudio();

		audio.DestroyPlayer(player_);
		audio.DestroySource(source_);

		player_ = nullptr;
		source_ = nullptr;
		sound_ = nullptr;
	}

	/**
	* [EN]
	* Draws the cue selector for a loaded cue-sheet sound.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 読み込み済みのキューシート用にキュー選択 UI を描画する。
	*/
	void AudioSource::OnInspectorGUI()
	{
		/// [EN] Show the state that prevents cue selection before opening the combo box.
		/// [JP] コンボボックスを開く前に、キューを選択できない状態を表示する。
		if (soundID_ == 0)
		{
			ImGui::TextDisabled("サウンド未設定");
			return;
		}

		if (!sound_)
		{
			ImGui::TextDisabled("サウンド未取得");
			return;
		}

		if (sound_->Type() != SoundType::CueSheet)
		{
			ImGui::TextDisabled("波形データ（キュー指定なし）");
			return;
		}

		CriAtomExAcbHn acbHandle = sound_->AcbHandle();
		if (!acbHandle)
		{
			ImGui::TextDisabled("キューシート未取得");
			return;
		}

		/// [EN] Enumerate valid cue names from the loaded ACB for selection.
		/// [JP] 読み込み済み ACB から有効なキュー名を列挙して選択可能にする。
		const Char* preview = cueName_.c_str();
		if (ImGui::BeginCombo("キュー", preview ? preview : ""))
		{
			CriSint32 cueCount = criAtomExAcb_GetNumCues(acbHandle);
			for (CriSint32 cueIndex = 0; cueIndex < cueCount; ++cueIndex)
			{
				CriAtomExCueInfo cueInfo{};
				if (criAtomExAcb_GetCueInfoByIndex(acbHandle, static_cast<CriAtomExCueIndex>(cueIndex), &cueInfo) != CRI_TRUE)
				{
					continue;
				}

				if (!cueInfo.name || cueInfo.name[0] == '\0')
				{
					continue;
				}

				Bool selected = cueName_ == cueInfo.name;
				if (ImGui::Selectable(cueInfo.name, selected))
				{
					cueName_ = cueInfo.name;
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
	}

	/**
	* [EN]
	* Starts playback using the configured sound and cue name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 設定済みのサウンドとキュー名を使って再生を開始する。
	*/
	void AudioSource::Play()
	{
		if (!sound_)
		{
			return;
		}

		Audio& audio = GetActor().GetAudio();

		/// [EN] Select cue-sheet or wave playback according to the resolved sound type.
		/// [JP] 解決済みサウンドの種類に応じて、キューシート再生か波形再生を選ぶ。
		if (sound_->Type() == SoundType::CueSheet)
		{
			audio.PlayCue(player_, *sound_, cueName_, loop_);
			return;
		}

		audio.PlayWave(player_, *sound_, loop_);
	}

	/**
	* [EN]
	* Selects a cue name and starts playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キュー名を選択して再生を開始する。
	*/
	void AudioSource::Play(const String& cueName)
	{
		cueName_ = cueName;

		Play();
	}

	/**
	* [EN]
	* Stops the current playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の再生を停止する。
	*/
	void AudioSource::Stop()
	{
		GetActor().GetAudio().Stop(player_);
	}

	/**
	* [EN]
	* Pauses the current playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の再生を一時停止する。
	*/
	void AudioSource::Pause()
	{
		GetActor().GetAudio().Pause(player_);
	}

	/**
	* [EN]
	* Resumes paused playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 一時停止中の再生を再開する。
	*/
	void AudioSource::Resume()
	{
		GetActor().GetAudio().Resume(player_);
	}

	/**
	* [EN]
	* Reports whether this source is currently playing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この音源が現在再生中かを返す。
	*/
	Bool AudioSource::Playing()const
	{
		return GetActor().GetAudio().Playing(player_);
	}

	/**
	* [EN]
	* Replaces the resolved Sound and records the current asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 解決済み Sound を置き換え、現在のアセット ID を記録する。
	*/
	void AudioSource::Build(Sound* sound)
	{
		/// [EN] Stop the previous sound so automatic playback can start once for the replacement.
		/// [JP] 置き換え後の自動再生を一度開始できるよう、以前のサウンドを停止する。
		if (player_ && sound_ != sound)
		{
			GetActor().GetAudio().Stop(player_);
			autoPlayStarted_ = false;
		}

		sound_ = sound;
		resolvedSoundID_ = soundID_;
	}

	/**
	* [EN]
	* Applies the current transform and playback settings to the audio handles.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の変換と再生設定をオーディオハンドルへ反映する。
	*/
	void AudioSource::Apply(Float deltaTime)
	{
		Audio& audio = GetActor().GetAudio();

		/// [EN] Calculate the source position and velocity from the actor's world transform.
		/// [JP] アクターのワールド変換から、音源の位置と速度を求める。
		Vector3 position = GetActor().GetWorldMatrix().Translation();
		Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		if (deltaTime > 0.0f)
		{
			velocity = (position - previousPosition_) / deltaTime;
		}
		previousPosition_ = position;

		/// [EN] Keep the player attachment synchronized with the spatial-audio setting.
		/// [JP] プレイヤーの接続状態を立体音響設定と同期する。
		if (attached_ != spatial_)
		{
			attached_ = spatial_;

			if (attached_)
			{
				audio.AttachSource(player_, source_);
			}
			else
			{
				audio.DetachSource(player_);
			}
		}

		/// [EN] Update spatial properties and the playback parameters that may change at runtime.
		/// [JP] 空間特性と、実行中に変更される可能性がある再生パラメーターを更新する。
		if (attached_)
		{
			audio.AttenuationDistance(source_, minDistance_, maxDistance_);
			audio.UpdateSource(source_, position, velocity);
		}

		audio.Volume(player_, volume_);
		audio.Pitch(player_, pitch_ * 100.0f);

		/// [EN] Start automatic playback once the assigned Sound becomes available.
		/// [JP] 割り当てた Sound が利用可能になった時点で、自動再生を一度開始する。
		if (autoPlay_ && !autoPlayStarted_ && sound_)
		{
			autoPlayStarted_ = true;
			Play();
		}
	}

	/**
	* [EN]
	* Reports whether the assigned sound ID still needs to be resolved.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 割り当てられたサウンド ID をまだ解決する必要があるかを返す。
	*/
	Bool AudioSource::Pending()const
	{
		return soundID_ != resolvedSoundID_;
	}
}
