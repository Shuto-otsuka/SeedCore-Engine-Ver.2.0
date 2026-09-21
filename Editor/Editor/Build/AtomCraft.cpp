#include <Editor/Editor/Build/AtomCraft.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	String AtomCraft::Detect()
	{
		std::error_code errorCode;

		Size environmentLength = 0;
		Char environmentBuffer[MAX_PATH]{};
		if (getenv_s(&environmentLength, environmentBuffer, MAX_PATH, "CRI_ATOMCRAFT") == 0 && environmentLength > 0)
		{
			std::filesystem::path overridePath(environmentBuffer);
			if (std::filesystem::is_directory(overridePath, errorCode))
			{
				overridePath /= "CriAtomCraft.exe";
			}

			if (std::filesystem::exists(overridePath, errorCode))
			{
				std::string overrideNativePath = overridePath.string();
				std::ranges::replace(overrideNativePath, '\\', '/');
				return String(overrideNativePath);
			}
		}

		DynamicArray<std::filesystem::path> roots;

		Size profileLength = 0;
		Char profileBuffer[MAX_PATH]{};
		if (getenv_s(&profileLength, profileBuffer, MAX_PATH, "USERPROFILE") == 0 && profileLength > 0)
		{
			std::filesystem::path profilePath(profileBuffer);
			roots.push_back(profilePath / "Desktop");
			roots.push_back(profilePath / "Downloads");
			roots.push_back(profilePath / "Documents");
			roots.push_back(profilePath);
		}

		for (const Char* variable : { "ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA" })
		{
			Size length = 0;
			Char buffer[MAX_PATH]{};
			if (getenv_s(&length, buffer, MAX_PATH, variable) == 0 && length > 0)
			{
				roots.push_back(std::filesystem::path(buffer));
			}
		}

		roots.push_back("C:/");
		roots.push_back("D:/");

		static const std::set<std::string> excludedDirectories = { "windows", "windows.old", "winsxs", "$recycle.bin", "system volume information", "programdata", "node_modules" };

		DynamicArray<std::filesystem::path> searchPaths = roots;
		DynamicArray<std::filesystem::path> frontier = roots;
		for (Size depth = 0; depth < 2; ++depth)
		{
			DynamicArray<std::filesystem::path> expanded;
			for (const std::filesystem::path& candidate : frontier)
			{
				for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(candidate, std::filesystem::directory_options::skip_permission_denied, errorCode))
				{
					if (!entry.is_directory(errorCode))
					{
						continue;
					}

					std::string directoryName = entry.path().filename().string();
					std::ranges::transform(directoryName, directoryName.begin(), [](Uchar character) { return static_cast<Char>(std::tolower(character)); });
					if (excludedDirectories.contains(directoryName))
					{
						continue;
					}

					expanded.push_back(entry.path());
					searchPaths.push_back(entry.path());
				}
			}

			frontier = std::move(expanded);
		}

		DynamicArray<std::pair<Uint64, std::string>> candidates;
		for (const std::filesystem::path& searchPath : searchPaths)
		{
			std::filesystem::path toolRoot = searchPath / "cri" / "tools" / "ADX2LE";
			if (!std::filesystem::is_directory(toolRoot, errorCode))
			{
				continue;
			}

			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(toolRoot, errorCode))
			{
				std::filesystem::path executablePath = entry.path() / "CriAtomCraft.exe";
				if (!std::filesystem::exists(executablePath, errorCode))
				{
					continue;
				}

				std::string nativePath = executablePath.string();
				std::ranges::replace(nativePath, '\\', '/');
				if (std::ranges::any_of(candidates, [&nativePath](const std::pair<Uint64, std::string>& candidate) { return candidate.second == nativePath; }))
				{
					continue;
				}

				Uint64 version = 0;
				Uint32 infoSize = GetFileVersionInfoSizeA(nativePath.c_str(), nullptr);
				if (infoSize > 0)
				{
					DynamicArray<Byte> infoBuffer(infoSize);
					if (GetFileVersionInfoA(nativePath.c_str(), 0, infoSize, infoBuffer.data()))
					{
						VS_FIXEDFILEINFO* fixedInfo = nullptr;
						Uint32 fixedInfoSize = 0;
						if (VerQueryValueA(infoBuffer.data(), "\\", reinterpret_cast<void**>(&fixedInfo), &fixedInfoSize) && fixedInfo)
						{
							version = (static_cast<Uint64>(fixedInfo->dwFileVersionMS) << 32) | fixedInfo->dwFileVersionLS;
						}
					}
				}

				candidates.push_back({ version, nativePath });
			}
		}

		if (candidates.empty())
		{
			return String();
		}

		std::ranges::sort(candidates, [](const std::pair<Uint64, std::string>& first, const std::pair<Uint64, std::string>& second)
		{
			if (first.first != second.first)
			{
				return first.first > second.first;
			}
			if (first.second.size() != second.second.size())
			{
				return first.second.size() < second.second.size();
			}
			return first.second < second.second;
		});

		if (candidates.size() > 1)
		{
			SC_LOG_WARNING("CRI Atom Craft が {} 箇所で見つかりました。{} を使用します（環境変数 CRI_ATOMCRAFT で上書き可）。", candidates.size(), candidates.front().second);
			for (const std::pair<Uint64, std::string>& candidate : candidates)
			{
				SC_LOG_WARNING("CRI Atom Craft 候補: {} (Ver {}.{}.{})", candidate.second, (candidate.first >> 48) & 0xFFFF, (candidate.first >> 32) & 0xFFFF, (candidate.first >> 16) & 0xFFFF);
			}
		}

		return String(candidates.front().second);
	}

	Bool AtomCraft::Open(const String& executablePath, const String& projectPath)
	{
		std::error_code errorCode;

		if (executablePath.view().empty() || !std::filesystem::exists(executablePath.c_str(), errorCode))
		{
			SC_LOG_WARNING("CRI Atom Craft が見つかりません。ADX LE ツールを導入してください: https://game.criware.jp/products/adx-le/");
			return false;
		}

		if (!projectPath.view().empty() && !std::filesystem::exists(projectPath.c_str(), errorCode))
		{
			SC_LOG_WARNING("Atom Craft プロジェクトが見つかりません ({})", projectPath.str());
			return false;
		}

		std::wstring wideExecutablePath = std::filesystem::path(executablePath.c_str()).wstring();
		std::wstring wideProjectPath = projectPath.view().empty() ? std::wstring() : L"\"" + std::filesystem::path(projectPath.c_str()).wstring() + L"\"";
		std::wstring wideWorkingDirectory = std::filesystem::path(executablePath.c_str()).parent_path().wstring();

		HINSTANCE result = ShellExecuteW(nullptr, L"open", wideExecutablePath.c_str(), wideProjectPath.empty() ? nullptr : wideProjectPath.c_str(), wideWorkingDirectory.c_str(), SW_SHOWNORMAL);
		if (reinterpret_cast<INT_PTR>(result) <= 32)
		{
			SC_LOG_WARNING("CRI Atom Craft の起動に失敗しました ({})", executablePath.str());
			return false;
		}

		return true;
	}
}
