#include <Editor/Editor/Build/HotReload.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Plugin/PluginHost.h>
#include <FoundationEngine/Plugin/PluginModule.h>
#include <Windows.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] How long a source edit must stay unchanged before a build is triggered. Sits directly on the edit-to-reload path, so it's kept just long enough to outlast an editor's save (a single fast write, unlike a link).
		/// [JP] ソース編集を検知してからビルドを開始するまでに、変化なしで待つ時間。編集からリロードまでの待ち時間に直接乗るため、エディタの保存(リンクとは違い一度の短い書き込み)を取りこぼさない範囲で短くしてある。
		constexpr Uint64 SourceStableWindowMilliseconds = 150;

		/// [EN] How often the UserProject source tree is rescanned, in milliseconds.
		/// [JP] UserProject のソースツリーを再スキャンする間隔(ミリ秒)。
		constexpr Uint64 SourceScanIntervalMilliseconds = 150;
	}

	HotReload::~HotReload()
	{
		if (buildProcessHandle_)
		{
			CloseHandle(buildProcessHandle_);
		}
		if (buildOutputReadPipe_)
		{
			CloseHandle(buildOutputReadPipe_);
		}
	}

	void HotReload::Initialize(PluginHost& pluginHost)
	{
		pluginHost_ = &pluginHost;
		userProjectPlugin_ = pluginHost.FindByStem("UserProject");
	}

	std::filesystem::path HotReload::UserProjectSourceDirectory()
	{
		/// [EN] exeDirectory is Runtime\Build\x64\Debug (or Release); the repo root is three levels up from Runtime\.
		/// [JP] exeDirectory は Runtime\Build\x64\Debug(またはRelease); リポジトリルートは Runtime\ からさらに1つ上。
		Char buffer[MAX_PATH]{};
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		std::filesystem::path exeDirectory = std::filesystem::path(buffer).parent_path();
		std::filesystem::path repositoryRoot = exeDirectory.parent_path().parent_path().parent_path().parent_path();
		return repositoryRoot / "UserProject";
	}

	std::filesystem::path HotReload::UserProjectVcxprojPath()
	{
		return UserProjectSourceDirectory() / "UserProject.vcxproj";
	}

	Uint64 HotReload::GetLastWriteTime(const std::filesystem::path& path)
	{
		WIN32_FILE_ATTRIBUTE_DATA attributeData{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributeData))
		{
			return 0;
		}

		ULARGE_INTEGER writeTime{};
		writeTime.LowPart = attributeData.ftLastWriteTime.dwLowDateTime;
		writeTime.HighPart = attributeData.ftLastWriteTime.dwHighDateTime;
		return writeTime.QuadPart;
	}

	Uint64 HotReload::ScanSourceLastWriteTime()
	{
		std::error_code errorCode;
		std::filesystem::path sourceDirectory = UserProjectSourceDirectory();

		Uint64 newest = 0;

		auto iterator = std::filesystem::recursive_directory_iterator(sourceDirectory, std::filesystem::directory_options::skip_permission_denied, errorCode);
		if (errorCode)
		{
			return 0;
		}

		for (const auto& entry : iterator)
		{
			if (!entry.is_regular_file(errorCode))
			{
				continue;
			}

			std::filesystem::path extension = entry.path().extension();
			if (extension != L".cpp" && extension != L".h" && extension != L".hpp")
			{
				continue;
			}

			/// [EN] The codegen output is rewritten by the build's own pre-build step, so counting it here would make every build's output look like a fresh source edit and trigger another build. The generated file is derived entirely from the headers already being scanned, so ignoring it loses no change detection.
			/// [JP] コード生成の出力はビルド自身のプレビルドステップによって書き換えられるため、ここで数えるとビルドの出力が新しいソース編集に見えてしまい、さらにビルドを誘発してしまう。生成ファイルの内容はここでスキャン済みのヘッダから完全に導出されるので、無視しても変更検知は失われない。
			if (entry.path().filename().wstring().ends_with(L".generated.cpp"))
			{
				continue;
			}

			Uint64 writeTime = GetLastWriteTime(entry.path());
			if (writeTime > newest)
			{
				newest = writeTime;
			}
		}

		return newest;
	}

	std::filesystem::path HotReload::FindMSBuild()
	{
		std::filesystem::path vswhere = LR"(C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe)";
		if (!std::filesystem::exists(vswhere))
		{
			return {};
		}

		std::wstring commandLine = std::format(L"\"{}\" -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe", vswhere.wstring());

		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
		{
			return {};
		}
		SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;

		PROCESS_INFORMATION processInfo{};

		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		Bool created = CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
		CloseHandle(writePipe);

		std::filesystem::path result;

		if (created)
		{
			std::string output;
			Char readBuffer[512]{};
			DWORD bytesRead = 0;
			while (ReadFile(readPipe, readBuffer, sizeof(readBuffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
			{
				output.append(readBuffer, bytesRead);
			}

			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);

			while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
			{
				output.pop_back();
			}

			if (!output.empty())
			{
				result = std::filesystem::path(output);
			}
		}

		CloseHandle(readPipe);

		return result;
	}

	void HotReload::TriggerBuild()
	{
		if (!msbuildResolved_)
		{
			msbuildPath_ = FindMSBuild();
			msbuildResolved_ = true;

			if (msbuildPath_.empty())
			{
				SC_LOG_WARNING("HotReload: MSBuild.exe が見つかりません。UserProject の自動ビルドは無効になります。");
			}
		}

		if (msbuildPath_.empty())
		{
			return;
		}

#ifdef _DEBUG
		const Wchar* configuration = L"Debug";
#else
		const Wchar* configuration = L"Release";
#endif

		/// [EN] UserProject.vcxproj's OutDir/IntDir are defined in terms of $(SolutionDir), which MSBuild only auto-populates when building through Runtime.sln. Since this invokes the .vcxproj directly (bypassing the .sln), SolutionDir must be passed explicitly or the build silently lands outside Runtime\Build\ — where nothing here is watching it.
		/// [JP] UserProject.vcxproj の OutDir/IntDir は $(SolutionDir) を使って定義されているが、これは Runtime.sln 経由でビルドしたときだけ MSBuild が自動設定する。ここでは .sln を介さず .vcxproj を直接呼んでいるため、SolutionDir を明示的に渡さないと、ビルド成果物が誰も監視していない Runtime\Build\ 以外の場所へ静かに出力されてしまう。
		std::filesystem::path solutionDir = UserProjectSourceDirectory().parent_path() / L"Runtime\\";

		/// [EN] A trailing backslash immediately before the closing quote in a Windows command line escapes that quote (\" is read as a literal "), corrupting everything after it — double it so the parser sees one literal backslash followed by a properly closed quote.
		/// [JP] Windowsのコマンドライン解析では、閉じ引用符の直前のバックスラッシュはその引用符をエスケープしてしまう(\" はリテラルな " と解釈される)ため、以降の解析が壊れる — バックスラッシュを2つにすることで、1つのリテラルなバックスラッシュ + 正しく閉じた引用符として解釈させる。
		std::wstring solutionDirArgument = solutionDir.wstring();
		if (!solutionDirArgument.empty() && solutionDirArgument.back() == L'\\')
		{
			solutionDirArgument.push_back(L'\\');
		}

		/// [EN] BuildProjectReferences=false is essential, not an optimization: UserProject.vcxproj references SeedCore.vcxproj, so a default build would walk the whole engine graph and relink SeedCore.dll — which this very Editor process has loaded and therefore holds locked, making the link fail with LNK1104 and the auto-build fail forever. Skipping reference builds links UserProject.dll against the already-built SeedCore.lib instead, which is exactly what a gameplay-script hot-reload needs (engine changes still require a normal full build from Visual Studio).
		/// [JP] BuildProjectReferences=false は最適化ではなく必須: UserProject.vcxproj は SeedCore.vcxproj を参照しているため、既定のビルドではエンジン全体をたどって SeedCore.dll を再リンクしてしまう — その SeedCore.dll はこの Editor プロセス自身がロード中でロックされているので、リンクが LNK1104 で失敗し、自動ビルドが永久に失敗し続ける。参照プロジェクトのビルドを省くことで、代わりにビルド済みの SeedCore.lib に対して UserProject.dll をリンクする — ゲームプレイスクリプトのホットリロードに必要なのはまさにこれ(エンジン側の変更は従来通り Visual Studio でのフルビルドが必要)。

		/// [EN] A debugger attached to this process keeps UserProject.pdb open for as long as the module is loaded — the shadow copy still names the original PDB path internally — so relinking to that same fixed path fails with LNK1201 on every reload. Giving each build its own PDB name sidesteps the lock entirely and keeps hot-reloaded gameplay code breakpoint-able. The debounce in Tick() is far longer than a tick's resolution, so the tick count cannot collide between two builds.
		/// [JP] このプロセスにデバッガがアタッチされていると、モジュールがロードされている間 UserProject.pdb は開かれたままになる(シャドウコピーも内部的には元のPDBパスを指しているため) — そのため同じ固定パスへ再リンクすると、リロードのたびに LNK1201 で失敗する。ビルドごとに別のPDB名を与えることでロックを完全に回避でき、ホットリロードしたゲームプレイコードにブレークポイントも張れるままになる。Tick() のデバウンス時間はティックの分解能より遥かに長いため、2つのビルドでティック値が衝突することはない。
		std::wstring hotReloadPdbName = std::format(L"UserProject_HotReload_{}", GetTickCount64());

		std::wstring commandLine = std::format(L"\"{}\" \"{}\" /nologo /verbosity:minimal /p:Configuration={} /p:Platform=x64 /p:BuildProjectReferences=false /p:HotReloadPdbName={} /p:SolutionDir=\"{}\"", msbuildPath_.wstring(), UserProjectVcxprojPath().wstring(), configuration, hotReloadPdbName, solutionDirArgument);

		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
		{
			SC_LOG_WARNING("HotReload: UserProject の自動ビルド起動に失敗しました(パイプ作成失敗)。");
			return;
		}
		SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = writePipe;
		startupInfo.hStdError = writePipe;
		PROCESS_INFORMATION processInfo{};

		DynamicArray<Wchar> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		Bool created = CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
		CloseHandle(writePipe);

		if (!created)
		{
			SC_LOG_WARNING("HotReload: UserProject の自動ビルド起動に失敗しました。");
			CloseHandle(readPipe);
			return;
		}

		CloseHandle(processInfo.hThread);
		buildProcessHandle_ = processInfo.hProcess;
		buildOutputReadPipe_ = readPipe;
		buildOutput_.clear();

		SC_LOG_NOTICE("HotReload: UserProject の自動ビルドを開始しました。");
	}

	void HotReload::DrainBuildOutput()
	{
		if (!buildOutputReadPipe_)
		{
			return;
		}

		DWORD bytesAvailable = 0;
		while (PeekNamedPipe(buildOutputReadPipe_, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
		{
			Char buffer[512]{};
			DWORD bytesRead = 0;
			if (!ReadFile(buildOutputReadPipe_, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) || bytesRead == 0)
			{
				break;
			}
			buildOutput_.append(buffer, bytesRead);
		}
	}

	void HotReload::PollBuildProcess()
	{
		if (!buildProcessHandle_)
		{
			return;
		}

		DrainBuildOutput();

		if (WaitForSingleObject(buildProcessHandle_, 0) != WAIT_OBJECT_0)
		{
			return;
		}

		/// [EN] One final drain in case output arrived between the last DrainBuildOutput() and the process actually exiting.
		/// [JP] 直前の DrainBuildOutput() 呼び出しからプロセス終了までの間に届いた出力を取りこぼさないよう、最後にもう一度読み出す。
		DrainBuildOutput();

		DWORD exitCode = 0;
		GetExitCodeProcess(buildProcessHandle_, &exitCode);
		CloseHandle(buildProcessHandle_);
		buildProcessHandle_ = nullptr;

		CloseHandle(buildOutputReadPipe_);
		buildOutputReadPipe_ = nullptr;

		if (exitCode == 0)
		{
			SC_LOG_NOTICE("HotReload: UserProject の自動ビルドが完了しました。");

			/// [EN] The build process exiting is a stronger guarantee that the DLL is complete than any timestamp-stability window could be — every handle the linker held is closed by then. Reloading on this instead of waiting for the write-time watcher to settle removes that wait from the edit-to-reload path entirely.
			/// [JP] ビルドプロセスが終了したという事実は、DLLが完全に書き終わっている保証として、タイムスタンプの安定待ちより強い(その時点でリンカが保持していたハンドルは全て閉じられている)。更新時刻監視の安定待ちを待たずにこれを合図としてリロードすることで、編集からリロードまでの待ち時間からその分を丸ごと削れる。
			reloadRequested_ = true;
		}
		else
		{
			SC_LOG_WARNING("HotReload: UserProject の自動ビルドが失敗しました(終了コード {})。\n{}", exitCode, buildOutput_);
		}

		buildOutput_.clear();
	}

	void HotReload::Tick(World& world)
	{
		PollBuildProcess();

		Uint64 nowTick = GetTickCount64();

		if (!buildProcessHandle_ && nowTick - lastSourceScanTick_ >= SourceScanIntervalMilliseconds)
		{
			lastSourceScanTick_ = nowTick;

			Uint64 sourceWriteTime = ScanSourceLastWriteTime();
			if (sourceWriteTime != 0 && sourceWriteTime != lastTriggeredSourceWriteTime_)
			{
				if (sourceWriteTime == pendingSourceWriteTime_)
				{
					if (nowTick - pendingSourceStableSinceTick_ >= SourceStableWindowMilliseconds)
					{
						lastTriggeredSourceWriteTime_ = sourceWriteTime;
						TriggerBuild();
					}
				}
				else
				{
					pendingSourceWriteTime_ = sourceWriteTime;
					pendingSourceStableSinceTick_ = nowTick;
				}
			}
		}

		if (reloadRequested_)
		{
			reloadRequested_ = false;

			/// [EN] The plugin may not have been present at Initialize (e.g. UserProject.dll built for the first time during this session) — pick it up now that a build has produced it.
			/// [JP] Initialize 時点ではプラグインが存在しなかった可能性がある(例: このセッション中に UserProject.dll が初めてビルドされた) — ビルドが生成した今、拾い直す。
			if (!userProjectPlugin_ && pluginHost_)
			{
				userProjectPlugin_ = pluginHost_->FindByStem("UserProject");
			}

			if (pluginHost_ && userProjectPlugin_)
			{
				pluginHost_->ReloadModule(world, *userProjectPlugin_);
			}
		}
	}
}
