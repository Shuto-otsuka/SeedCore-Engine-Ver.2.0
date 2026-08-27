#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/NonTransferable.h>
#include <FoundationEngine/Utility/ResourcePtr.h>
#include <FoundationEngine/Plugin/PluginModule.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Owns every loaded gameplay plugin (see PluginModule) discovered
	* anywhere under a single plugin directory (each plugin may sit in its
	* own subfolder next to its sidecar DLLs), and drives their lifecycle:
	* load all on
	* startup, unload all on shutdown, and once per frame reload any whose
	* DLL was rebuilt (debounced on a stable-timestamp window, since
	* MSBuild can touch a file's write time more than once while writing
	* it). Editor.exe and the game runtime each own one PluginHost.
	*
	* Plugins are loaded from shadow copies with
	* LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR so a plugin can carry its own
	* sidecar DLLs in the plugin directory; the application directory stays
	* in the search set for SeedCore.dll.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一のプラグインディレクトリで発見された、ロード済みの全ゲームプレイ
	* プラグイン（PluginModule 参照）を所有し、そのライフサイクルを統括
	* する: 起動時に全ロード、終了時に全アンロード、毎フレーム DLL が
	* リビルドされたものをリロードする（MSBuild は書き込み中に最終更新
	* 時刻を複数回更新することがあるため、タイムスタンプが安定するまで
	* 待ってから動作する）。Editor.exe とゲームランタイムがそれぞれ
	* PluginHost を1個所有する。
	*
	* プラグインは LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 付きでシャドウコピー
	* からロードされるため、プラグインは自身の付随 DLL をプラグイン
	* ディレクトリに持てる; SeedCore.dll のためにアプリケーション
	* ディレクトリは検索対象に残る。
	*/
	class SEEDCORE_API PluginHost :public NonTransferable
	{
	public:
		PluginHost() = default;
		~PluginHost() = default;

		/**
		* [EN]
		* Binds the plugin directory to scan and the ImGui context to
		* forward to each plugin. Must be called once before LoadAll.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 走査するプラグインディレクトリと、各プラグインへ渡す ImGui
		* コンテキストを束縛する。LoadAll の前に一度呼び出す必要がある。
		*/
		void Initialize(const std::filesystem::path& pluginDirectory, ImGuiContext* imguiContext);

		/**
		* [EN]
		* Discovers every non-shadow *.dll anywhere under the plugin
		* directory (recursively) and loads each as a PluginModule. A DLL
		* missing SC_OnGameLoad / SC_OnGameUnload is skipped (not retained).
		* No-op if the directory does not exist.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プラグインディレクトリ配下（再帰的）のシャドウコピーでない全
		* *.dll を発見し、それぞれ PluginModule としてロードする。
		* SC_OnGameLoad / SC_OnGameUnload を持たない DLL はスキップされる
		* （保持しない）。ディレクトリが存在しなければ何もしない。
		*/
		void LoadAll(World& world);

		/**
		* [EN]
		* Unloads and releases every loaded plugin.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ロード済みの全プラグインをアンロードして解放する。
		*/
		void UnloadAll(World& world);

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
		[[nodiscard]] PluginModule* FindByStem(const std::filesystem::path& stem)const;

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
		* A loaded plugin plus the per-module debounce state for its
		* DLL-rebuild watcher.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ロード済みプラグインと、その DLL リビルド監視用のモジュール
		* ごとのデバウンス状態。
		*/
		struct Entry
		{
			/// [EN] The owned plugin module.
			/// [JP] 所有しているプラグインモジュール。
			ResourcePtr<PluginModule> module_;

			/// [EN] Source DLL write time seen on the previous Tick, awaiting confirmation that it has stopped changing.
			/// [JP] 前回 Tick 時に観測した元 DLL の更新時刻。変化が止まったかどうかの確認待ち状態で使う。
			Uint64 pendingWriteTime_ = 0;

			/// [EN] GetTickCount64() timestamp when pendingWriteTime_ was first observed.
			/// [JP] pendingWriteTime_ を最初に観測した時点の GetTickCount64()。
			Uint64 pendingStableSinceTick_ = 0;
		};

		/// [EN] Whether path names a shadow copy this host produced (stem ends in "_<digits>").
		/// [JP] path が、この host が生成したシャドウコピー（stem が "_<数字>" で終わる）かどうか。
		[[nodiscard]] static Bool IsShadowCopy(const std::filesystem::path& path);

		/// [EN] Recursively scans the plugin directory and loads every non-shadow *.dll (in the directory itself or any subdirectory, so a plugin can live in its own folder alongside its sidecar DLLs) that is not already loaded and exports the plugin entry points.
		/// [JP] プラグインディレクトリを再帰的に走査し、シャドウコピーでなく、未ロードで、プラグインのエントリポイントをエクスポートする全 *.dll（ディレクトリ直下でも任意のサブディレクトリ内でも — プラグインは付随 DLL と一緒に自分専用のフォルダに置ける）をロードする。
		void ScanAndLoad(World& world);

		/// [EN] The directory scanned for plugin DLLs.
		/// [JP] プラグイン DLL を走査するディレクトリ。
		std::filesystem::path pluginDirectory_;

		/// [EN] ImGui context forwarded to every plugin on load.
		/// [JP] ロード時に各プラグインへ渡す ImGui コンテキスト。
		ImGuiContext* imguiContext_ = nullptr;

		/// [EN] Every loaded plugin.
		/// [JP] ロード済みの全プラグイン。
		DynamicArray<Entry> entries_;

		/// [EN] GetTickCount64() timestamp of the last plugin-directory rescan, used to throttle rescans to a fixed interval.
		/// [JP] 直近にプラグインディレクトリを再スキャンした時点の GetTickCount64()。再スキャン頻度を一定間隔に抑えるために使う。
		Uint64 lastRescanTick_ = 0;
	};
}
