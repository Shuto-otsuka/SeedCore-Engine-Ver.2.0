#include <Editor/Editor/Build/HotReload.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/ECS/PayloadRegistry.h>
#include <Windows.h>
#include <psapi.h>

namespace SeedCore
{
	/// [EN] Returns every key currently in registry, used to snapshot a registry's contents before a module load.
	/// [JP] registry に現在ある全てのキーを返す。モジュールをロードする前にレジストリの内容をスナップショットするために使う。
	template<typename Map>
	static DynamicArray<String> CollectRegistryKeys(const Map& registry)
	{
		DynamicArray<String> keys;
		for (const auto& [name, value] : registry)
		{
			keys.push_back(name);
		}
		return keys;
	}

	/// [EN] Returns the keys present in registry but absent from previousKeys — i.e. those a just-loaded module's static initializers added.
	/// [JP] registry には存在するが previousKeys には無いキー、つまりロードされたばかりのモジュールの静的初期化子が追加したキーを返す。
	template<typename Map>
	static DynamicArray<String> FindAddedRegistryKeys(const Map& registry, const DynamicArray<String>& previousKeys)
	{
		DynamicArray<String> added;
		for (const auto& [name, value] : registry)
		{
			if (std::find(previousKeys.begin(), previousKeys.end(), name) == previousKeys.end())
			{
				added.push_back(name);
			}
		}
		return added;
	}

	/// [EN] How long a timestamp must stay unchanged before it's trusted (MSBuild can touch a file's write time more than once while writing it). Only the externally-built DLL is judged this way now, so it can stay conservative.
	/// [JP] タイムスタンプを信用するまでに変化なしで待つ時間(MSBuildは書き込み中に最終更新時刻を複数回更新することがあるため)。現在この判定を使うのは外部でビルドされたDLLだけなので、余裕を持った値のままでよい。
	static constexpr Uint64 StableWindowMilliseconds = 500;

	/// [EN] How long a source edit must stay unchanged before a build is triggered. Sits directly on the edit-to-reload path, so it's kept just long enough to outlast an editor's save (which is a single fast write, unlike a link).
	/// [JP] ソース編集を検知してからビルドを開始するまでに、変化なしで待つ時間。編集からリロードまでの待ち時間に直接乗るため、エディタの保存(リンクとは違い一度の短い書き込み)を取りこぼさない範囲で短くしてある。
	static constexpr Uint64 SourceStableWindowMilliseconds = 150;

	/// [EN] How often the UserProject source tree is rescanned, in milliseconds.
	/// [JP] UserProject のソースツリーを再スキャンする間隔(ミリ秒)。
	static constexpr Uint64 SourceScanIntervalMilliseconds = 150;

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

	std::filesystem::path HotReload::SourceDllPath()
	{
		Char buffer[MAX_PATH]{};
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		std::filesystem::path exeDirectory = std::filesystem::path(buffer).parent_path();
		return exeDirectory / "UserProject.dll";
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

		CleanupStaleArtifacts();

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

	void HotReload::UnregisterModuleReflection()
	{
		auto& reflectionRegistry = ReflectionRegistry::GetRegistry();
		for (const String& name : registeredReflectionNames_)
		{
			reflectionRegistry.erase(name);
		}
		registeredReflectionNames_.clear();

		auto& payloadRegistry = PayloadRegistry::GetRegistry();
		for (const String& name : registeredPayloadNames_)
		{
			payloadRegistry.erase(name);
		}
		registeredPayloadNames_.clear();
	}

	void HotReload::CleanupStaleArtifacts()const
	{
		std::error_code errorCode;
		std::filesystem::path outputDirectory = SourceDllPath().parent_path();

		std::filesystem::directory_iterator iterator(outputDirectory, errorCode);
		if (errorCode)
		{
			return;
		}

		for (const auto& entry : iterator)
		{
			std::filesystem::path path = entry.path();
			std::wstring name = path.filename().wstring();

			if (!name.starts_with(L"UserProject_"))
			{
				continue;
			}

			std::filesystem::path extension = path.extension();
			if (extension != L".dll" && extension != L".pdb")
			{
				continue;
			}

			if (!shadowPath_.empty() && path == shadowPath_)
			{
				continue;
			}

			std::error_code removeError;
			std::filesystem::remove(path, removeError);
		}
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

	Bool HotReload::IsOwnedByLoadedModule(ComponentID id)const
	{
		if (!handle_)
		{
			return false;
		}

		const void* address = ComponentRegistry::Get(id).construct_;
		if (!address)
		{
			return false;
		}

		MODULEINFO moduleInfo{};
		if (!K32GetModuleInformation(GetCurrentProcess(), handle_, &moduleInfo, sizeof(moduleInfo)))
		{
			return false;
		}

		const Byte* base = static_cast<const Byte*>(moduleInfo.lpBaseOfDll);
		const Byte* target = static_cast<const Byte*>(address);
		return target >= base && target < base + moduleInfo.SizeOfImage;
	}

	void HotReload::CaptureAndDestroyOwnedComponents(World& world)
	{
		capturedComponents_.clear();

		for (const ResourcePtr<Actor>& actorPtr : world.GetActors())
		{
			Actor* actor = actorPtr.get();

			DynamicArray<ComponentID> ownedIDs;
			for (ComponentID id : actor->ComponentBaseIDList())
			{
				if (IsOwnedByLoadedModule(id))
				{
					ownedIDs.push_back(id);
				}
			}

			for (ComponentID id : ownedIDs)
			{
				void* data = world.GetComponent(actor->GetEntity(), id);
				if (data)
				{
					String name = ComponentRegistry::GetName(id);
					capturedComponents_.emplace_back(actor, CaptureComponent(name, data));
				}

				actor->RemoveComponent(id);
			}
		}

		/// [EN] Collected during a read-only pass over GetRegistry() and unregistered afterwards — ComponentRegistry::Unregister() erases from the very map GetRegistry() returns a reference to, so mutating it mid-iteration would be unsafe.
		/// [JP] GetRegistry() を読み取り専用で走査する間に集めておき、走査後にまとめて登録解除する — ComponentRegistry::Unregister() は GetRegistry() が参照を返しているまさにそのマップから削除するため、走査中に変更するのは安全ではない。
		DynamicArray<ComponentID> ownedComponentIDs;
		for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
		{
			if (!IsOwnedByLoadedModule(id))
			{
				continue;
			}

			if (metadata.storage_ == ComponentStorage::SparseSet)
			{
				world.UnregisterSparseSetStorage(id);
			}

			ownedComponentIDs.push_back(id);
		}

		/// [EN] Must happen before FreeLibrary() unloads the module (Unload() calls this before FreeLibrary()) — otherwise a stale std::type_index left behind in ComponentRegistry::TypeIndex() would point at a type_info living in freed memory, crashing the next Register<T>() call that happens to probe the same slot.
		/// [JP] FreeLibrary() がモジュールをアンロードするより前に行う必要がある(Unload() はこれを FreeLibrary() より前に呼ぶ) — そうしなければ ComponentRegistry::TypeIndex() に残った古い std::type_index が解放済みメモリ上の type_info を指したままになり、以後同じスロットを探査する Register<T>() 呼び出しでクラッシュする。
		for (ComponentID id : ownedComponentIDs)
		{
			ComponentRegistry::Unregister(id);
		}
	}

	void HotReload::RestoreCapturedComponents(World& world)
	{
		for (const auto& [actor, component] : capturedComponents_)
		{
			ComponentID id = ComponentRegistry::GetComponentID(component.componentName_);
			if (!id)
			{
				continue;
			}

			actor->AddComponent(id);

			void* data = world.GetComponent(actor->GetEntity(), id);
			if (data)
			{
				ApplyComponent(component, data);
			}
		}

		capturedComponents_.clear();
	}

	Bool HotReload::Load(World& world)
	{
		std::filesystem::path sourcePath = SourceDllPath();

		Uint64 writeTime = GetLastWriteTime(sourcePath);
		if (writeTime == 0)
		{
			SC_LOG_WARNING("HotReload: UserProject.dll が見つかりません。");
			return false;
		}

		std::filesystem::path shadowPath = sourcePath;
		shadowPath.replace_filename(std::format(L"UserProject_{}.dll", writeTime));

		/// [EN] The linker's own process has already exited (guaranteeing the file is fully written), but a brief exclusive lock right after that — most commonly Windows Defender's real-time scan of a freshly written executable — can still make the very next copy attempt fail with a sharing violation. Retrying a few times with a short wait clears this without needing to fall all the way back to the slower timestamp-stability watcher.
		/// [JP] リンカのプロセス自体は既に終了している(ファイルが書き終わっている保証はある)が、その直後の一瞬の排他ロック — 多くの場合 Windows Defender による書き込み直後の実行ファイルへのリアルタイムスキャン — によって、直後のコピー試行が共有違反で失敗することがある。短い待機を挟んで数回リトライすれば、より遅いタイムスタンプ安定待ちまで後退せずに解消できる。
		std::error_code errorCode;
		constexpr Int maxAttempts = 5;
		for (Int attempt = 0; attempt < maxAttempts; ++attempt)
		{
			errorCode.clear();
			std::filesystem::copy_file(sourcePath, shadowPath, std::filesystem::copy_options::overwrite_existing, errorCode);
			if (!errorCode)
			{
				break;
			}
			Sleep(50);
		}

		if (errorCode)
		{
			SC_LOG_WARNING("HotReload: UserProject.dll のシャドウコピーに失敗しました。");
			return false;
		}

		/// [EN] Snapshotted before the load so the module's own static initializers, which run inside LoadLibraryW, show up as a clean diff afterwards. Diffing is used instead of naming types explicitly because reflection is also registered for non-component types (nested structs), which no other registry enumerates.
		/// [JP] ロード前にスナップショットを取ることで、LoadLibraryW の内部で走るモジュール自身の静的初期化子による追加を、後から差分として綺麗に取り出せる。型名を明示的に列挙せず差分を使うのは、リフレクションがコンポーネント以外の型(ネストされた構造体)に対しても登録され、それらを列挙できるレジストリが他に無いため。
		DynamicArray<String> reflectionKeysBefore = CollectRegistryKeys(ReflectionRegistry::GetRegistry());
		DynamicArray<String> payloadKeysBefore = CollectRegistryKeys(PayloadRegistry::GetRegistry());

		HMODULE handle = LoadLibraryW(shadowPath.c_str());
		if (!handle)
		{
			SC_LOG_WARNING("HotReload: UserProject.dll のロードに失敗しました。");
			return false;
		}

		registeredReflectionNames_ = FindAddedRegistryKeys(ReflectionRegistry::GetRegistry(), reflectionKeysBefore);
		registeredPayloadNames_ = FindAddedRegistryKeys(PayloadRegistry::GetRegistry(), payloadKeysBefore);

		auto onGameLoad = reinterpret_cast<OnGameLoadFn>(GetProcAddress(handle, "SC_OnGameLoad"));
		auto onGameUnload = reinterpret_cast<OnGameUnloadFn>(GetProcAddress(handle, "SC_OnGameUnload"));
		auto setImGuiContext = reinterpret_cast<SetImGuiContextFn>(GetProcAddress(handle, "SC_SetImGuiContext"));

		if (!onGameLoad || !onGameUnload)
		{
			SC_LOG_WARNING("HotReload: SC_OnGameLoad/SC_OnGameUnload が見つかりません。");

			/// [EN] The module's initializers already ran, so its registry entries exist even though the load is being abandoned; they must go before FreeLibrary or they would outlive their own code with no later Unload to clean them up (handle_ is never set on this path).
			/// [JP] モジュールの初期化子は既に走っているため、ロードを中止する場合でもレジストリのエントリは存在している。この経路では handle_ が設定されず後の Unload で片付けられないので、FreeLibrary より前に削除しなければ、自身のコードより長く生き残ってしまう。
			UnregisterModuleReflection();

			FreeLibrary(handle);
			return false;
		}

		handle_ = handle;
		shadowPath_ = shadowPath;
		lastWriteTime_ = writeTime;
		onGameLoad_ = onGameLoad;
		onGameUnload_ = onGameUnload;
		setImGuiContext_ = setImGuiContext;

		/// [EN] Must run before onGameLoad_, since UserProject.dll's own ImGui copy is otherwise uninitialized.
		/// [JP] onGameLoad_ より先に実行する必要がある — そうしないと UserProject.dll 自身のImGuiコピーが未初期化のままになる。
		if (setImGuiContext_)
		{
			setImGuiContext_(ImGui::GetCurrentContext());
		}

		onGameLoad_(world);

		RestoreCapturedComponents(world);

		SC_LOG_NOTICE("HotReload: UserProject.dll をロードしました。");

		return true;
	}

	void HotReload::Unload(World& world)
	{
		if (!handle_)
		{
			return;
		}

		/// [EN] Runs first: capturing field values goes through ReflectionRegistry, so the entries must still be present and callable at this point.
		/// [JP] 最初に実行する: フィールド値の取得は ReflectionRegistry を経由するため、この時点ではエントリがまだ存在し呼び出し可能である必要がある。
		CaptureAndDestroyOwnedComponents(world);

		onGameUnload_(world);

		UnregisterModuleReflection();

		FreeLibrary(handle_);

		std::error_code errorCode;
		std::filesystem::remove(shadowPath_, errorCode);

		handle_ = nullptr;
		onGameLoad_ = nullptr;
		onGameUnload_ = nullptr;
		setImGuiContext_ = nullptr;
		shadowPath_.clear();
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
			Unload(world);
			Load(world);
			return;
		}

		/// [EN] Still watched so a build started outside the editor (a normal Visual Studio build of UserProject) is picked up too. Builds this class started take the reloadRequested_ path above and never reach here, because Load() refreshes lastWriteTime_.
		/// [JP] エディタの外で行われたビルド(Visual Studio による通常の UserProject ビルド)も拾えるよう、監視自体は残している。このクラスが起動したビルドは上の reloadRequested_ の経路を通り、Load() が lastWriteTime_ を更新するため、ここには到達しない。
		Uint64 dllWriteTime = GetLastWriteTime(SourceDllPath());
		if (dllWriteTime != 0 && dllWriteTime != lastWriteTime_)
		{
			if (dllWriteTime == pendingDllWriteTime_)
			{
				if (nowTick - pendingDllStableSinceTick_ >= StableWindowMilliseconds)
				{
					Unload(world);
					Load(world);
				}
			}
			else
			{
				pendingDllWriteTime_ = dllWriteTime;
				pendingDllStableSinceTick_ = nowTick;
			}
		}
	}
}
