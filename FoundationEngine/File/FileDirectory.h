#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Small helper for querying filesystem locations relative to the
	* running process.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 実行中プロセスに関連するファイルシステムの場所を問い合わせるための
	* 小さなヘルパー。
	*/
	class FileDirectory
	{
	public:
		/**
		* [EN]
		* Returns the directory containing the currently running
		* executable (no trailing slash).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在実行中の実行ファイルを含むディレクトリを返す
		* （末尾のスラッシュなし）。
		*/
		static std::wstring ExecutableDirectory();
	};
}