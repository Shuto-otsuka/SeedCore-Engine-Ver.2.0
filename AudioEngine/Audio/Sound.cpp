#include <AudioEngine/Audio/Sound.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns the stored sound representation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 保持しているサウンド形式を返す。
	*/
	SoundType Sound::Type()const
	{
		return type_;
	}

	/**
	* [EN]
	* Returns the loaded ACB handle for a cue sheet.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キューシート用に読み込まれた ACB ハンドルを返す。
	*/
	CriAtomExAcbHn Sound::AcbHandle()const
	{
		return acbHandle_;
	}

	/**
	* [EN]
	* Returns writable access to the in-memory audio bytes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メモリ上の音声バイト列へ書き込み可能なポインターを返す。
	*/
	void* Sound::Data()
	{
		return data_.data();
	}

	/**
	* [EN]
	* Returns the size of the in-memory audio data in bytes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メモリ上の音声データのバイト数を返す。
	*/
	Size Sound::DataSize()const
	{
		return data_.size();
	}

	/**
	* [EN]
	* Returns the virtual path used to stream the associated AWB data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 関連する AWB データのストリームに使う仮想パスを返す。
	*/
	const String& Sound::AwbPath()const
	{
		return awbPath_;
	}
}
