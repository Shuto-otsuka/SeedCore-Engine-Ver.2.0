#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Owns the CRI ADX runtime: initializes the library, registers the ACF
	* (the project-wide audio settings built by Atom Craft), allocates the
	* voice pools and streaming bandwidth, and keeps the master and
	* category volumes. The ACF and the volumes are persisted together in
	* AudioBindings.scg, so a shipped game needs no separate .acf file.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI ADX のランタイムを所有する。ライブラリを初期化し、ACF(Atom Craft
	* が出力するプロジェクト全体の音声設定)を登録し、ボイスプールと
	* ストリーミング帯域を確保し、マスター音量とカテゴリ音量を保持する。
	* ACF と音量は AudioBindings.scg にまとめて保存するので、出荷した
	* ゲームは .acf ファイルを別に持たなくてよい。
	*/
	class SEEDCORE_API CriManager
	{
	public:
		/**
		* [EN]
		* Constructs an uninitialized manager; Initialize does the work.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 未初期化の管理機構を作る。実際の準備は Initialize が行う。
		*/
		CriManager() = default;

		/**
		* [EN]
		* Destroys the manager; Finalize must already have shut CRI down.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 管理機構を破棄する。CRI の終了は、先に Finalize で済ませておく。
		*/
		~CriManager() = default;

		/**
		* [EN]
		* Initializes CRI, loads the saved bindings, registers the ACF and
		* applies the saved volumes, then allocates the voice pools and the
		* streaming bandwidth manager. Returns false if any step fails.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CRI を初期化し、保存済みの設定を読み、ACF を登録して保存済みの音量を
		* 反映したあと、ボイスプールとストリーミング帯域の管理を確保する。
		* どこかで失敗したら false を返す。
		*/
		Bool Initialize();

		/**
		* [EN]
		* Runs CRI's per-frame server processing; called once every frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CRI の毎フレームのサーバー処理を行う。毎フレーム1回呼ぶ。
		*/
		void Execute();

		/**
		* [EN]
		* Releases the voice pools, the bandwidth manager and the ACF, and
		* shuts CRI down. Does nothing if CRI was never initialized.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボイスプール、帯域の管理、ACF を解放し、CRI を終了する。CRI が
		* 初期化されていなければ何もしない。
		*/
		void Finalize();

	public:
		/**
		* [EN]
		* Sets the path of the .acf file that Atom Craft writes. Used by the
		* editor to pick up a rebuilt ACF; the runtime leaves it empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Atom Craft が出力する .acf ファイルのパスを設定する。エディタが
		* ビルドし直した ACF を取り込むのに使い、ランタイムは空のままにする。
		*/
		void MasterPath(const String& path);

		/**
		* [EN]
		* Sets the master volume and applies it to the master bus at once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マスター音量を設定し、すぐにマスターバスへ反映する。
		*/
		void MasterVolume(Float volume);

		/**
		* [EN]
		* Sets the master volume that ResetVolume returns to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ResetVolume で戻す先のマスター音量を設定する。
		*/
		void DefaultMasterVolume(Float volume);

		/**
		* [EN]
		* Sets the volume of a category (such as BGM or SE) and applies it
		* at once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カテゴリ(BGM や SE など)の音量を設定し、すぐに反映する。
		*/
		void CategoryVolume(const String& category, Float volume);

		/**
		* [EN]
		* Returns the master volume to its default and every category to the
		* volume defined in the ACF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マスター音量を既定値に、全カテゴリを ACF で定めた音量に戻す。
		*/
		void ResetVolume();

	public:
		/**
		* [EN]
		* Reads the ACF and the volumes from AudioBindings.scg. Returns false
		* if the file cannot be read.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* AudioBindings.scg から ACF と音量を読み込む。ファイルを読めなければ
		* false を返す。
		*/
		Bool Load();

		/**
		* [EN]
		* Writes the ACF and the volumes to AudioBindings.scg. Returns false
		* if the file cannot be written.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ACF と音量を AudioBindings.scg へ書き出す。書き込めなければ
		* false を返す。
		*/
		Bool Save();

	public:
		/**
		* [EN]
		* Returns the current master volume.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のマスター音量を返す。
		*/
		[[nodiscard]] Float MasterVolume()const;

		/**
		* [EN]
		* Returns the master volume that ResetVolume returns to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ResetVolume で戻す先のマスター音量を返す。
		*/
		[[nodiscard]] Float DefaultMasterVolume()const;

		/**
		* [EN]
		* Returns the volume of a category: the value set by the user, else
		* the ACF's value, else 1.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カテゴリの音量を返す。ユーザーが設定した値、無ければ ACF の値、
		* それも無ければ 1。
		*/
		[[nodiscard]] Float CategoryVolume(const String& category)const;

		/**
		* [EN]
		* Returns the names of the categories defined in the registered ACF.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録した ACF に定義されたカテゴリ名の一覧を返す。
		*/
		[[nodiscard]] const DynamicArray<String>& CategoryNameList()const;

	private:
		/// [EN] Voices for uncompressed Wave (PCM) data.
		/// [JP] 非圧縮の Wave(PCM)データを鳴らすボイス。
		CriAtomExVoicePoolHn waveVoicePool_ = nullptr;

		/// [EN] Voices for the compressed CRI codecs, ADX and HCA.
		/// [JP] CRI の圧縮コーデック(ADX と HCA)を鳴らすボイス。
		CriAtomExVoicePoolHn standardVoicePool_ = nullptr;

		/// [EN] D-BAS: shares the streaming bandwidth among the voices that stream from disk.
		/// [JP] D-BAS。ディスクからストリーミングするボイスの間で、読み込み帯域を配分する。
		CriAtomDbasId dbasID_ = CRIATOMEXDBAS_ILLEGAL_ID;

		/// [EN] ACF contents; CRI reads from this buffer, so it must outlive the registration.
		/// [JP] ACF の中身。CRI はこのバッファを参照するので、登録中は保持し続ける。
		DynamicArray<Byte> acfData_;

		/// [EN] Category names defined in the ACF, in ACF order.
		/// [JP] ACF に定義されたカテゴリ名。ACF の順番のまま。
		DynamicArray<String> categoryNames_;

		/// [EN] Volume of each category as defined in the ACF; the target of ResetVolume.
		/// [JP] ACF で定めた各カテゴリの音量。ResetVolume で戻す先。
		FlatMap<String, Float> defaultCategoryVolumes_;

		/// [EN] Category volumes set by the user; saved, and applied over the ACF's values.
		/// [JP] ユーザーが設定したカテゴリ音量。保存され、ACF の値より優先される。
		FlatMap<String, Float> categoryVolumes_;

		/// [EN] Path of Atom Craft's .acf output; empty in the runtime.
		/// [JP] Atom Craft が出力する .acf のパス。ランタイムでは空。
		String masterPath_;

		/// [EN] Current master volume, applied as the volume of master bus 0.
		/// [JP] 現在のマスター音量。マスターバス 0 の音量として反映する。
		Float masterVolume_ = 1.0f;

		/// [EN] Master volume that ResetVolume returns to.
		/// [JP] ResetVolume で戻す先のマスター音量。
		Float defaultMasterVolume_ = 1.0f;
	};

	/**
	* [EN]
	* Error callback registered with CRI: forwards its messages to the
	* engine log, as warnings for IDs starting with 'W' and as errors
	* otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI に登録するエラーコールバック。メッセージをエンジンのログへ流す。
	* ID が 'W' で始まるものは警告、それ以外はエラーとして出す。
	*/
	inline void ScCriErrorCallback(const CriChar8* id, CriUint32 p1, CriUint32 p2, CriUint32* parray)
	{
		/// [EN] CRI encodes the message as an ID plus two parameters; this turns them into readable text.
		/// [JP] CRI はメッセージを ID と2つの引数で表す。これを読める文字列に直す。
		const CriChar8* message = criErr_ConvertIdToMessage(id, p1, p2);

		if (id && id[0] == 'W')
		{
			SC_LOG_WARNING("CRIWARE: {} ({})", message ? message : "", id);
		}
		else
		{
			SC_LOG_ERROR("CRIWARE: {} ({})", message ? message : "", id);
		}
	}
}
