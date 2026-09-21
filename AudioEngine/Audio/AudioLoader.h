#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <AudioEngine/Audio/Sound.h>

namespace SeedCore
{
	struct LoaderSystem;

	/**
	* [EN]
	* Loads encrypted audio caches into stable Sound storage and converts
	* supported source files into the engine's .audio cache format.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 暗号化された音声キャッシュを安定した Sound ストレージへ読み込み、
	* 対応する素材ファイルをエンジンの .audio キャッシュ形式へ変換する。
	*/
	class SEEDCORE_API AudioLoader :public NonCopyable
	{
	public:
		/**
		* [EN]
		* Initializes COM and Media Foundation for MP3 decoding.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* MP3 のデコードに使う COM と Media Foundation を初期化する。
		*/
		AudioLoader();

		/**
		* [EN]
		* Shuts down the facilities initialized by this loader.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このローダーが初期化した機能を終了する。
		*/
		~AudioLoader();

		/**
		* [EN]
		* Loads a Sound from an up-to-date .audio cache, baking one from the
		* source file when needed. Returns a null handle on failure.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最新の .audio キャッシュから Sound を読み込み、必要なら素材ファイル
		* からキャッシュを作成する。失敗時は null ハンドルを返す。
		*/
		Handle<Sound> Load(LoaderSystem& loader, String filePath);

		/**
		* [EN]
		* Resolves a Sound handle to its pooled object.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Sound ハンドルからプール内のオブジェクトを取得する。
		*/
		Sound* Get(const Handle<Sound>& handle);

		/**
		* [EN]
		* Releases CRI data associated with a Sound and destroys its handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Sound に関連する CRI データを解放し、ハンドルを破棄する。
		*/
		void Clear(Handle<Sound>& handle)noexcept;

		/**
		* [EN]
		* Converts a supported audio source into an encrypted .audio cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 対応する音声素材を暗号化された .audio キャッシュへ変換する。
		*/
		Bool Bake(String sourcePath, String cachePath);

	private:
		/// [EN] Stable storage backing every Sound handle created by this loader.
		/// [JP] このローダーが生成する全 Sound ハンドルの安定した格納先。
		StablePool<Sound> pool_;

		/// [EN] Whether this instance owns a successful COM initialization call.
		/// [JP] このインスタンスが成功した COM 初期化呼び出しを所有するか。
		Bool ownsComInitialize_ = false;

		/// [EN] Whether Media Foundation started successfully for this loader.
		/// [JP] このローダー用の Media Foundation が正常に開始されたか。
		Bool mfStarted_ = false;
	};
}
