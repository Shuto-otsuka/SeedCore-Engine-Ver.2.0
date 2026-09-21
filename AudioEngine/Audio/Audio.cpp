#include <AudioEngine/Audio/Audio.h>
#include <AudioEngine/Audio/Sound.h>
#include <AudioEngine/CRI/CriManager.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the shared 3D listener.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有の 3D リスナーを生成する。
	*/
	Audio::Audio() :criManager_(Gateway::GetCriManager())
	{
		/// [EN] One listener for the whole World; every 3D source attached later is heard from it.
		/// [JP] World 全体で1つのリスナー。後から取り付ける 3D ソースは全てこれで聞き取る。
		listener_ = criAtomEx3dListener_Create(nullptr, nullptr, 0);
	}

	/**
	* [EN]
	* Stops and destroys every player, then destroys every 3D source and
	* the listener. If the CRI library has already been finalized, only the
	* bookkeeping is cleared.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全てのプレイヤーを停止して破棄し、続けて全ての 3D ソースとリスナー
	* を破棄する。CRI ライブラリが既に終了している場合は、管理用の配列
	* だけを空にする。
	*/
	Audio::~Audio()
	{
		/// [EN] After CRI shutdown the handles are already invalid, so destroying them again would crash.
		/// [JP] CRI 終了後はハンドルが既に無効で、もう一度破棄するとクラッシュするため。
		if (!criAtomEx_IsInitialized())
		{
			players_.clear();
			sources_.clear();
			listener_ = nullptr;
			return;
		}

		/// [EN] Players first: a voice still playing may reference a 3D source.
		/// [JP] プレイヤーを先に: 再生中のボイスが 3D ソースを参照している場合があるため。
		for (CriAtomExPlayerHn player : players_)
		{
			criAtomExPlayer_Stop(player);
			criAtomExPlayer_Destroy(player);
		}
		players_.clear();

		for (CriAtomEx3dSourceHn source : sources_)
		{
			criAtomEx3dSource_Destroy(source);
		}
		sources_.clear();

		if (listener_)
		{
			criAtomEx3dListener_Destroy(listener_);
			listener_ = nullptr;
		}
	}

	/**
	* [EN]
	* Creates a player and tracks it for release on destruction. Returns
	* null if CRI cannot create one.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーを生成し、破棄時に解放できるよう追跡する。CRI が生成に
	* 失敗した場合は null を返す。
	*/
	CriAtomExPlayerHn Audio::CreatePlayer()
	{
		/// [EN] Default player settings, with CRI allocating the work memory itself.
		/// [JP] 既定のプレイヤー設定。ワークメモリは CRI 側で確保させる。
		CriAtomExPlayerHn player = criAtomExPlayer_Create(nullptr, nullptr, 0);
		if (!player)
		{
			return nullptr;
		}

		players_.push_back(player);
		return player;
	}

	/**
	* [EN]
	* Stops, untracks and destroys a player.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーを停止し、追跡から外して破棄する。
	*/
	void Audio::DestroyPlayer(CriAtomExPlayerHn player)
	{
		if (!player)
		{
			return;
		}

		/// [EN] Untrack first so the destructor never destroys this handle a second time.
		/// [JP] 先に追跡から外し、デストラクタがこのハンドルを二重に破棄しないようにする。
		erase(players_, player);

		criAtomExPlayer_Stop(player);
		criAtomExPlayer_Destroy(player);
	}

	/**
	* [EN]
	* Creates a 3D source and tracks it for release on destruction.
	* Returns null if CRI cannot create one.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3D ソースを生成し、破棄時に解放できるよう追跡する。CRI が生成に
	* 失敗した場合は null を返す。
	*/
	CriAtomEx3dSourceHn Audio::CreateSource()
	{
		/// [EN] Default source settings, with CRI allocating the work memory itself.
		/// [JP] 既定のソース設定。ワークメモリは CRI 側で確保させる。
		CriAtomEx3dSourceHn source = criAtomEx3dSource_Create(nullptr, nullptr, 0);
		if (!source)
		{
			return nullptr;
		}

		sources_.push_back(source);
		return source;
	}

	/**
	* [EN]
	* Untracks and destroys a 3D source.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3D ソースを追跡から外して破棄する。
	*/
	void Audio::DestroySource(CriAtomEx3dSourceHn source)
	{
		if (!source)
		{
			return;
		}

		/// [EN] Untrack first so the destructor never destroys this handle a second time.
		/// [JP] 先に追跡から外し、デストラクタがこのハンドルを二重に破棄しないようにする。
		erase(sources_, source);

		criAtomEx3dSource_Destroy(source);
	}

	/**
	* [EN]
	* Plays the cue named cueName from a cue-sheet Sound (an ACB). Needs the
	* ACF to be registered and the Sound's ACB to be loaded; logs a warning
	* and does nothing otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キューシートの Sound(ACB)から、cueName という名前のキューを再生
	* する。ACF が登録済みで、Sound の ACB が読み込み済みである必要があり、
	* そうでなければ警告を出して何もしない。
	*/
	void Audio::PlayCue(CriAtomExPlayerHn player, Sound& sound, const String& cueName, Bool loop)
	{
		if (!player || sound.Type() != SoundType::CueSheet || cueName.view().empty())
		{
			return;
		}

		/// [EN] Cue playback needs the ACF (categories, buses, DSP) to be registered first.
		/// [JP] キュー再生には、先に ACF(カテゴリ・バス・DSP)が登録されている必要がある。
		CriAtomExAcfInfo acfInfo{};
		if (criAtomExAcf_GetAcfInfo(&acfInfo) != CRI_TRUE)
		{
			SC_LOG_WARNING("Audio: ACFが登録されていないため、キューシートを再生できません。EditorConfig の acfPath に .acf を指定してください");
			return;
		}

		/// [EN] The cue sheet itself must already be loaded into CRI.
		/// [JP] キューシート自体も CRI へ読み込み済みである必要がある。
		if (!sound.AcbHandle())
		{
			SC_LOG_WARNING("Audio: ACBがCRIへ登録されていないため、キューシートを再生できません");
			return;
		}

		/// [EN] Looping plays the cue's loop region forever; otherwise the loop region is skipped.
		/// [JP] ループならキューのループ区間を無限に繰り返し、そうでなければループ区間を無視する。
		criAtomExPlayer_LimitLoopCount(player, loop ? CRIATOMEXPLAYER_NO_LOOP_LIMITATION : CRIATOMEXPLAYER_IGNORE_LOOP);

		/// [EN] Clear any wave-loop callback left from a previous PlayWave on this player.
		/// [JP] このプレイヤーで以前に PlayWave した際の、波形ループ用コールバックを外す。
		criAtomExPlayer_SetDataRequestCallback(player, nullptr, nullptr);

		criAtomExPlayer_SetCueName(player, sound.AcbHandle(), cueName.c_str());
		criAtomExPlayer_Start(player);
	}

	/**
	* [EN]
	* Plays a raw .wav Sound on the player. The channel count and sampling
	* rate are read from the file's "fmt " chunk; a file that is not
	* RIFF/WAVE or has no usable "fmt " chunk is ignored.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生の .wav の Sound をプレイヤーで再生する。チャンネル数とサンプリング
	* レートはファイルの "fmt " チャンクから読む。RIFF/WAVE でないファイルや、
	* 使える "fmt " チャンクが無いファイルは無視する。
	*/
	void Audio::PlayWave(CriAtomExPlayerHn player, Sound& sound, Bool loop)
	{
		/// [EN] 44 bytes is the smallest possible header (RIFF + "fmt " + "data").
		/// [JP] 44 バイトは、ヘッダ(RIFF + "fmt " + "data")が取りうる最小の大きさ。
		if (!player || sound.Type() != SoundType::Wave || !sound.Data() || sound.DataSize() < 44)
		{
			return;
		}

		/// [EN] Bytes 0-3 must be "RIFF" and bytes 8-11 must be "WAVE".
		/// [JP] 0〜3 バイト目が "RIFF"、8〜11 バイト目が "WAVE" である必要がある。
		const Byte* bytes = static_cast<const Byte*>(sound.Data());
		if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0)
		{
			return;
		}

		/// [EN] Walk the chunks after the 12-byte RIFF header until "fmt " is found.
		/// [JP] 12 バイトの RIFF ヘッダの後ろのチャンクを、"fmt " が見つかるまでたどる。
		Uint16 channels = 0;
		Uint32 samplingRate = 0;
		Size offset = 12;
		while (offset + 8 <= sound.DataSize())
		{
			/// [EN] Each chunk is a 4-byte id followed by a 4-byte little-endian size.
			/// [JP] 各チャンクは、4 バイトの ID と 4 バイトのリトルエンディアンのサイズで始まる。
			Uint32 chunkSize = 0;
			std::memcpy(&chunkSize, bytes + offset + 4, 4);

			/// [EN] In "fmt ", the channel count is at +10 and the sampling rate at +12.
			/// [JP] "fmt " の中では、チャンネル数が +10、サンプリングレートが +12 にある。
			if (std::memcmp(bytes + offset, "fmt ", 4) == 0 && chunkSize >= 16)
			{
				std::memcpy(&channels, bytes + offset + 10, 2);
				std::memcpy(&samplingRate, bytes + offset + 12, 4);
				break;
			}

			/// [EN] Chunks are padded to an even size, so an odd size skips one extra byte.
			/// [JP] チャンクは偶数サイズに詰められるので、奇数サイズなら 1 バイト余分に飛ばす。
			offset += 8 + static_cast<Size>(chunkSize) + (chunkSize & 1);
		}

		if (channels == 0 || samplingRate == 0)
		{
			return;
		}

		/// [EN] Hand the whole file to CRI, which parses the rest of the wave format itself.
		/// [JP] ファイル全体を CRI に渡す。残りの波形フォーマットは CRI 側が解釈する。
		criAtomExPlayer_SetData(player, sound.Data(), static_cast<CriSint32>(sound.DataSize()));
		criAtomExPlayer_SetFormat(player, CRIATOMEX_FORMAT_WAVE);
		criAtomExPlayer_SetNumChannels(player, static_cast<CriSint32>(channels));
		criAtomExPlayer_SetSamplingRate(player, static_cast<CriSint32>(samplingRate));

		/// [EN] Looping: when the data runs out, feed the same buffer again for a gapless repeat.
		/// [JP] ループ: データを使い切ったら同じバッファをもう一度渡し、途切れず繰り返す。
		criAtomExPlayer_SetDataRequestCallback(player, loop ? static_cast<CriAtomExPlayerDataRequestCbFunc>([](void* obj, CriAtomExPlaybackId playbackID, CriAtomPlayerHn atomPlayer) { criAtomPlayer_SetPreviousDataAgain(atomPlayer); }) : nullptr, nullptr);

		criAtomExPlayer_Start(player);
	}

	/**
	* [EN]
	* Stops every voice the player is playing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーが再生中の全てのボイスを停止する。
	*/
	void Audio::Stop(CriAtomExPlayerHn player)
	{
		if (!player)
		{
			return;
		}

		criAtomExPlayer_Stop(player);
	}

	/**
	* [EN]
	* Pauses the player's playback.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーの再生を一時停止する。
	*/
	void Audio::Pause(CriAtomExPlayerHn player)
	{
		if (!player)
		{
			return;
		}

		criAtomExPlayer_Pause(player, CRI_TRUE);
	}

	/**
	* [EN]
	* Resumes every paused voice of the player.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーの一時停止中の全てのボイスを再開する。
	*/
	void Audio::Resume(CriAtomExPlayerHn player)
	{
		if (!player)
		{
			return;
		}

		/// [EN] ALL_PLAYBACK resumes paused voices and also starts ones held by criAtomExPlayer_Prepare.
		/// [JP] ALL_PLAYBACK は、一時停止中のボイスに加え、Prepare で待機中のボイスも開始させる。
		criAtomExPlayer_Resume(player, CRIATOMEX_RESUME_ALL_PLAYBACK);
	}

	/**
	* [EN]
	* Sets the player's volume and applies it to the voices that are
	* already playing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーの音量を設定し、既に再生中のボイスにも反映する。
	*/
	void Audio::Volume(CriAtomExPlayerHn player, Float volume)
	{
		if (!player)
		{
			return;
		}

		/// [EN] Set only affects the next Start; UpdateAll pushes it to the voices already playing.
		/// [JP] Set だけでは次の Start からしか効かないので、UpdateAll で再生中のボイスにも反映する。
		criAtomExPlayer_SetVolume(player, volume);
		criAtomExPlayer_UpdateAll(player);
	}

	/**
	* [EN]
	* Sets the player's pitch shift in cents and applies it to the voices
	* that are already playing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーのピッチ変化をセント単位で設定し、既に再生中のボイスにも
	* 反映する。
	*/
	void Audio::Pitch(CriAtomExPlayerHn player, Float cents)
	{
		if (!player)
		{
			return;
		}

		/// [EN] Set only affects the next Start; UpdateAll pushes it to the voices already playing.
		/// [JP] Set だけでは次の Start からしか効かないので、UpdateAll で再生中のボイスにも反映する。
		criAtomExPlayer_SetPitch(player, cents);
		criAtomExPlayer_UpdateAll(player);
	}

	/**
	* [EN]
	* Makes the player positional, heard from source's position by the
	* shared listener.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーを立体音響にし、共有リスナーから source の位置で聞こえる
	* ようにする。
	*/
	void Audio::AttachSource(CriAtomExPlayerHn player, CriAtomEx3dSourceHn source)
	{
		if (!player || !source)
		{
			return;
		}

		/// [EN] A 3D voice needs both ends: the emitter (source) and the ear (listener).
		/// [JP] 3D のボイスには、発音側(source)と聴取側(listener)の両方が必要。
		criAtomExPlayer_Set3dSourceHn(player, source);
		criAtomExPlayer_Set3dListenerHn(player, listener_);

		/// [EN] Pan from the source's 3D position instead of the cue's authored panning.
		/// [JP] キューに設定されたパンではなく、ソースの 3D 位置から定位を決める。
		criAtomExPlayer_SetPanType(player, CRIATOMEX_PAN_TYPE_3D_POS);
		criAtomExPlayer_UpdateAll(player);
	}

	/**
	* [EN]
	* Makes the player non-positional again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーを立体音響でない状態に戻す。
	*/
	void Audio::DetachSource(CriAtomExPlayerHn player)
	{
		if (!player)
		{
			return;
		}

		criAtomExPlayer_Set3dSourceHn(player, nullptr);
		criAtomExPlayer_Set3dListenerHn(player, nullptr);

		/// [EN] AUTO goes back to whatever panning the cue itself was authored with.
		/// [JP] AUTO は、キュー自身に設定されたパンの方式へ戻す。
		criAtomExPlayer_SetPanType(player, CRIATOMEX_PAN_TYPE_AUTO);
		criAtomExPlayer_UpdateAll(player);
	}

	/**
	* [EN]
	* Moves a 3D source to position with velocity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3D ソースを position へ移動し、速度 velocity を設定する。
	*/
	void Audio::UpdateSource(CriAtomEx3dSourceHn source, const Vector3& position, const Vector3& velocity)
	{
		if (!source)
		{
			return;
		}

		CriAtomExVector sourcePosition{};
		sourcePosition.x = position.x;
		sourcePosition.y = position.y;
		sourcePosition.z = position.z;

		/// [EN] Velocity is what CRI uses to compute the Doppler shift.
		/// [JP] 速度は、CRI がドップラー効果を計算するために使う。
		CriAtomExVector sourceVelocity{};
		sourceVelocity.x = velocity.x;
		sourceVelocity.y = velocity.y;
		sourceVelocity.z = velocity.z;

		/// [EN] The Set calls are only staged; Update applies them to the playing voices.
		/// [JP] Set の呼び出しは溜めておくだけで、Update で再生中のボイスに反映される。
		criAtomEx3dSource_SetPosition(source, &sourcePosition);
		criAtomEx3dSource_SetVelocity(source, &sourceVelocity);
		criAtomEx3dSource_Update(source);
	}

	/**
	* [EN]
	* Moves the shared listener to position, facing front with up as its up
	* direction, moving at velocity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有リスナーを position へ移動し、front 方向を向き up を上方向とする
	* 向きと、速度 velocity を設定する。
	*/
	void Audio::UpdateListener(const Vector3& position, const Vector3& front, const Vector3& up, const Vector3& velocity)
	{
		if (!listener_)
		{
			return;
		}

		CriAtomExVector listenerPosition{};
		listenerPosition.x = position.x;
		listenerPosition.y = position.y;
		listenerPosition.z = position.z;

		/// [EN] Front and up together define which way is left/right for panning.
		/// [JP] front と up の組で、パンにおける左右の向きが決まる。
		CriAtomExVector listenerFront{};
		listenerFront.x = front.x;
		listenerFront.y = front.y;
		listenerFront.z = front.z;

		CriAtomExVector listenerUp{};
		listenerUp.x = up.x;
		listenerUp.y = up.y;
		listenerUp.z = up.z;

		/// [EN] Velocity is what CRI uses to compute the Doppler shift.
		/// [JP] 速度は、CRI がドップラー効果を計算するために使う。
		CriAtomExVector listenerVelocity{};
		listenerVelocity.x = velocity.x;
		listenerVelocity.y = velocity.y;
		listenerVelocity.z = velocity.z;

		/// [EN] The Set calls are only staged; Update applies them to the playing voices.
		/// [JP] Set の呼び出しは溜めておくだけで、Update で再生中のボイスに反映される。
		criAtomEx3dListener_SetPosition(listener_, &listenerPosition);
		criAtomEx3dListener_SetOrientation(listener_, &listenerFront, &listenerUp);
		criAtomEx3dListener_SetVelocity(listener_, &listenerVelocity);
		criAtomEx3dListener_Update(listener_);
	}

	/**
	* [EN]
	* Sets a 3D source's distance attenuation: full volume up to
	* minDistance, silent at maxDistance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3D ソースの距離減衰を設定する: minDistance までは最大音量で、
	* maxDistance で聞こえなくなる。
	*/
	void Audio::AttenuationDistance(CriAtomEx3dSourceHn source, Float minDistance, Float maxDistance)
	{
		if (!source)
		{
			return;
		}

		/// [EN] The Set call is only staged; Update applies it to the playing voices.
		/// [JP] Set の呼び出しは溜めておくだけで、Update で再生中のボイスに反映される。
		criAtomEx3dSource_SetMinMaxAttenuationDistance(source, minDistance, maxDistance);
		criAtomEx3dSource_Update(source);
	}

	/**
	* [EN]
	* Scales the Doppler shift heard by the shared listener.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有リスナーに聞こえるドップラー効果の強さを倍率で設定する。
	*/
	void Audio::DopplerMultiplier(Float multiplier)
	{
		if (!listener_)
		{
			return;
		}

		/// [EN] The Set call is only staged; Update applies it to the playing voices.
		/// [JP] Set の呼び出しは溜めておくだけで、Update で再生中のボイスに反映される。
		criAtomEx3dListener_SetDopplerMultiplier(listener_, multiplier);
		criAtomEx3dListener_Update(listener_);
	}

	/**
	* [EN]
	* Returns whether the player is currently playing (true while paused
	* too). False for a null player.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーが現在再生中かどうかを返す(一時停止中も true)。
	* プレイヤーが null なら false。
	*/
	Bool Audio::Playing(CriAtomExPlayerHn player)const
	{
		if (!player)
		{
			return false;
		}

		/// [EN] PREP (still buffering) and PLAYEND (finished) both count as not playing.
		/// [JP] PREP(準備中)と PLAYEND(再生終了)は、どちらも再生中に含めない。
		return criAtomExPlayer_GetStatus(player) == CRIATOMEXPLAYER_STATUS_PLAYING;
	}

	/**
	* [EN]
	* Returns whether the player is paused. False for a null player.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレイヤーが一時停止中かどうかを返す。プレイヤーが null なら false。
	*/
	Bool Audio::Paused(CriAtomExPlayerHn player)const
	{
		if (!player)
		{
			return false;
		}

		return criAtomExPlayer_IsPaused(player) == CRI_TRUE;
	}
}
