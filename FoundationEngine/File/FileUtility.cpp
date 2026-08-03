#include <FoundationEngine/File/FileUtility.h>

namespace SeedCore
{
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
	std::string FileUtility::LoadFileText(String filePath)
	{
		std::ifstream ifs(filePath.c_str(), std::ios::binary);
		if (!ifs)
		{
			return "";
		}
		return std::string((std::istreambuf_iterator<Char>(ifs)), std::istreambuf_iterator<Char>());
	}

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
	DynamicArray<Uint8> FileUtility::LoadFileBinary(String filePath)
	{
		std::ifstream ifs(filePath.c_str(), std::ios::binary | std::ios::ate);
		if (!ifs)
		{
			return {};
		}
		Size size = static_cast<Size>(ifs.tellg());
		DynamicArray<Uint8> buffer(size);
		ifs.seekg(0, std::ios::beg);
		ifs.read(reinterpret_cast<Char*>(buffer.data()), size);
		return buffer;
	}
}