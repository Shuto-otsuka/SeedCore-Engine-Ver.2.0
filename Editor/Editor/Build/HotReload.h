#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/NonTransferable.h>

namespace SeedCore
{
	class World;
	class PluginHost;
	class PluginModule;

	/**
	* [EN]
	* Editor-only development loop for the UserProject plugin: watches its
	* source tree, auto-builds UserProject.dll via MSBuild when a .h/.cpp
	* changes, and — once the build finishes — asks the PluginHost to
	* reload that one plugin. Both the source-change and DLL-rebuild
	* detection debounce on a stable timestamp window before acting, since
	* MSBuild can touch a file's write time more than once while writing
	* it.
	*
	* The module lifecycle itself (shadow-copy load, entry-point
	* resolution, reflection-registry bookkeeping, component
	* capture/restore across a reload) lives in PluginHost / PluginModule
	* and is shared with the game runtime; this class only adds the
	* editor-side "rebuild from source" half.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UserProject プラグイン用の、エディタ限定の開発ループ: そのソース
	* ツリーを監視し、.h/.cpp が変わると MSBuild で UserProject.dll を自動
	* ビルドし、ビルド完了時に PluginHost へそのプラグイン1個のリロードを
	* 依頼する。ソース変更検知・DLL 再ビルド検知のどちらも、タイムスタンプ
	* が一定時間安定するまで待ってから動作する（MSBuild は書き込み中に
	* 最終更新時刻を複数回更新することがあるため）。
	*
	* モジュールのライフサイクル自体（シャドウコピーからのロード、
	* エントリポイント解決、リフレクションレジストリの管理、リロードを
	* またいだコンポーネントの取得/復元）は PluginHost / PluginModule に
	* あり、ゲームランタイムと共有される; このクラスはエディタ側の
	* 「ソースからの再ビルド」の半分だけを担う。
	*/
	class HotReload :public NonTransferable
	{
	public:
		HotReload() = default;
		~HotReload();

		/**
		* [EN]
		* Binds the PluginHost this class drives and caches its
		* already-loaded UserProject plugin (nullptr if UserProject.dll was
		* not among the loaded plugins). Call once after
		* PluginHost::LoadAll.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このクラスが駆動する PluginHost を束縛し、そこでロード済みの
		* UserProject プラグインをキャッシュする（UserProject.dll が
		* ロード済みプラグインに無ければ nullptr）。PluginHost::LoadAll の
		* 後に一度呼ぶこと。
		*/
		void Initialize(PluginHost& pluginHost);

		/**
		* [EN]
		* Polls the running build process and the UserProject source tree
		* once per frame; triggers an auto-build when the source changed,
		* and reloads the UserProject plugin once a build this class
		* started has finished.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 実行中のビルドプロセスと UserProject のソースツリーを毎フレーム
		* 確認する; ソースが変わっていれば自動ビルドを開始し、このクラスが
		* 起動したビルドが完了したら UserProject プラグインをリロードする。
		*/
		void Tick(World& world);

	private:
		/// [EN] The plugin host that owns the loaded plugins; this class only drives UserProject's rebuild-and-reload.
		/// [JP] ロード済みプラグインを所有するプラグインホスト; このクラスは UserProject の再ビルド＆リロードだけを駆動する。
		PluginHost* pluginHost_ = nullptr;

		/// [EN] The loaded UserProject plugin, cached from pluginHost_ (nullptr if not loaded).
		/// [JP] pluginHost_ からキャッシュした、ロード済みの UserProject プラグイン（未ロードなら nullptr）。
		PluginModule* userProjectPlugin_ = nullptr;

		/// [EN] Path to MSBuild.exe, resolved once via vswhere on first use. Empty if it could not be found.
		/// [JP] MSBuild.exe のパス。初回使用時に vswhere 経由で一度だけ解決する。見つからなければ空。
		std::filesystem::path msbuildPath_;

		/// [EN] True once msbuildPath_ resolution (success or failure) has been attempted.
		/// [JP] msbuildPath_ の解決を（成否に関わらず）一度試みたら true になる。
		Bool msbuildResolved_ = false;

		/// [EN] Handle of the currently running background build process (nullptr when idle).
		/// [JP] 現在実行中のバックグラウンドビルドプロセスのハンドル（アイドル時は nullptr）。
		HANDLE buildProcessHandle_ = nullptr;

		/// [EN] Read end of the pipe the build process's stdout/stderr are redirected to (nullptr when idle). Drained every Tick() so the child never blocks on a full pipe buffer.
		/// [JP] ビルドプロセスの標準出力/標準エラーのリダイレクト先パイプの読み取り側（アイドル時は nullptr）。子プロセスがパイプバッファ満杯でブロックしないよう、毎 Tick() で読み出す。
		HANDLE buildOutputReadPipe_ = nullptr;

		/// [EN] Accumulated stdout/stderr text from the currently (or most recently) running build, for logging on failure.
		/// [JP] 現在（または直近）のビルドの標準出力/標準エラーの蓄積テキスト。失敗時のログ出力に使う。
		std::string buildOutput_;

		/// [EN] Set when a build this class started succeeds, telling the next Tick() to reload the UserProject plugin.
		/// [JP] このクラスが起動したビルドが成功したときに立つ。次の Tick() で UserProject プラグインをリロードするよう伝える。
		Bool reloadRequested_ = false;

		/// [EN] Latest last-write time observed anywhere under UserProject's source tree, at the moment a build was last triggered.
		/// [JP] 直近にビルドをトリガーした時点で観測していた、UserProject ソースツリー内の最新の最終更新時刻。
		Uint64 lastTriggeredSourceWriteTime_ = 0;

		/// [EN] Source-tree write time seen on the previous scan, awaiting confirmation that it has stopped changing.
		/// [JP] 前回スキャン時に観測したソースツリーの更新時刻。変化が止まったかどうかの確認待ち状態で使う。
		Uint64 pendingSourceWriteTime_ = 0;

		/// [EN] GetTickCount64() timestamp when pendingSourceWriteTime_ was first observed.
		/// [JP] pendingSourceWriteTime_ を最初に観測した時点の GetTickCount64()。
		Uint64 pendingSourceStableSinceTick_ = 0;

		/// [EN] GetTickCount64() timestamp of the last source-tree scan, used to throttle scans to a fixed interval.
		/// [JP] 直近にソースツリーをスキャンした時点の GetTickCount64()。スキャン頻度を一定間隔に抑えるために使う。
		Uint64 lastSourceScanTick_ = 0;

		/// [EN] Reads whatever's currently buffered in buildOutputReadPipe_ (non-blocking) into buildOutput_.
		/// [JP] buildOutputReadPipe_ に現在溜まっている分を（ノンブロッキングで）buildOutput_ へ読み出す。
		void DrainBuildOutput();

		/// [EN] Checks whether the background build process has finished; logs the result and, on success, sets reloadRequested_.
		/// [JP] バックグラウンドビルドプロセスが終了したか確認する; 結果をログに出し、成功していれば reloadRequested_ を立てる。
		void PollBuildProcess();

		/// [EN] Launches an async MSBuild.exe build of UserProject.vcxproj. No-op if MSBuild couldn't be resolved.
		/// [JP] UserProject.vcxproj の非同期ビルドを MSBuild.exe で起動する。MSBuild が見つからなければ何もしない。
		void TriggerBuild();

		[[nodiscard]] static std::filesystem::path UserProjectSourceDirectory();

		[[nodiscard]] static std::filesystem::path UserProjectVcxprojPath();

		[[nodiscard]] static Uint64 GetLastWriteTime(const std::filesystem::path& path);

		/// [EN] Recursively finds the newest last-write time among UserProject's .h/.cpp files. Returns 0 if the directory can't be scanned.
		/// [JP] UserProject 配下の .h/.cpp ファイルのうち、最も新しい最終更新時刻を再帰的に探す。ディレクトリをスキャンできない場合は 0 を返す。
		[[nodiscard]] static Uint64 ScanSourceLastWriteTime();

		/// [EN] Resolves MSBuild.exe's path via vswhere.exe (a synchronous, one-time call).
		/// [JP] vswhere.exe 経由で MSBuild.exe のパスを解決する（同期・一度きりの呼び出し）。
		[[nodiscard]] static std::filesystem::path FindMSBuild();
	};
}
