#include <Editor/Editor/Build/RuntimeBuilder.h>

namespace SeedCore
{
	namespace
	{
		const DynamicArray<std::string> projectNames =
		{
			"FoundationEngine.vcxproj",
			"AIEngine.vcxproj",
			"PhysicsEngine.vcxproj",
			"AudioEngine.vcxproj",
			"GraphicsEngine.vcxproj",
			"SeedCore.vcxproj",
			"UserProject.vcxproj",
			"Runtime.vcxproj",
		};
	}


	RuntimeBuilder::~RuntimeBuilder()
	{
		if (thread_.joinable())
		{
			thread_.join();
		}
	}

	void RuntimeBuilder::BuildAsync(const std::filesystem::path& projectRoot)
	{
		if (building_)
		{
			return;
		}

		if (thread_.joinable())
		{
			thread_.join();
		}

		building_ = true;

		thread_ = std::thread([this, projectRoot]()
		{
			std::string log;
			Bool success = Build(projectRoot, log);

			{
				std::lock_guard<std::mutex> lock(resultMutex_);
				lastSuccess_ = success;
				lastLog_ = std::move(log);
				hasResult_ = true;
			}

			building_ = false;
		});
	}

	Bool RuntimeBuilder::IsBuilding()const
	{
		return building_;
	}

	Float RuntimeBuilder::GetProgress()const
	{
		return static_cast<Float>(completedProjects_) / static_cast<Float>(projectNames.size());
	}

	Bool RuntimeBuilder::ConsumeResult(Bool& outSuccess, std::string& outLog)
	{
		std::lock_guard<std::mutex> lock(resultMutex_);
		if (!hasResult_)
		{
			return false;
		}

		outSuccess = lastSuccess_;
		outLog = std::move(lastLog_);
		hasResult_ = false;
		return true;
	}

	Bool RuntimeBuilder::RunProcessCaptureOutput(const std::wstring& commandLine, std::string& outResult, const std::function<void(const std::string&)>& onChunk)
	{
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readHandle = nullptr;
		HANDLE writeHandle = nullptr;
		if (!CreatePipe(&readHandle, &writeHandle, &securityAttributes, 0))
		{
			return false;
		}
		SetHandleInformation(readHandle, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(STARTUPINFOW);
		startupInfo.hStdOutput = writeHandle;
		startupInfo.hStdError = writeHandle;
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;

		PROCESS_INFORMATION processInfo{};

		std::wstring mutableCommandLine = commandLine;
		Bool created = CreateProcessW(
			nullptr,
			mutableCommandLine.data(),
			nullptr, nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr, nullptr,
			&startupInfo,
			&processInfo
		);

		CloseHandle(writeHandle);

		if (!created)
		{
			CloseHandle(readHandle);
			return false;
		}

		constexpr DWORD overallTimeoutMs = 10 * 60 * 1000;
		constexpr DWORD drainGraceMs = 2000;

		auto startTime = std::chrono::steady_clock::now();
		Bool processExited = false;
		auto processExitTime = startTime;

		Char buffer[4096];
		for (;;)
		{
			/// [EN] MSBuild's -m spawns multiple worker node processes that all inherit a duplicate of writeHandle, so several of them can be writing to the pipe concurrently. A ReadFile that comes back empty/failed immediately after PeekNamedPipe reported bytes available is a transient hiccup of that concurrency (e.g. a node briefly closing its duplicate handle while another is still open), not proof the pipe is done — treating it as end-of-stream here used to abandon (and then forcibly terminate, see below) a still-running, healthy build the moment this raced once. Only PeekNamedPipe itself failing is a reliable done signal, since that only happens once every process holding a handle to the write end has closed it.
			/// [JP] MSBuildの -m は複数のワーカーノードプロセスを起動し、それぞれが writeHandle の複製を継承するため、複数のプロセスが同時にパイプへ書き込みうる。PeekNamedPipe がバイト有りと報告した直後に ReadFile が空/失敗で返ってくるのは、その並行性による一時的な事象であり(あるノードが複製ハンドルを一瞬閉じている間に別のノードはまだ開いている、等)、パイプが終わった証拠ではない — ここで終端とみなしてしまうと、これが一度でも起きた瞬間、まだ生きている正常なビルドを見捨てて(さらに下で強制終了して)しまっていた。確実な終了シグナルは PeekNamedPipe 自体の失敗だけであり、これは書き込み側のハンドルを持つ全プロセスが閉じたときにのみ起こる。
			DWORD available = 0;
			Bool peekSucceeded = PeekNamedPipe(readHandle, nullptr, 0, nullptr, &available, nullptr);
			if (!peekSucceeded)
			{
				break;
			}

			if (available > 0)
			{
				DWORD bytesRead = 0;
				if (ReadFile(readHandle, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
				{
					outResult.append(buffer, bytesRead);
					if (onChunk)
					{
						onChunk(outResult);
					}
				}
				continue;
			}

			if (!processExited && WaitForSingleObject(processInfo.hProcess, 50) == WAIT_OBJECT_0)
			{
				processExited = true;
				processExitTime = std::chrono::steady_clock::now();
			}

			auto now = std::chrono::steady_clock::now();

			/// [EN] MSBuild spawns cl.exe/link.exe/the Python codegen step as
			///      children that inherit our pipe's write handle; if one of
			///      them keeps it open past MSBuild's own exit, ReadFile would
			///      block forever waiting for EOF that never comes. Once the
			///      watched process has exited, only wait a short grace period
			///      for any trailing buffered output before giving up on it.
			/// [JP] MSBuildはcl.exe/link.exe/Pythonコード生成ステップを子として
			///      起動し、パイプの書き込みハンドルを継承する。そのどれかが
			///      MSBuild自体の終了後もハンドルを握ったままだと、ReadFileは
			///      来ないEOFを永遠に待ってブロックする。監視対象プロセスが
			///      終了したら、残りの出力はごく短い猶予だけ待って諦める。
			if (processExited && std::chrono::duration_cast<std::chrono::milliseconds>(now - processExitTime).count() > drainGraceMs)
			{
				break;
			}

			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > overallTimeoutMs)
			{
				TerminateProcess(processInfo.hProcess, 1);
				break;
			}
		}

		WaitForSingleObject(processInfo.hProcess, 1000);

		DWORD exitCode = 1;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
		{
			TerminateProcess(processInfo.hProcess, 1);
			exitCode = 1;
		}

		CloseHandle(readHandle);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		return exitCode == 0;
	}

	std::optional<std::filesystem::path> RuntimeBuilder::FindMSBuild(const std::filesystem::path& projectRoot)
	{
		std::filesystem::path vswherePath = projectRoot / "Tools" / "Build" / "vswhere.exe";

		std::wstring commandLine =
			L"\"" + vswherePath.wstring() + L"\" "
			L"-latest -products * -requires Microsoft.Component.MSBuild "
			L"-find MSBuild\\**\\Bin\\MSBuild.exe";

		std::string output;
		if (!RunProcessCaptureOutput(commandLine, output))
		{
			return std::nullopt;
		}

		while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
		{
			output.pop_back();
		}

		if (output.empty())
		{
			return std::nullopt;
		}

		return std::filesystem::path(output);
	}

	Bool RuntimeBuilder::Build(const std::filesystem::path& projectRoot, std::string& outLog)
	{
		auto msbuildPath = FindMSBuild(projectRoot);
		if (!msbuildPath)
		{
			outLog = "MSBuildが見つかりませんでした";
			return false;
		}

		std::filesystem::path runtimeProject = projectRoot / "Runtime" / "Runtime.vcxproj";

		/// [EN] $(SolutionDir) is only auto-populated when building through Runtime.sln. This invokes Runtime.vcxproj directly, so without passing it explicitly, every referenced project (SeedCore.vcxproj, FoundationEngine.vcxproj, ...) evaluates $(SolutionDir) as blank and writes its own output under its own project folder instead of Runtime\Build\. That went unnoticed while the engine was statically linked (the linker consumed those .lib files wherever they landed), but now that SeedCore.dll/UserProject.dll are runtime dependencies, RuntimePackager.py — which only copies .dll files it finds in Runtime\Build\...\Release — would silently ship a package missing them.
		/// [JP] $(SolutionDir) は Runtime.sln 経由でビルドしたときだけ MSBuild が自動設定する。ここでは Runtime.vcxproj を直接呼んでいるため、明示的に渡さないと、参照される全プロジェクト(SeedCore.vcxproj、FoundationEngine.vcxproj、…)が $(SolutionDir) を空と評価し、Runtime\Build\ではなく自分自身のプロジェクトフォルダ配下に出力してしまう。エンジンが静的リンクだった頃は気づかれなかった(リンカがその.libをどこにあっても取り込めたため)が、今は SeedCore.dll/UserProject.dll が実行時の依存になっているため、Runtime\Build\...\Release にある.dllしかコピーしない RuntimePackager.py が、それらを含まないパッケージを黙って出荷してしまう。
		/// [EN] Appending "Runtime\\" as a single path component (rather than "Runtime" alone) guarantees the resulting path string ends in a backslash, matching $(SolutionDir)'s own convention. The trailing backslash is then doubled below since, immediately before a closing quote on a Windows command line, a lone backslash escapes that quote instead of closing the path.
		/// [JP] "Runtime" 単体ではなく "Runtime\\" を1つのパス要素として付加することで、結果の文字列が確実に末尾にバックスラッシュを持つようにしている($(SolutionDir) 自身の慣習に合わせるため)。この末尾のバックスラッシュは、Windowsのコマンドライン上で閉じ引用符の直前に単独である場合その引用符をエスケープしてしまうため、下でさらに2つに増やす。
		std::wstring solutionDirArgument = (projectRoot / L"Runtime\\").wstring();
		if (!solutionDirArgument.empty() && solutionDirArgument.back() == L'\\')
		{
			solutionDirArgument.push_back(L'\\');
		}

		std::wstring commandLine =
			L"\"" + msbuildPath->wstring() + L"\" "
			L"\"" + runtimeProject.wstring() + L"\" "
			L"-t:Rebuild -p:Configuration=Release -p:Platform=x64 -m "
			L"\"-p:SolutionDir=" + solutionDirArgument + L"\"";

		projectSeen_.assign(projectNames.size(), false);
		completedProjects_ = 0;

		auto onChunk = [this](const std::string& accumulated)
		{
			for (Size index = 0; index < projectNames.size(); ++index)
			{
				if (!projectSeen_[index] && accumulated.find(projectNames[index]) != std::string::npos)
				{
					projectSeen_[index] = true;
					++completedProjects_;
				}
			}
		};

		if (!RunProcessCaptureOutput(commandLine, outLog, onChunk))
		{
			return false;
		}

		std::time_t now = std::time(nullptr);
		std::tm localTime{};
		localtime_s(&localTime, &now);

		Char timestamp[32];
		std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &localTime);

		std::filesystem::path outputDir = projectRoot / "Package" / (std::string("Build_") + timestamp);

		std::string packageLog;
		Bool packageSuccess = Package(projectRoot, outputDir, packageLog);
		outLog += "\n" + packageLog;

		return packageSuccess;
	}

	Bool RuntimeBuilder::Package(const std::filesystem::path& projectRoot, const std::filesystem::path& outputDir, std::string& outLog)
	{
		std::filesystem::path packagerScript = projectRoot / "Tools" / "Python" / "RuntimePackager.py";

		std::wstring commandLine =
			L"py \"" + packagerScript.wstring() + L"\" "
			L"\"" + projectRoot.wstring() + L"\" "
			L"\"" + outputDir.wstring() + L"\"";

		return RunProcessCaptureOutput(commandLine, outLog);
	}
}