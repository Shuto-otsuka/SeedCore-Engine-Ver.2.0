#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class CriManager;
	class Sound;

	/**
	* [EN]
	* Thin wrapper over the CRI ADX2 player / 3D-positioning API, owned by
	* the World and reached from components through Actor::GetAudio(). It
	* creates and tracks every player (one playback voice controller) and
	* 3D source (one positional emitter) so that they can all be released
	* together when the World goes away, and holds the single 3D listener
	* that every 3D source is heard from. Every call taking a handle is a
	* no-op for a null handle, so a component whose handle was never
	* created (e.g. an actor that never became active) can call through
	* safely.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI ADX2 のプレイヤー / 3D ポジショニング API の薄いラッパー。World が
	* 所有し、コンポーネントからは Actor::GetAudio() で取得する。プレイヤー
	* (1つの再生ボイスを制御する単位)と 3D ソース(1つの位置を持つ発音源)
	* を生成・追跡し、World の破棄時にまとめて解放できるようにする。また、
	* 全ての 3D ソースを聞き取る唯一の 3D リスナーを持つ。ハンドルを受け
	* 取る関数は、ハンドルが null なら何もしない。そのため、ハンドルが
	* 一度も作られていないコンポーネント(例: 一度もアクティブにならな
	* かったアクター)からでも安全に呼び出せる。
	*/
	class SEEDCORE_API Audio
	{
	public:
		/**
		* [EN]
		* Creates the shared 3D listener.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有の 3D リスナーを生成する。
		*/
		Audio();

		/**
		* [EN]
		* Stops and destroys every player, then destroys every 3D source and
		* the listener. If the CRI library has already been finalized, the
		* handles are already gone, so only the bookkeeping is cleared.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全てのプレイヤーを停止して破棄し、続けて全ての 3D ソースとリスナー
		* を破棄する。CRI ライブラリが既に終了している場合はハンドル自体が
		* 無効になっているので、管理用の配列だけを空にする。
		*/
		~Audio();

	public:
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
		CriAtomExPlayerHn CreatePlayer();

		/**
		* [EN]
		* Stops, untracks and destroys a player.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーを停止し、追跡から外して破棄する。
		*/
		void DestroyPlayer(CriAtomExPlayerHn player);

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
		CriAtomEx3dSourceHn CreateSource();

		/**
		* [EN]
		* Untracks and destroys a 3D source.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 3D ソースを追跡から外して破棄する。
		*/
		void DestroySource(CriAtomEx3dSourceHn source);

	public:
		/**
		* [EN]
		* Plays the cue named cueName from a cue-sheet Sound (an ACB). Needs
		* the ACF to be registered and the Sound's ACB to be loaded; logs a
		* warning and does nothing otherwise. loop plays the cue's loop
		* region forever; without it, loop regions authored in the cue are
		* ignored and the cue plays through once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キューシートの Sound(ACB)から、cueName という名前のキューを再生
		* する。ACF が登録済みで、Sound の ACB が読み込み済みである必要があり、
		* そうでなければ警告を出して何もしない。loop なら、キューのループ区間
		* を無限に繰り返す。loop でなければ、キューに設定されたループ区間を
		* 無視して最後まで1回再生する。
		*/
		void PlayCue(CriAtomExPlayerHn player, Sound& sound, const String& cueName, Bool loop);

		/**
		* [EN]
		* Plays a raw .wav Sound on the player. The channel count and
		* sampling rate are read from the file's "fmt " chunk; a file that is
		* not RIFF/WAVE or has no usable "fmt " chunk is ignored. loop feeds
		* the same buffer again every time the player asks for more data, so
		* the wave repeats without a gap.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生の .wav の Sound をプレイヤーで再生する。チャンネル数とサンプリング
		* レートはファイルの "fmt " チャンクから読む。RIFF/WAVE でないファイルや、
		* 使える "fmt " チャンクが無いファイルは無視する。loop なら、プレイヤー
		* が次のデータを要求するたびに同じバッファをもう一度渡すので、波形が
		* 途切れずに繰り返される。
		*/
		void PlayWave(CriAtomExPlayerHn player, Sound& sound, Bool loop);

		/**
		* [EN]
		* Stops every voice the player is playing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーが再生中の全てのボイスを停止する。
		*/
		void Stop(CriAtomExPlayerHn player);

		/**
		* [EN]
		* Pauses the player's playback.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーの再生を一時停止する。
		*/
		void Pause(CriAtomExPlayerHn player);

		/**
		* [EN]
		* Resumes every paused voice of the player.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーの一時停止中の全てのボイスを再開する。
		*/
		void Resume(CriAtomExPlayerHn player);

	public:
		/**
		* [EN]
		* Sets the player's volume (1 = unchanged) and applies it to the
		* voices that are already playing, not only to the next Play.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーの音量(1 = そのまま)を設定し、次の再生からだけでなく、
		* 既に再生中のボイスにも反映する。
		*/
		void Volume(CriAtomExPlayerHn player, Float volume);

		/**
		* [EN]
		* Sets the player's pitch shift in cents (100 cents = one semitone,
		* 0 = unchanged) and applies it to the voices that are already
		* playing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーのピッチ変化をセント単位で設定し(100 セント = 半音、
		* 0 = そのまま)、既に再生中のボイスにも反映する。
		*/
		void Pitch(CriAtomExPlayerHn player, Float cents);

	public:
		/**
		* [EN]
		* Makes the player positional: its voices are panned and attenuated
		* from source's position as heard by the shared listener.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーを立体音響にする: そのボイスは、共有リスナーから見た
		* source の位置に応じて定位と減衰がかかる。
		*/
		void AttachSource(CriAtomExPlayerHn player, CriAtomEx3dSourceHn source);

		/**
		* [EN]
		* Makes the player non-positional again: its 3D source and listener
		* are cleared and panning goes back to the cue's own setting.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーを立体音響でない状態に戻す: 3D ソースとリスナーの割り当て
		* を外し、定位はキュー自身の設定に戻る。
		*/
		void DetachSource(CriAtomExPlayerHn player);

		/**
		* [EN]
		* Moves a 3D source to position with velocity (world units per
		* second; the velocity drives the Doppler shift).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 3D ソースを position へ移動し、速度 velocity(ワールド単位/秒。
		* ドップラー効果の計算に使う)を設定する。
		*/
		void UpdateSource(CriAtomEx3dSourceHn source, const Vector3& position, const Vector3& velocity);

		/**
		* [EN]
		* Moves the shared listener to position, facing front with up as its
		* up direction, moving at velocity (world units per second).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有リスナーを position へ移動し、front 方向を向き up を上方向とする
		* 向きと、速度 velocity(ワールド単位/秒)を設定する。
		*/
		void UpdateListener(const Vector3& position, const Vector3& front, const Vector3& up, const Vector3& velocity);

	public:
		/**
		* [EN]
		* Sets a 3D source's distance attenuation: full volume up to
		* minDistance from the listener, fading until it becomes silent at
		* maxDistance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 3D ソースの距離減衰を設定する: リスナーから minDistance までは
		* 最大音量で、そこから小さくなり maxDistance で聞こえなくなる。
		*/
		void AttenuationDistance(CriAtomEx3dSourceHn source, Float minDistance, Float maxDistance);

		/**
		* [EN]
		* Scales the Doppler shift heard by the shared listener (0 = no
		* Doppler, 1 = physical strength).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有リスナーに聞こえるドップラー効果の強さを倍率で設定する
		* (0 = ドップラー無し、1 = 物理的な強さ)。
		*/
		void DopplerMultiplier(Float multiplier);

	public:
		/**
		* [EN]
		* Returns whether the player is currently playing. A paused player is
		* still playing in CRI's sense and returns true; use Paused() to tell
		* them apart. False for a null player.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーが現在再生中かどうかを返す。一時停止中のプレイヤーも CRI
		* 上は再生中の扱いなので true を返す。区別するには Paused() を使う。
		* プレイヤーが null なら false。
		*/
		[[nodiscard]] Bool Playing(CriAtomExPlayerHn player)const;

		/**
		* [EN]
		* Returns whether the player is paused. False for a null player.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレイヤーが一時停止中かどうかを返す。プレイヤーが null なら false。
		*/
		[[nodiscard]] Bool Paused(CriAtomExPlayerHn player)const;

	private:
		/// [EN] The CRI library owner, bound through Gateway at construction.
		/// [JP] CRI ライブラリの所有者。生成時に Gateway から取得する。
		CriManager& criManager_;

		/// [EN] The single 3D listener every 3D source is heard from.
		/// [JP] 全ての 3D ソースを聞き取る、唯一の 3D リスナー。
		CriAtomEx3dListenerHn listener_ = nullptr;

		/// [EN] Every live player, destroyed together with this object.
		/// [JP] 生存中の全プレイヤー。このオブジェクトと一緒に破棄する。
		DynamicArray<CriAtomExPlayerHn> players_;

		/// [EN] Every live 3D source, destroyed together with this object.
		/// [JP] 生存中の全 3D ソース。このオブジェクトと一緒に破棄する。
		DynamicArray<CriAtomEx3dSourceHn> sources_;
	};
}
