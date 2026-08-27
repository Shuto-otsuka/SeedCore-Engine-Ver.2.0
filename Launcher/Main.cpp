#include <Windows.h>
#include <string>
#include <filesystem>

// Package-root launcher: starts Plugins/Runtime.exe with its working directory
// set to Plugins/ - the same directory, so the loader resolves SeedCore.dll and
// the third-party DLLs next to it, and the engine's "../CompiledShaderObject" /
// "../UserProject" relative paths resolve against the package root one level up.
// Forwards the command line and mirrors the child's exit code.

namespace
{
	std::wstring ArgumentsAfterProgramName(const wchar_t* commandLine)
	{
		const wchar_t* cursor = commandLine;

		if (*cursor == L'"')
		{
			++cursor;
			while (*cursor && *cursor != L'"')
			{
				++cursor;
			}
			if (*cursor == L'"')
			{
				++cursor;
			}
		}
		else
		{
			while (*cursor && *cursor != L' ' && *cursor != L'\t')
			{
				++cursor;
			}
		}

		while (*cursor == L' ' || *cursor == L'\t')
		{
			++cursor;
		}

		return std::wstring(cursor);
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	wchar_t modulePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

	std::filesystem::path root = std::filesystem::path(modulePath).parent_path();
	std::filesystem::path pluginsDirectory = root / L"Plugins";
	std::filesystem::path targetExe = pluginsDirectory / L"Runtime.exe";

	if (!std::filesystem::exists(targetExe))
	{
		std::wstring message = L"起動できませんでした。本体が見つかりません:\n" + targetExe.wstring();
		MessageBoxW(nullptr, message.c_str(), L"Launcher", MB_OK | MB_ICONERROR);
		return 1;
	}

	std::wstring commandLine = L"\"" + targetExe.wstring() + L"\"";
	std::wstring forwarded = ArgumentsAfterProgramName(GetCommandLineW());
	if (!forwarded.empty())
	{
		commandLine += L" " + forwarded;
	}

	std::wstring mutableCommandLine = commandLine;

	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo{};

	BOOL created = CreateProcessW(targetExe.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, pluginsDirectory.c_str(), &startupInfo, &processInfo);
	if (!created)
	{
		std::wstring message = L"本体の起動に失敗しました (エラーコード " + std::to_wstring(GetLastError()) + L")";
		MessageBoxW(nullptr, message.c_str(), L"Launcher", MB_OK | MB_ICONERROR);
		return 1;
	}

	CloseHandle(processInfo.hThread);

	WaitForSingleObject(processInfo.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);
	CloseHandle(processInfo.hProcess);

	return static_cast<int>(exitCode);
}
