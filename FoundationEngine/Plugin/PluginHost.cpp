#include <FoundationEngine/Plugin/PluginHost.h>
#include <FoundationEngine/Log/Notice.h>
#include <Windows.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] How long a plugin DLL's write time must stay unchanged before a reload is triggered (MSBuild can touch a file's write time more than once while writing it).
		/// [JP] プラグイン DLL の更新時刻が、リロードをトリガーするまでに変化なしで安定していなければならない時間（MSBuild は書き込み中に最終更新時刻を複数回更新することがあるため）。
		constexpr Uint64 StableWindowMilliseconds = 500;

		/// [EN] How often the plugin directory is rescanned for added/removed DLLs, in milliseconds.
		/// [JP] プラグインディレクトリの DLL 追加/削除を再スキャンする間隔（ミリ秒）。
		constexpr Uint64 RescanIntervalMilliseconds = 500;
	}

	Bool PluginHost::IsShadowCopy(const std::filesystem::path& path)
	{
		std::wstring stem = path.stem().wstring();
		Size underscore = stem.rfind(L'_');
		if (underscore == std::wstring::npos || underscore + 1 >= stem.size())
		{
			return false;
		}

		for (Size index = underscore + 1; index < stem.size(); ++index)
		{
			if (stem[index] < L'0' || stem[index] > L'9')
			{
				return false;
			}
		}
		return true;
	}

	void PluginHost::Initialize(const std::filesystem::path& pluginDirectory, ImGuiContext* imguiContext)
	{
		pluginDirectory_ = pluginDirectory;
		imguiContext_ = imguiContext;

		std::error_code errorCode;
		std::filesystem::create_directories(pluginDirectory_, errorCode);
	}

	/**
	* [EN]
	* Discovers every non-shadow *.dll in the plugin directory and loads
	* each as a PluginModule.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プラグインディレクトリ内のシャドウコピーでない全 *.dll を発見し、
	* それぞれ PluginModule としてロードする。
	*/
	void PluginHost::LoadAll(World& world)
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
	* Recursively scans the plugin directory and loads every non-shadow
	* *.dll not already loaded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プラグインディレクトリを再帰的に走査し、シャドウコピーでなく未ロード
	* の全 *.dll をロードする。
	*/
	void PluginHost::ScanAndLoad(World& world)
	{
		std::error_code errorCode;
		auto iterator = std::filesystem::recursive_directory_iterator(pluginDirectory_, std::filesystem::directory_options::skip_permission_denied, errorCode);
		if (errorCode)
		{
			return;
		}

		for (const auto& entry : iterator)
		{
			std::filesystem::path path = entry.path();
			if (path.extension() != L".dll" || IsShadowCopy(path))
			{
				continue;
			}

			if (FindByStem(path.stem()) != nullptr)
			{
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

	/**
	* [EN]
	* Unloads and releases every loaded plugin.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ロード済みの全プラグインをアンロードして解放する。
	*/
	void PluginHost::UnloadAll(World& world)
	{
		for (Entry& entry : entries_)
		{
			entry.module_->Unload(world);
		}
		entries_.clear();
	}

	PluginModule* PluginHost::FindByStem(const std::filesystem::path& stem)const
	{
		for (const Entry& entry : entries_)
		{
			if (entry.module_->SourcePath().stem() == stem)
			{
				return entry.module_.get();
			}
		}
		return nullptr;
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

		for (Entry& entry : entries_)
		{
			if (entry.module_.get() == &module)
			{
				entry.pendingWriteTime_ = 0;
				entry.pendingStableSinceTick_ = 0;
				break;
			}
		}
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
				if (nowTick - entry.pendingStableSinceTick_ >= StableWindowMilliseconds)
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

		if (nowTick - lastRescanTick_ < RescanIntervalMilliseconds)
		{
			return;
		}
		lastRescanTick_ = nowTick;

		/// [EN] Drop plugins whose source DLL has disappeared.
		/// [JP] 元 DLL が消えたプラグインを外す。
		for (Size index = entries_.size(); index > 0; --index)
		{
			Entry& entry = entries_[index - 1];
			if (!std::filesystem::exists(entry.module_->SourcePath()))
			{
				entry.module_->Unload(world);
				entries_.erase(entries_.begin() + (index - 1));
			}
		}

		/// [EN] Load plugins newly dropped into the directory.
		/// [JP] ディレクトリに新たに置かれたプラグインをロードする。
		ScanAndLoad(world);
	}
}
