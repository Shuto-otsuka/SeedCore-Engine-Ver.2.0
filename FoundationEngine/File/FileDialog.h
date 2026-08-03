#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Thin wrapper over Win32's common Open/Save file dialogs
	* (GetOpenFileNameW/GetSaveFileNameW), used by the Editor for
	* asset/project file pickers.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Win32の共通ファイル選択ダイアログ（GetOpenFileNameW/GetSaveFileNameW）
	* の薄いラッパー。Editorのアセット/プロジェクトファイル選択に使う。
	*/
	class SEEDCORE_API FileDialog
	{
	public:
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
		static Bool OpenFile(std::filesystem::path& outPath, const std::filesystem::path& initialDir, const Wchar* filterName, const Wchar* filterExt);

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
		static Bool SaveFile(std::filesystem::path& outPath, const std::filesystem::path& initialDir, const Wchar* filterName, const Wchar* filterExt, const Wchar* defaultExt);
	};
}
