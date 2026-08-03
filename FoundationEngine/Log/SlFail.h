#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Pool/InternPool.h>

/**
* [EN]
* Checks an sl::Result from a Streamline (DLSS) call. On failure,
* forwards the current file/line to SlFail.
*
* ---------------------------------------------------------------------
*
* [JP]
* Streamline（DLSS）呼び出しの sl::Result を検査する。失敗時は現在の
* ファイル/行を SlFail に渡す。
*/
#define SC_SL_CHECK(sr, msg) SeedCore::SlFail(sr, msg, __FILE__, __LINE__)

namespace SeedCore
{
#if !SC_RENDER_DOC_USAGE
	/**
	* [EN]
	* Converts an sl::Result to its enumerator name for display in error messages.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* sl::Result を、エラーメッセージ表示用にその列挙子名の文字列へ変換する。
	*/
	inline std::string SlResultToString(sl::Result sr)
	{
		switch (sr)
		{
		case sl::Result::eOk: return "eOk";
		case sl::Result::eErrorIO: return "eErrorIO";
		case sl::Result::eErrorDriverOutOfDate: return "eErrorDriverOutOfDate";
		case sl::Result::eErrorOSOutOfDate: return "eErrorOSOutOfDate";
		case sl::Result::eErrorOSDisabledHWS: return "eErrorOSDisabledHWS";
		case sl::Result::eErrorDeviceNotCreated: return "eErrorDeviceNotCreated";
		case sl::Result::eErrorNoSupportedAdapterFound: return "eErrorNoSupportedAdapterFound";
		case sl::Result::eErrorAdapterNotSupported: return "eErrorAdapterNotSupported";
		case sl::Result::eErrorNoPlugins: return "eErrorNoPlugins";
		case sl::Result::eErrorVulkanAPI: return "eErrorVulkanAPI";
		case sl::Result::eErrorDXGIAPI: return "eErrorDXGIAPI";
		case sl::Result::eErrorD3DAPI: return "eErrorD3DAPI";
		case sl::Result::eErrorNRDAPI: return "eErrorNRDAPI";
		case sl::Result::eErrorNVAPI: return "eErrorNVAPI";
		case sl::Result::eErrorReflexAPI: return "eErrorReflexAPI";
		case sl::Result::eErrorNGXFailed: return "eErrorNGXFailed";
		case sl::Result::eErrorJSONParsing: return "eErrorJSONParsing";
		case sl::Result::eErrorMissingProxy: return "eErrorMissingProxy";
		case sl::Result::eErrorMissingResourceState: return "eErrorMissingResourceState";
		case sl::Result::eErrorInvalidIntegration: return "eErrorInvalidIntegration";
		case sl::Result::eErrorMissingInputParameter: return "eErrorMissingInputParameter";
		case sl::Result::eErrorNotInitialized: return "eErrorNotInitialized";
		case sl::Result::eErrorComputeFailed: return "eErrorComputeFailed";
		case sl::Result::eErrorInitNotCalled: return "eErrorInitNotCalled";
		case sl::Result::eErrorExceptionHandler: return "eErrorExceptionHandler";
		case sl::Result::eErrorInvalidParameter: return "eErrorInvalidParameter";
		case sl::Result::eErrorMissingConstants: return "eErrorMissingConstants";
		case sl::Result::eErrorDuplicatedConstants: return "eErrorDuplicatedConstants";
		case sl::Result::eErrorMissingOrInvalidAPI: return "eErrorMissingOrInvalidAPI";
		case sl::Result::eErrorCommonConstantsMissing: return "eErrorCommonConstantsMissing";
		case sl::Result::eErrorUnsupportedInterface: return "eErrorUnsupportedInterface";
		case sl::Result::eErrorFeatureMissing: return "eErrorFeatureMissing";
		case sl::Result::eErrorFeatureNotSupported: return "eErrorFeatureNotSupported";
		case sl::Result::eErrorFeatureMissingHooks: return "eErrorFeatureMissingHooks";
		case sl::Result::eErrorFeatureFailedToLoad: return "eErrorFeatureFailedToLoad";
		case sl::Result::eErrorFeatureWrongPriority: return "eErrorFeatureWrongPriority";
		case sl::Result::eErrorFeatureMissingDependency: return "eErrorFeatureMissingDependency";
		case sl::Result::eErrorFeatureManagerInvalidState: return "eErrorFeatureManagerInvalidState";
		case sl::Result::eErrorInvalidState: return "eErrorInvalidState";
		case sl::Result::eWarnOutOfVRAM: return "eWarnOutOfVRAM";
		default: return "Unknown Error";
		}
	}

	/**
	* [EN]
	* Handles a failed sl::Result: does nothing if sr is eOk,
	* otherwise shows a blocking error message box with the failing
	* result/message and breaks into the debugger.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 失敗した sl::Result を処理する。sr が eOk であれば何もしない。
	* それ以外は失敗した結果/メッセージを含むブロッキングのエラー
	* メッセージボックスを表示し、デバッガへブレークする。
	*/
	inline void SlFail(sl::Result sr, const std::string& msg, const Char* file, Int line)
	{
		if (sr == sl::Result::eOk)
		{
			return;
		}

		std::string output = std::format("重要：処理が中断されました。\n\n" "詳細: {}\n" "エラー: {} ({:#010x})\n\n" "場所: {}:{}", msg, SlResultToString(sr), static_cast<Uint32>(sr), file, line);

		std::wstring wideOutput = ConvertToWideString(output);

		MessageBoxW(NULL, wideOutput.c_str(), L"SeedCore Engine - Streamline Notification", MB_ICONERROR | MB_OK);

		__debugbreak();
	}
#endif
}