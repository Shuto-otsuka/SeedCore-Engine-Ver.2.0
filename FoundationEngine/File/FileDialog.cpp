#include <FoundationEngine/File/FileDialog.h>
#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

namespace SeedCore
{
	namespace
	{
		/**
		* [EN]
		* Builds a Win32 OPENFILENAME-style filter string: "filterName\0filterExt\0\0".
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Win32のOPENFILENAME形式のフィルタ文字列
		* 「filterName\0filterExt\0\0」を構築する。
		*/
		std::wstring MakeFilter(const Wchar* filterName, const Wchar* filterExt)
		{
			std::wstring filter;
			filter += filterName;
			filter.push_back(L'\0');
			filter += filterExt;
			filter.push_back(L'\0');
			filter.push_back(L'\0');
			return filter;
		}
	}

	/**
	* [EN]
	* Shows the Open File dialog filtered by filterName/filterExt
	* (e.g. "Scene Files"/"*.scene"), starting in initialDir. On
	* success, writes the chosen path to outPath and returns true;
	* returns false if the user cancels or the dialog fails.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* filterName/filterExt（例: "Scene Files"/"*.scene"）でフィルタした
	* ファイルを開くダイアログを、initialDir から開始して表示する。
	* 成功時は選択されたパスを outPath に書き込み true を返す。
	* ユーザーがキャンセルしたか、ダイアログが失敗した場合は false。
	*/
	Bool FileDialog::OpenFile(std::filesystem::path& outPath, const std::filesystem::path& initialDir, const Wchar* filterName, const Wchar* filterExt)
	{
		Wchar fileBuffer[MAX_PATH] = {};
		std::wstring filter = MakeFilter(filterName, filterExt);
		std::wstring initialDirStr = initialDir.wstring();

		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GetActiveWindow();
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = fileBuffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = initialDirStr.c_str();
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameW(&ofn))
		{
			return false;
		}

		outPath = fileBuffer;
		return true;
	}

	/**
	* [EN]
	* Shows the Save File dialog filtered by filterName/filterExt,
	* starting in initialDir with defaultExt appended if the user omits
	* one. On success, writes the chosen path to outPath and returns
	* true; returns false if the user cancels or the dialog fails.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* filterName/filterExt でフィルタしたファイルを保存するダイアログを、
	* initialDir から開始して表示する。ユーザーが拡張子を省略した場合は
	* defaultExt が付与される。成功時は選択されたパスを outPath に
	* 書き込み true を返す。ユーザーがキャンセルしたか、ダイアログが
	* 失敗した場合は false。
	*/
	Bool FileDialog::SaveFile(std::filesystem::path& outPath, const std::filesystem::path& initialDir, const Wchar* filterName, const Wchar* filterExt, const Wchar* defaultExt)
	{
		Wchar fileBuffer[MAX_PATH] = {};
		std::wstring filter = MakeFilter(filterName, filterExt);
		std::wstring initialDirStr = initialDir.wstring();

		OPENFILENAMEW ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GetActiveWindow();
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = fileBuffer;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = initialDirStr.c_str();
		ofn.lpstrDefExt = defaultExt;
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetSaveFileNameW(&ofn))
		{
			return false;
		}

		outPath = fileBuffer;
		return true;
	}
}
