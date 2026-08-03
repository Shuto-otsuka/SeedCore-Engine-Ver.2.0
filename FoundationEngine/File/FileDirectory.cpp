#include <FoundationEngine/File/FileDirectory.h>

namespace SeedCore
{
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
	std::wstring FileDirectory::ExecutableDirectory()
	{
		Wchar buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring path(buffer);
		return path.substr(0, path.find_last_of(L"\\/"));
	}
}