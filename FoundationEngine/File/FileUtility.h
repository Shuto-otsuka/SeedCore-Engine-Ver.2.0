#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Small helper for whole-file reads.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ファイル全体を読み込むための小さなヘルパー。
	*/
	class SEEDCORE_API FileUtility
	{
	public:
		/**
		* [EN]
		* Reads the entire file at filePath and returns its contents as a
		* std::string. Returns an empty string if the file can't be opened.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* filePath のファイル全体を読み込み、その内容を std::string として
		* 返す。ファイルを開けなければ空文字列を返す。
		*/
		static std::string LoadFileText(String filePath);

		/**
		* [EN]
		* Reads the entire file at filePath and returns its raw bytes.
		* Returns an empty array if the file can't be opened.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* filePath のファイル全体を読み込み、その生バイト列を返す。
		* ファイルを開けなければ空の配列を返す。
		*/
		static DynamicArray<Uint8> LoadFileBinary(String filePath);
	};
}