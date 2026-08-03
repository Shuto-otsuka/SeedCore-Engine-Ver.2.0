#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Pool/InternPool.h>

/**
* [EN]
* Checks an HRESULT from a DirectX call. On failure, forwards the
* current file/line to DxFail.
*
* ---------------------------------------------------------------------
*
* [JP]
* DirectX 呼び出しの HRESULT を検査する。失敗時は現在のファイル/行を
* DxFail に渡す。
*/
#define SC_HR_CHECK(hr, msg) SeedCore::DxFail(hr, msg, __FILE__, __LINE__)

namespace SeedCore
{
	/**
	* [EN]
	* Handles a failed HRESULT: does nothing if hr succeeded, otherwise
	* shows a blocking error message box with the failing HRESULT/message
	* and breaks into the debugger.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 失敗した HRESULT を処理する。hr が成功していれば何もしない。
	* それ以外は失敗した HRESULT/メッセージを含むブロッキングのエラー
	* メッセージボックスを表示し、デバッガへブレークする。
	*/
	inline void DxFail(HRESULT hr, const std::string& msg, const Char* file, Int line)
	{
		if (SUCCEEDED(hr))
		{
			return;
		}

		_com_error error(hr);

		std::string output = std::format("重要：DirectX 処理が失敗しました。\n\n" "詳細: {}\n" "コード: {:#010x}\n" "内容: {}\n\n" "場所: {}:{}", msg, static_cast<Uint32>(hr), ConvertToCharString(error.ErrorMessage()), std::filesystem::path(file).filename().string(), line);

		std::wstring wideOutput = ConvertToWideString(output);

		MessageBoxW(NULL, wideOutput.c_str(), L"SeedCore Engine - DirectX Error", MB_ICONERROR | MB_OK);

		__debugbreak();
	}
}