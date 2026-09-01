#include <FoundationEngine/Plugin/PluginHost.h>
#include <FoundationEngine/Log/Notice.h>
#include <Windows.h>
#include <algorithm>

namespace SeedCore
{
	/**
	* [EN]
	* Binds the plugin directory to scan (typically the directory the
	* executable lives in) and the ImGui context to forward to each
	* plugin. Must be called once before Load.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 走査するプラグインディレクトリ（通常は実行ファイルが置かれている
	* ディレクトリ）と、各プラグインへ渡す ImGui コンテキストを束縛する。
	* Load の前に一度呼び出す必要がある。
	*/
	void PluginHost::Initialize(const std::filesystem::path& pluginDirectory, ImGuiContext* imguiContext)
	{
		pluginDirectory_ = pluginDirectory;
		imguiContext_ = imguiContext;
	}

	/**
	* [EN]
	* Loads every gameplay plugin found in the plugin directory.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プラグインディレクトリ内で見つかった全ゲームプレイプラグインを
	* ロードする。
	*/
	void PluginHost::Load(World& world)
	{
		if (!std::filesystem::exists(pluginDirectory_))
		{
			SC_LOG_NOTICE("PluginHost: プラグインディレクトリがありません ({})。プラグインは読み込まれません。", pluginDirectory_.string());
			return;
		}

		ScanAndLoad(world);
	}

	/**
	* [EN]
	* Unloads and releases every loaded plugin.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ロード済みの全プラグインをアンロードして解放する。
	*/
	void PluginHost::Unload(World& world)
	{
		for (Entry& entry : entries_)
		{
			entry.module_->Unload(world);
		}
		entries_.clear();
	}

	/**
	* [EN]
	* Once per frame: reloads any plugin whose source DLL has been rebuilt
	* (after its write time has stopped changing), and picks up DLLs newly
	* added to / removed from the plugin directory.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 毎フレーム: 元 DLL がリビルドされたプラグインを（更新時刻の変化が
	* 止まった後で）リロードし、プラグインディレクトリに新たに追加/削除
	* された DLL を反映する。
	*/
	void PluginHost::Tick(World& world)
	{
		Uint64 nowTick = GetTickCount64();

		for (Entry& entry : entries_)
		{
			Uint64 writeTime = PluginModule::GetLastWriteTime(entry.module_->SourcePath());
			if (writeTime == 0 || !entry.module_->SourceChanged())
			{
				continue;
			}

			if (writeTime == entry.pendingWriteTime_)
			{
				if (nowTick - entry.pendingStableSinceTick_ >= stableWindowMilliseconds_)
				{
					entry.module_->Reload(world, imguiContext_);
					entry.pendingWriteTime_ = 0;
					entry.pendingStableSinceTick_ = 0;
				}
			}
			else
			{
				entry.pendingWriteTime_ = writeTime;
				entry.pendingStableSinceTick_ = nowTick;
			}
		}

		if (nowTick - lastRescanTick_ < rescanIntervalMilliseconds_)
		{
			return;
		}
		lastRescanTick_ = nowTick;

		/// [EN] Drop plugins whose source DLL has disappeared.
		/// [JP] 元 DLL が消えたプラグインを外す。
		SeedCore::erase_if(entries_, [&world](Entry& entry)
		{
			if (std::filesystem::exists(entry.module_->SourcePath()))
			{
				return false;
			}
			entry.module_->Unload(world);
			return true;
		});

		/// [EN] Load plugins newly dropped into the directory.
		/// [JP] ディレクトリに新たに置かれたプラグインをロードする。
		ScanAndLoad(world);
	}

	/**
	* [EN]
	* Returns the loaded plugin whose source DLL file name stem equals
	* stem (e.g. "UserProject"), or nullptr if none is loaded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 元 DLL のファイル名 stem が stem（例: "UserProject"）に一致する
	* ロード済みプラグインを返す。無ければ nullptr。
	*/
	PluginModule* PluginHost::Find(const std::filesystem::path& stem)const
	{
		auto found = std::ranges::find(entries_, stem, [](const Entry& entry){ return entry.module_->SourcePath().stem(); });
		return found != entries_.end() ? found->module_.get() : nullptr;
	}

	/**
	* [EN]
	* Reloads a single plugin now, bypassing the timestamp debounce, and
	* clears that plugin's pending debounce state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タイムスタンプのデバウンスを飛ばして単一プラグインを今すぐリロード
	* し、そのプラグインのデバウンス待ち状態をクリアする。
	*/
	void PluginHost::ReloadModule(World& world, PluginModule& module)
	{
		module.Reload(world, imguiContext_);

		auto found = std::ranges::find(entries_, &module, [](const Entry& entry){ return entry.module_.get(); });
		if (found != entries_.end())
		{
			found->pendingWriteTime_ = 0;
			found->pendingStableSinceTick_ = 0;
		}
	}

	/**
	* [EN]
	* Scans the plugin directory (non-recursively) and loads every
	* gameplay plugin (*.dll exporting SC_OnGameLoad / SC_OnGameUnload)
	* that is not already loaded. Shadow copies and engine / third-party
	* DLLs are skipped; a DLL that fails the plugin check is remembered in
	* rejectedFileNames_ so a later rescan does not probe it again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プラグインディレクトリを（非再帰で）走査し、未ロードの全ゲーム
	* プレイプラグイン（SC_OnGameLoad / SC_OnGameUnload をエクスポート
	* する *.dll）をロードする。シャドウコピーとエンジン/サードパーティ
	* DLL はスキップし、プラグインチェックに落ちた DLL は
	* rejectedFileNames_ に記憶して以後の再スキャンで再プローブしない。
	*/
	void PluginHost::ScanAndLoad(World& world)
	{
		/// [EN] A shadow copy this host produced, named "<stem>_<digits>.dll".
		/// [JP] この host が生成したシャドウコピー。"<stem>_<数字>.dll" という名前。
		auto isShadowCopy = [](const std::filesystem::path& path) -> Bool
		{
			std::wstring stem = path.stem().wstring();
			Size underscore = stem.rfind(L'_');
			if (underscore == std::wstring::npos || underscore + 1 >= stem.size())
			{
				return false;
			}
			std::wstring_view digits = std::wstring_view(stem).substr(underscore + 1);
			return std::ranges::all_of(digits, [](wchar_t character){ return character >= L'0' && character <= L'9'; });
		};

		/// [EN] A gameplay plugin, not engine / third-party code sharing the directory. Already-loaded modules (e.g. SeedCore.dll) are rejected at once; the rest are probed with DONT_RESOLVE_DLL_REFERENCES so no DllMain runs and no dependencies load.
		/// [JP] ディレクトリを共有しているだけのエンジン/サードパーティコードではなく、ゲームプレイプラグインかどうか。既にロード済みのモジュール（例: SeedCore.dll）は即除外し、残りは DONT_RESOLVE_DLL_REFERENCES でプローブするため DllMain は走らず依存もロードされない。
		auto isPluginCandidate = [](const std::filesystem::path& path) -> Bool
		{
			if (GetModuleHandleW(path.filename().c_str()))
			{
				return false;
			}
			HMODULE probe = LoadLibraryExW(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
			if (!probe)
			{
				return false;
			}
			Bool hasEntryPoints = GetProcAddress(probe, "SC_OnGameLoad") != nullptr && GetProcAddress(probe, "SC_OnGameUnload") != nullptr;
			FreeLibrary(probe);
			return hasEntryPoints;
		};

		std::error_code errorCode;
		std::filesystem::directory_iterator iterator(pluginDirectory_, errorCode);
		if (errorCode)
		{
			return;
		}

		for (const auto& entry : iterator)
		{
			std::filesystem::path path = entry.path();
			if (path.extension() != L".dll" || isShadowCopy(path))
			{
				continue;
			}

			if (Find(path.stem()) != nullptr)
			{
				continue;
			}

			String fileName = String(path.filename().string());
			if (std::ranges::contains(rejectedFileNames_, fileName))
			{
				continue;
			}

			if (!isPluginCandidate(path))
			{
				rejectedFileNames_.push_back(fileName);
				continue;
			}

			ResourcePtr<PluginModule> module = MakePtr<PluginModule>(path);
			if (!module->Load(world, imguiContext_))
			{
				continue;
			}

			Entry loaded;
			loaded.module_ = std::move(module);
			entries_.push_back(std::move(loaded));
		}
	}
}
