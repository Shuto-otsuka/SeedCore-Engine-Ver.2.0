#include <FoundationEngine/Plugin/PluginModule.h>
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
	/**
	* [EN]
	* Constructs a module bound to sourcePath (the real DLL on disk), not
	* yet loaded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* sourcePath（ディスク上の実 DLL）に紐づくモジュールを構築する。
	* この時点ではまだロードされていない。
	*/
	PluginModule::PluginModule(const std::filesystem::path& sourcePath) : sourcePath_(sourcePath)
	{
		/// No Code
	}

	PluginModule::~PluginModule()
	{
		/// No Code
	}

	/**
	* [EN]
	* Shadow-copies the source DLL, loads it, resolves its entry points,
	* forwards imguiContext, calls SC_OnGameLoad, then restores any
	* components captured by a prior Unload.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 元 DLL をシャドウコピーしてロードし、エントリポイントを解決し、
	* imguiContext を渡し、SC_OnGameLoad を呼び、直前の Unload で取得した
	* コンポーネントがあれば復元する。
	*/
	Bool PluginModule::Load(World& world, ImGuiContext* imguiContext)
	{
		Uint64 writeTime = GetLastWriteTime(sourcePath_);
		if (writeTime == 0)
		{
			SC_LOG_WARNING("Plugin: {} が見つかりません。", sourcePath_.filename().string());
			return false;
		}

		CleanupStaleArtifacts();

		std::filesystem::path shadowPath = sourcePath_;
		shadowPath.replace_filename(std::format(L"{}_{}.dll", sourcePath_.stem().wstring(), writeTime));

		/// [EN] The linker's own process has already exited (guaranteeing the file is fully written), but a brief exclusive lock right after that - most commonly Windows Defender's real-time scan of a freshly written executable - can still make the very next copy attempt fail with a sharing violation. Retrying a few times with a short wait clears this.
		/// [JP] リンカのプロセス自体は既に終了している（ファイルが書き終わっている保証はある）が、その直後の一瞬の排他ロック - 多くの場合 Windows Defender による書き込み直後の実行ファイルへのリアルタイムスキャン - によって、直後のコピー試行が共有違反で失敗することがある。短い待機を挟んで数回リトライすれば解消できる。
		std::error_code errorCode;
		constexpr Int maxAttempts = 5;
		for (Int attempt = 0; attempt < maxAttempts; ++attempt)
		{
			errorCode.clear();
			std::filesystem::copy_file(sourcePath_, shadowPath, std::filesystem::copy_options::overwrite_existing, errorCode);
			if (!errorCode)
			{
				break;
			}
			Sleep(50);
		}

		if (errorCode)
		{
			SC_LOG_WARNING("Plugin: {} のシャドウコピーに失敗しました。", sourcePath_.filename().string());
			return false;
		}

		/// [EN] Snapshotted before the load so the module's own static initializers, which run inside LoadLibraryW, show up as a clean diff afterwards. Diffing is used instead of naming types explicitly because reflection is also registered for non-component types (nested structs), which no other registry enumerates.
		/// [JP] ロード前にスナップショットを取ることで、LoadLibraryW の内部で走るモジュール自身の静的初期化子による追加を、後から差分として綺麗に取り出せる。型名を明示的に列挙せず差分を使うのは、リフレクションがコンポーネント以外の型（ネストされた構造体）に対しても登録され、それらを列挙できるレジストリが他に無いため。
		DynamicArray<String> reflectionKeysBefore = CollectRegistryKeys(ReflectionRegistry::GetRegistry());
		DynamicArray<String> payloadKeysBefore = CollectRegistryKeys(PayloadRegistry::GetRegistry());

		/// [EN] LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets a plugin resolve its own sidecar DLLs from the plugin directory; LOAD_LIBRARY_SEARCH_DEFAULT_DIRS keeps the application directory (and any AddDllDirectory paths, e.g. the engine's) in the search set so SeedCore.dll still resolves.
		/// [JP] LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR により、プラグインは自身の付随 DLL をプラグインディレクトリから解決できる; LOAD_LIBRARY_SEARCH_DEFAULT_DIRS はアプリケーションディレクトリ（および AddDllDirectory で追加したパス、例: エンジンの）を検索対象に残すため、SeedCore.dll も解決できる。
		HMODULE handle = LoadLibraryExW(shadowPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!handle)
		{
			SC_LOG_WARNING("Plugin: {} のロードに失敗しました。", sourcePath_.filename().string());
			return false;
		}

		registeredReflectionNames_ = FindAddedRegistryKeys(ReflectionRegistry::GetRegistry(), reflectionKeysBefore);
		registeredPayloadNames_ = FindAddedRegistryKeys(PayloadRegistry::GetRegistry(), payloadKeysBefore);

		auto onGameLoad = reinterpret_cast<OnGameLoadFn>(GetProcAddress(handle, "SC_OnGameLoad"));
		auto onGameUnload = reinterpret_cast<OnGameUnloadFn>(GetProcAddress(handle, "SC_OnGameUnload"));
		auto setImGuiContext = reinterpret_cast<SetImGuiContextFn>(GetProcAddress(handle, "SC_SetImGuiContext"));

		if (!onGameLoad || !onGameUnload)
		{
			SC_LOG_WARNING("Plugin: {} に SC_OnGameLoad/SC_OnGameUnload が見つかりません。", sourcePath_.filename().string());

			/// [EN] The module's initializers already ran, so its registry entries exist even though the load is being abandoned; they must go before FreeLibrary or they would outlive their own code with no later Unload to clean them up (handle_ is never set on this path).
			/// [JP] モジュールの初期化子は既に走っているため、ロードを中止する場合でもレジストリのエントリは存在している。この経路では handle_ が設定されず後の Unload で片付けられないので、FreeLibrary より前に削除しなければ、自身のコードより長く生き残ってしまう。
			UnregisterModuleReflection();

			FreeLibrary(handle);

			std::error_code shadowRemoveError;
			std::filesystem::remove(shadowPath, shadowRemoveError);
			return false;
		}

		handle_ = handle;
		shadowPath_ = shadowPath;
		lastWriteTime_ = writeTime;
		onGameLoad_ = onGameLoad;
		onGameUnload_ = onGameUnload;
		setImGuiContext_ = setImGuiContext;

		/// [EN] Must run before onGameLoad_, since the plugin's own ImGui copy is otherwise uninitialized.
		/// [JP] onGameLoad_ より先に実行する必要がある - そうしないとプラグイン自身の ImGui コピーが未初期化のままになる。
		if (setImGuiContext_ && imguiContext)
		{
			setImGuiContext_(imguiContext);
		}

		onGameLoad_(world);

		RestoreCapturedComponents(world);

		SC_LOG_NOTICE("Plugin: {} をロードしました。", sourcePath_.filename().string());

		return true;
	}

	/**
	* [EN]
	* Destroys and captures every component this module owns, calls
	* SC_OnGameUnload, unregisters the module's reflection / payload
	* entries, frees the module, and deletes its shadow copy.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このモジュールが所有する全コンポーネントを破棄・取得し、
	* SC_OnGameUnload を呼び、モジュールのリフレクション/ペイロード
	* エントリを登録解除し、モジュールを解放してシャドウコピーを削除する。
	*/
	void PluginModule::Unload(World& world)
	{
		if (!handle_)
		{
			return;
		}

		/// [EN] Runs first: capturing field values goes through ReflectionRegistry, so the entries must still be present and callable at this point.
		/// [JP] 最初に実行する: フィールド値の取得は ReflectionRegistry を経由するため、この時点ではエントリがまだ存在し呼び出し可能である必要がある。
		DestroyCapturedComponents(world);

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

	/**
	* [EN]
	* Unload followed by Load (component captures carry across).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Unload の後に Load を行う（コンポーネントの取得内容は引き継がれる）。
	*/
	Bool PluginModule::Reload(World& world, ImGuiContext* imguiContext)
	{
		Unload(world);
		return Load(world, imguiContext);
	}

	/**
	* [EN]
	* Returns whether the source DLL's last-write time differs from the one
	* seen at the last Load - i.e. it was rebuilt.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 元 DLL の最終更新時刻が直近の Load 時点と異なるか - つまりリビルド
	* されたか - を返す。
	*/
	Bool PluginModule::SourceChanged()const
	{
		Uint64 writeTime = GetLastWriteTime(sourcePath_);
		return writeTime != 0 && writeTime != lastWriteTime_;
	}

	/**
	* [EN]
	* The real DLL path on disk this module is bound to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このモジュールが紐づく、ディスク上の実 DLL パス。
	*/
	const std::filesystem::path& PluginModule::SourcePath()const
	{
		return sourcePath_;
	}

	/**
	* [EN]
	* Whether the module is currently loaded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* モジュールが現在ロードされているか。
	*/
	Bool PluginModule::IsLoaded()const
	{
		return handle_ != nullptr;
	}

	/**
	* [EN]
	* Best-effort deletion of leftover shadow DLLs / hot-reload PDBs for
	* this module's stem in its directory. The currently loaded shadow copy
	* is skipped.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このモジュールの stem に対応する、ディレクトリ内に残ったシャドウ
	* DLL / ホットリロード用 PDB を可能な範囲で削除する。現在ロード中の
	* シャドウコピーは対象外。
	*/
	void PluginModule::CleanupStaleArtifacts()const
	{
		std::error_code errorCode;
		std::filesystem::path directory = sourcePath_.parent_path();
		std::wstring prefix = sourcePath_.stem().wstring() + L"_";

		std::filesystem::directory_iterator iterator(directory, errorCode);
		if (errorCode)
		{
			return;
		}

		for (const auto& entry : iterator)
		{
			std::filesystem::path path = entry.path();
			std::wstring name = path.filename().wstring();

			if (!name.starts_with(prefix))
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

	/**
	* [EN]
	* Returns the source DLL's current last-write time (0 if it cannot be
	* read).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 元 DLL の現在の最終更新時刻を返す（読み取れない場合は 0）。
	*/
	Uint64 PluginModule::GetLastWriteTime(const std::filesystem::path& path)
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

	/**
	* [EN]
	* Erases every reflection / payload entry this module registered.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このモジュールが登録したリフレクション/ペイロードのエントリを全て
	* 削除する。
	*/
	void PluginModule::UnregisterModuleReflection()
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

	/**
	* [EN]
	* For every live component whose code lives in handle_: captures its
	* reflected fields into capturedComponents_, removes it from its Actor,
	* then destroys that type's now-empty sparse-set storage container.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* handle_ 内にコードがある全ての生きたコンポーネントについて: その
	* リフレクションフィールドを capturedComponents_ へ取得し、その Actor
	* から削除した上で、空になったその型のスパースセットストレージ
	* コンテナを破棄する。
	*/
	void PluginModule::DestroyCapturedComponents(World& world)
	{
		/// [EN] Whether id's ComponentMetadata function pointers live inside this module (handle_).
		/// [JP] id の ComponentMetadata の関数ポインタが、このモジュール（handle_）の中にあるかどうか。
		auto isOwnedByLoadedModule = [this](ComponentID id) -> Bool
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
		};

		capturedComponents_.clear();

		for (Actor actor : world.GetActors())
		{
			DynamicArray<ComponentID> ownedIDs;
			std::ranges::copy_if(actor.ComponentBaseIDList(), std::back_inserter(ownedIDs), isOwnedByLoadedModule);

			for (ComponentID id : ownedIDs)
			{
				void* data = world.GetComponent(actor.GetEntity(), id);
				if (data)
				{
					String name = ComponentRegistry::GetName(id);
					capturedComponents_.emplace_back(actor, CaptureComponent(name, data));
				}

				actor.RemoveComponent(id);
			}
		}

		/// [EN] Collected during a read-only pass over GetRegistry() and unregistered afterwards - ComponentRegistry::Unregister() erases from the very map GetRegistry() returns a reference to, so mutating it mid-iteration would be unsafe.
		/// [JP] GetRegistry() を読み取り専用で走査する間に集めておき、走査後にまとめて登録解除する - ComponentRegistry::Unregister() は GetRegistry() が参照を返しているまさにそのマップから削除するため、走査中に変更するのは安全ではない。
		DynamicArray<ComponentID> ownedComponentIDs;
		for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
		{
			if (!isOwnedByLoadedModule(id))
			{
				continue;
			}

			if (metadata.storage_ == ComponentStorage::SparseSet)
			{
				world.UnregisterSparseSetStorage(id);
			}

			ownedComponentIDs.push_back(id);
		}

		/// [EN] Must happen before FreeLibrary() unloads the module - otherwise a stale std::type_index left behind in ComponentRegistry::TypeIndex() would point at a type_info living in freed memory, crashing the next Register<T>() call that happens to probe the same slot.
		/// [JP] FreeLibrary() がモジュールをアンロードするより前に行う必要がある - そうしなければ ComponentRegistry::TypeIndex() に残った古い std::type_index が解放済みメモリ上の type_info を指したままになり、以後同じスロットを探査する Register<T>() 呼び出しでクラッシュする。
		for (ComponentID id : ownedComponentIDs)
		{
			ComponentRegistry::Unregister(id);
		}
	}

	/**
	* [EN]
	* Re-adds every component in capturedComponents_ to its original Actor
	* and restores its captured field values, then clears
	* capturedComponents_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* capturedComponents_ の各コンポーネントを元の Actor へ再追加し、
	* 取得済みのフィールド値を復元した上で、capturedComponents_ を
	* クリアする。
	*/
	void PluginModule::RestoreCapturedComponents(World& world)
	{
		for (auto& [actor, component] : capturedComponents_)
		{
			ComponentID id = ComponentRegistry::GetComponentID(component.componentName_);
			if (!id)
			{
				continue;
			}

			actor.AddComponent(id);

			void* data = world.GetComponent(actor.GetEntity(), id);
			if (data)
			{
				ApplyComponent(component, data);
			}
		}

		capturedComponents_.clear();
	}
}
