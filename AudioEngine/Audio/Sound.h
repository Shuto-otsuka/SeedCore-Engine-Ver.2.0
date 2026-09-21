#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Identifies the representation stored by a Sound resource.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Sound リソースが保持するデータ形式を表す。
	*/
	enum class SoundType :Uint32
	{
		/// [EN] CRI cue-sheet data loaded as an ACB.
		/// [JP] ACB として読み込む CRI キューシートデータ。
		CueSheet,

		/// [EN] Waveform data played directly from memory.
		/// [JP] メモリから直接再生する波形データ。
		Wave,
	};

	/**
	* [EN]
	* Loaded audio data together with optional CRI cue-sheet and streamed-AWB
	* state. Instances are created and populated by AudioLoader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 読み込み済み音声データと、任意の CRI キューシートおよびストリーム
	* AWB の状態を保持する。インスタンスの生成と設定は AudioLoader が行う。
	*/
	class SEEDCORE_API Sound :public NonCopyable
	{
	private:
		friend class AudioLoader;

	public:
		/**
		* [EN]
		* Creates an empty wave Sound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空の波形 Sound を生成する。
		*/
		Sound() = default;

		/**
		* [EN]
		* Destroys the stored Sound state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 保持している Sound の状態を破棄する。
		*/
		~Sound() = default;

		/**
		* [EN]
		* Moves loaded Sound state from another instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 別のインスタンスから読み込み済み Sound の状態を移動する。
		*/
		Sound(Sound&&)noexcept = default;

		/**
		* [EN]
		* Replaces this instance with moved Sound state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このインスタンスを、移動された Sound の状態で置き換える。
		*/
		Sound& operator=(Sound&&)noexcept = default;

	public:
		/**
		* [EN]
		* Returns the stored sound representation.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 保持しているサウンド形式を返す。
		*/
		[[nodiscard]] SoundType Type()const;

		/**
		* [EN]
		* Returns the loaded ACB handle for a cue sheet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キューシート用に読み込まれた ACB ハンドルを返す。
		*/
		[[nodiscard]] CriAtomExAcbHn AcbHandle()const;

		/**
		* [EN]
		* Returns writable access to the in-memory audio bytes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メモリ上の音声バイト列へ書き込み可能なポインターを返す。
		*/
		[[nodiscard]] void* Data();

		/**
		* [EN]
		* Returns the size of the in-memory audio data in bytes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メモリ上の音声データのバイト数を返す。
		*/
		[[nodiscard]] Size DataSize()const;

		/**
		* [EN]
		* Returns the virtual path used to stream the associated AWB data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 関連する AWB データのストリームに使う仮想パスを返す。
		*/
		[[nodiscard]] const String& AwbPath()const;

	private:
		/// [EN] Representation of the loaded audio data.
		/// [JP] 読み込まれた音声データの形式。
		SoundType type_ = SoundType::Wave;

		/// [EN] CRI ACB handle owned for cue-sheet playback.
		/// [JP] キューシート再生用に所有する CRI ACB ハンドル。
		CriAtomExAcbHn acbHandle_ = nullptr;

		/// [EN] Decrypted ACB or wave data retained in memory.
		/// [JP] メモリ上に保持する復号済み ACB または波形データ。
		DynamicArray<Byte> data_;

		/// [EN] Virtual path to an optional streamed AWB blob.
		/// [JP] 任意のストリーム AWB ブロブを示す仮想パス。
		String awbPath_;
	};
}
