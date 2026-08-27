#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Plugin/PluginModule.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Owns every loaded gameplay plugin (see PluginModule) found in the
	* plugin directory - the directory the executable itself lives in,
	* shared with SeedCore.dll and the third-party DLLs - and drives their
	* lifecycle: load all on startup, unload all on shutdown, and once per
	* frame reload any whose DLL was rebuilt (debounced on a
	* stable-timestamp window, since MSBuild can touch a file's write time
	* more than once while writing it). Editor.exe and the game runtime
	* each own one PluginHost.
	*
	* A DLL is treated as a plugin only if it exports SC_OnGameLoad /
	* SC_OnGameUnload (see IsPluginCandidate); engine and third-party DLLs
	* in the same directory are filtered out without running their code.
	* Plugins are loaded from shadow copies with
	* LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR so a plugin can carry its own
	* sidecar DLLs; the application directory stays in the search set for
	* SeedCore.dll.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プラグインディレクトリ - 実行ファイル自身が置かれ、SeedCore.dll や
	* サードパーティ DLL と共有されるディレクトリ - で見つかった、ロード済み
	* の全ゲームプレイプラグイン（PluginModule 参照）を所有し、そのライフ
	* サイクルを統括する: 起動時に全ロード、終了時に全アンロード、毎フレーム
	* DLL がリビルドされたものをリロードする（MSBuild は書き込み中に最終
	* 更新時刻を複数回更新することがあるため、タイムスタンプが安定するまで
	* 待ってから動作する）。Editor.exe とゲームランタイムがそれぞれ
	* PluginHost を1個所有する。
	*
	* DLL は SC_OnGameLoad / SC_OnGameUnload をエクスポートしている場合
	* のみプラグインとして扱う（IsPluginCandidate 参照）; 同じディレクトリ
	* にあるエンジン/サードパーティ DLL は、そのコードを実行せずに除外する。
	* プラグインは LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 付きでシャドウコピー
	* からロードされるため、プラグインは自身の付随 DLL を持てる;
	* SeedCore.dll のためにアプリケーションディレクトリは検索対象に残る。
	*/
	class SEEDCORE_API PluginHost :public NonTransferable
	{
	public:
		PluginHost() = default;
		~PluginHost() = default;

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
		void Initialize(const std::filesystem::path& pluginDirectory, ImGuiContext* imguiContext);

		/**
		* [EN]
		* Loads every gameplay plugin (a *.dll exporting SC_OnGameLoad /
		* SC_OnGameUnload) directly in the plugin directory. Engine and
		* third-party DLLs alongside it are filtered out. No-op if the
		* directory does not exist.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プラグインディレクトリ直下の全ゲームプレイプラグイン
		* （SC_OnGameLoad / SC_OnGameUnload をエクスポートする *.dll）を
		* ロードする。隣にあるエンジン/サードパーティ DLL は除外される。
		* ディレクトリが存在しなければ何もしない。
		*/
		void Load(World& world);

		/**
		* [EN]
		* Unloads and releases every loaded plugin.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ロード済みの全プラグインをアンロードして解放する。
		*/
		void Unload(World& world);

		/**
		* [EN]
		* Once per frame: reloads any plugin whose source DLL has been
		* rebuilt (after its write time has stopped changing), and picks up
		* DLLs newly added to / removed from the plugin directory.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 毎フレーム: 元 DLL がリビルドされたプラグインを（更新時刻の変化が
		* 止まった後で）リロードし、プラグインディレクトリに新たに追加/削除
		* された DLL を反映する。
		*/
		void Tick(World& world);

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
		[[nodiscard]] PluginModule* Find(const std::filesystem::path& stem)const;

		/**
		* [EN]
		* Reloads a single plugin now, bypassing the timestamp debounce.
		* Used by the editor when it started the rebuild itself and knows
		* the DLL is complete.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タイムスタンプのデバウンスを飛ばして、単一プラグインを今すぐ
		* リロードする。エディタがリビルドを自分で起動し、DLL が完成して
		* いると分かっている場合に使う。
		*/
		void ReloadModule(World& world, PluginModule& module);

	private:
		/**
		* [EN]
		* Scans the plugin directory (non-recursively) and loads every
		* gameplay plugin (*.dll exporting SC_OnGameLoad / SC_OnGameUnload)
		* that is not already loaded. Shadow copies and engine / third-party
		* DLLs are skipped; a DLL that fails the plugin check is remembered
		* in rejectedFileNames_ so a later rescan does not probe it again.
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
		void ScanAndLoad(World& world);

	private:
		/// [EN] How long a plugin DLL's write time must stay unchanged before a reload fires - MSBuild can touch a file's write time more than once while writing it.
		/// [JP] リロードが発火するまでに、プラグイン DLL の更新時刻が変化なしで安定していなければならない時間 - MSBuild は書き込み中に最終更新時刻を複数回更新することがある。
		static constexpr Uint64 stableWindowMilliseconds_ = 500;

		/// [EN] How often the plugin directory is rescanned for added / removed DLLs.
		/// [JP] プラグインディレクトリの DLL 追加/削除を再スキャンする間隔。
		static constexpr Uint64 rescanIntervalMilliseconds_ = 500;

		/**
		* [EN]
		* A loaded plugin plus the per-module debounce state for its
		* DLL-rebuild watcher.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ロード済みプラグインと、その DLL リビルド監視用のモジュールごとの
		* デバウンス状態。
		*/
		struct Entry
		{
			/// [EN] The owned plugin module.
			/// [JP] 所有しているプラグインモジュール。
			ResourcePtr<PluginModule> module_;

			/// [EN] Source DLL write time seen on the previous Tick, awaiting confirmation that it has stopped changing.
			/// [JP] 前回 Tick 時に観測した元 DLL の更新時刻。変化が止まったかどうかの確認待ちに使う。
			Uint64 pendingWriteTime_ = 0;

			/// [EN] GetTickCount64() timestamp when pendingWriteTime_ was first observed.
			/// [JP] pendingWriteTime_ を最初に観測した時点の GetTickCount64()。
			Uint64 pendingStableSinceTick_ = 0;
		};

		/// [EN] The directory scanned for plugin DLLs (the executable's own directory).
		/// [JP] プラグイン DLL を走査するディレクトリ（実行ファイル自身のディレクトリ）。
		std::filesystem::path pluginDirectory_;

		/// [EN] ImGui context forwarded to every plugin on load.
		/// [JP] ロード時に各プラグインへ渡す ImGui コンテキスト。
		ImGuiContext* imguiContext_ = nullptr;

		/// [EN] Every loaded plugin.
		/// [JP] ロード済みの全プラグイン。
		DynamicArray<Entry> entries_;

		/// [EN] File names of DLLs in the plugin directory that failed the plugin check, so a rescan does not probe them again.
		/// [JP] プラグインディレクトリ内でプラグインチェックに落ちた DLL のファイル名。再スキャンで再プローブしないために保持する。
		DynamicArray<String> rejectedFileNames_;

		/// [EN] GetTickCount64() timestamp of the last plugin-directory rescan, used to throttle rescans to a fixed interval.
		/// [JP] 直近にプラグインディレクトリを再スキャンした時点の GetTickCount64()。再スキャン頻度を一定間隔に抑えるために使う。
		Uint64 lastRescanTick_ = 0;
	};
}
