#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/NonTransferable.h>
#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;
	class Actor;

	/**
	* [EN]
	* Watches UserProject's source tree, auto-builds it via MSBuild when it
	* changes, and loads UserProject.dll as a shadow-copied, hot-reloadable
	* module — forwarding SC_OnGameLoad/SC_OnGameUnload across reload
	* boundaries. Both the source-change and DLL-rebuild detection debounce
	* on a stable timestamp window before acting, since MSBuild can touch a
	* file's write time more than once while writing it.
	*
	* Before unloading, every ComponentBase-derived component whose code
	* lives in the currently loaded module is: its reflected field values
	* captured (ActorSerialization::CaptureComponent), the instance
	* destroyed on its Actor, and — once no instance of that type remains
	* — its sparse-set storage container itself destroyed, so no vtable
	* pointer into the soon-to-be-unloaded DLL survives. After the new
	* module loads and re-registers those types, captured components are
	* re-added to the same Actors and their field values restored, so
	* gameplay data (and even which component types exist) survives a
	* reload much like Unity's domain reload — provided reflected field
	* layout is compatible across the rebuild (adding logic is always
	* safe; renaming/retyping a reflected field loses that field's data).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UserProject のソースツリーを監視し、変更があれば MSBuild で自動ビルドし、
	* UserProject.dll をシャドウコピーしてホットリロード可能なモジュールとして
	* ロードする — リロードをまたいで SC_OnGameLoad/SC_OnGameUnload を呼び出す。
	* ソース変更検知・DLL再ビルド検知のどちらも、タイムスタンプが一定時間
	* 安定するまで待ってから動作する(MSBuildは書き込み中に最終更新時刻を
	* 複数回更新することがあるため)。
	*
	* アンロード前に、現在ロード中のモジュール内にコードがある全ての
	* ComponentBase 派生コンポーネントについて: リフレクションフィールド値を
	* 取得し(ActorSerialization::CaptureComponent)、その Actor 上のインスタンスを
	* 破棄し、その型のインスタンスが無くなった時点でスパースセットストレージ
	* コンテナ自体も破棄する — アンロードされる DLL 内へのポインタが
	* 一切残らないようにする。新しいモジュールがロードされ、それらの型を
	* 再登録した後、取得済みのコンポーネントは同じ Actor へ再度追加され、
	* フィールド値が復元される — Unity のドメインリロードに近い形で、
	* ゲームプレイデータ(さらにはコンポーネント型の構成そのもの)がリロードを
	* またいで残る(ただし、リビルドをまたいでリフレクションフィールドの
	* レイアウトに互換性がある場合に限る — ロジックの追加は常に安全だが、
	* リフレクションフィールドの名前変更/型変更はそのフィールドのデータを失う)。
	*/
	class HotReload :public NonTransferable
	{
	public:
		HotReload() = default;
		~HotReload();

		/// [EN] Shadow-copies and loads UserProject.dll, calls SC_OnGameLoad, then restores any components captured by a prior Unload.
		/// [JP] UserProject.dll をシャドウコピーしてロードし、SC_OnGameLoad を呼び、前回の Unload で取得したコンポーネントがあれば復元する。DLLやエントリポイントが見つからない場合は false を返す。
		Bool Load(World& world);

		/// [EN] Captures and destroys every component this module owns (see class doc), calls SC_OnGameUnload, frees the module, and deletes its shadow copy.
		/// [JP] このモジュールが所有する全コンポーネントを取得・破棄し(クラスのドキュメント参照)、SC_OnGameUnload を呼び、モジュールを解放し、そのシャドウコピーを削除する。
		void Unload(World& world);

		/// [EN] Polls the build process, the source tree, and the DLL's timestamp once per frame; triggers an auto-build or a reload as needed.
		/// [JP] ビルドプロセス・ソースツリー・DLLのタイムスタンプを毎フレーム確認し、必要に応じて自動ビルドまたはリロードを行う。
		void Tick(World& world);

	private:
		/// [EN] Handle to the currently loaded shadow-copy module (nullptr while unloaded).
		/// [JP] 現在ロード中のシャドウコピーモジュールへのハンドル(未ロード時は nullptr)。
		HMODULE handle_ = nullptr;

		/// [EN] Path of the shadow-copy DLL currently loaded via handle_.
		/// [JP] handle_ が指す、現在ロード中のシャドウコピーDLLのパス。
		std::filesystem::path shadowPath_;

		/// [EN] Source UserProject.dll's last-write time at the moment it was last shadow-copied.
		/// [JP] 直近にシャドウコピーした時点での、元となる UserProject.dll の最終更新時刻。
		Uint64 lastWriteTime_ = 0;

		/// [EN] DLL write time seen on the previous Tick, awaiting confirmation that it has stopped changing.
		/// [JP] 前回Tick時に観測したDLLの更新時刻。変化が止まったかどうかの確認待ち状態で使う。
		Uint64 pendingDllWriteTime_ = 0;

		/// [EN] GetTickCount64() timestamp when pendingDllWriteTime_ was first observed.
		/// [JP] pendingDllWriteTime_ を最初に観測した時点の GetTickCount64()。
		Uint64 pendingDllStableSinceTick_ = 0;

		using OnGameLoadFn = void(*)(World&);
		using OnGameUnloadFn = void(*)(World&);
		using SetImGuiContextFn = void(*)(ImGuiContext*);

		OnGameLoadFn onGameLoad_ = nullptr;
		OnGameUnloadFn onGameUnload_ = nullptr;
		SetImGuiContextFn setImGuiContext_ = nullptr;

		/// [EN] Path to MSBuild.exe, resolved once via vswhere on first use. Empty if it could not be found.
		/// [JP] MSBuild.exe のパス。初回使用時に vswhere 経由で一度だけ解決する。見つからなければ空。
		std::filesystem::path msbuildPath_;

		/// [EN] True once msbuildPath_ resolution (success or failure) has been attempted.
		/// [JP] msbuildPath_ の解決を(成否に関わらず)一度試みたら true になる。
		Bool msbuildResolved_ = false;

		/// [EN] Handle of the currently running background build process (nullptr when idle).
		/// [JP] 現在実行中のバックグラウンドビルドプロセスのハンドル(アイドル時は nullptr)。
		HANDLE buildProcessHandle_ = nullptr;

		/// [EN] Read end of the pipe the build process's stdout/stderr are redirected to (nullptr when idle). Drained every Tick() so the child never blocks on a full pipe buffer.
		/// [JP] ビルドプロセスの標準出力/標準エラーのリダイレクト先パイプの読み取り側(アイドル時は nullptr)。子プロセスがパイプバッファ満杯でブロックしないよう、毎Tick()で読み出す。
		HANDLE buildOutputReadPipe_ = nullptr;

		/// [EN] Accumulated stdout/stderr text from the currently (or most recently) running build, for logging on failure.
		/// [JP] 現在(または直近)のビルドの標準出力/標準エラーの蓄積テキスト。失敗時のログ出力に使う。
		std::string buildOutput_;

		/// [EN] Set when a build this class started succeeds, telling the next Tick() to reload right away instead of waiting for the DLL write-time watcher to settle.
		/// [JP] このクラスが起動したビルドが成功したときに立つ。DLLの更新時刻監視が安定するのを待たず、次の Tick() で直ちにリロードするよう伝える。
		Bool reloadRequested_ = false;

		/// [EN] Reads whatever's currently buffered in buildOutputReadPipe_ (non-blocking) into buildOutput_.
		/// [JP] buildOutputReadPipe_ に現在溜まっている分を(ノンブロッキングで) buildOutput_ へ読み出す。
		void DrainBuildOutput();

		/// [EN] Best-effort deletion of shadow DLLs and hot-reload PDBs left over from earlier reloads (and from any session that ended without a clean Unload). The currently loaded shadow copy is skipped, and files still held open elsewhere simply fail to delete and are retried next build.
		/// [JP] 過去のリロードで残ったシャドウDLLとホットリロード用PDBを可能な範囲で削除する(正常な Unload を経ずに終了したセッションの残骸も含む)。現在ロード中のシャドウコピーは対象外とし、他で開かれたままのファイルは削除に失敗するだけで、次回ビルド時に再度試行される。
		void CleanupStaleArtifacts()const;

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

		/// [EN] Component field snapshots captured from the previously loaded module, awaiting restoration onto the same Actor once the new module re-registers the type.
		/// [JP] 直前にロードされていたモジュールから取得したコンポーネントのフィールドスナップショット。新しいモジュールが型を再登録した後、同じActorへ復元されるのを待っている。
		DynamicArray<std::pair<Actor*, SerializedComponent>> capturedComponents_;

		/// [EN] Type names the currently loaded module added to ReflectionRegistry, determined by diffing the registry across LoadLibrary. Erased again before the module is freed.
		/// [JP] 現在ロード中のモジュールが ReflectionRegistry へ追加した型名。LoadLibrary の前後でレジストリを差分比較して求める。モジュールを解放する前に再び削除される。
		DynamicArray<String> registeredReflectionNames_;

		/// [EN] Type names the currently loaded module added to PayloadRegistry (see registeredReflectionNames_).
		/// [JP] 現在ロード中のモジュールが PayloadRegistry へ追加した型名(registeredReflectionNames_ を参照)。
		DynamicArray<String> registeredPayloadNames_;

		/// [EN] Erases every reflection/payload entry this module registered. Must run while the module is still mapped: the registries hold std::function objects whose destructors live inside it, so leaving them behind would make the next Register() (which assigns over the old value) destroy a std::function whose code has already been unmapped.
		/// [JP] このモジュールが登録したリフレクション/ペイロードのエントリを全て削除する。モジュールがまだメモリ上にある間に実行する必要がある: レジストリが保持する std::function のデストラクタはモジュール内にあるため、放置すると次回の Register()(古い値へ代入する)が、既にアンマップされたコードを持つ std::function を破棄しようとしてしまう。
		void UnregisterModuleReflection();

		/// [EN] Returns whether id's ComponentMetadata function pointers live inside the currently loaded module (handle_).
		/// [JP] id の ComponentMetadata の関数ポインタが、現在ロード中のモジュール(handle_)の中にあるかどうかを返す。
		[[nodiscard]] Bool IsOwnedByLoadedModule(ComponentID id)const;

		/// [EN] For every live component whose code lives in handle_: captures its fields into capturedComponents_, removes it from its Actor, then destroys that type's now-empty sparse-set storage container.
		/// [JP] handle_ 内にコードがある全ての生きたコンポーネントについて: フィールドを capturedComponents_ へ取得し、その Actor から削除した上で、空になったその型のスパースセットストレージコンテナを破棄する。
		void CaptureAndDestroyOwnedComponents(World& world);

		/// [EN] Re-adds every component in capturedComponents_ to its original Actor (using the freshly reloaded module's registration) and restores its captured field values. Clears capturedComponents_ when done.
		/// [JP] capturedComponents_ の各コンポーネントを(リロードされたモジュールの再登録を使って)元の Actor へ再追加し、取得済みのフィールド値を復元する。完了後 capturedComponents_ をクリアする。
		void RestoreCapturedComponents(World& world);

		[[nodiscard]] static std::filesystem::path SourceDllPath();

		[[nodiscard]] static std::filesystem::path UserProjectSourceDirectory();

		[[nodiscard]] static std::filesystem::path UserProjectVcxprojPath();

		[[nodiscard]] static Uint64 GetLastWriteTime(const std::filesystem::path& path);

		/// [EN] Recursively finds the newest last-write time among UserProject's .h/.cpp files. Returns 0 if the directory can't be scanned.
		/// [JP] UserProject 配下の .h/.cpp ファイルのうち、最も新しい最終更新時刻を再帰的に探す。ディレクトリをスキャンできない場合は 0 を返す。
		[[nodiscard]] static Uint64 ScanSourceLastWriteTime();

		/// [EN] Resolves MSBuild.exe's path via vswhere.exe (a synchronous, one-time call).
		/// [JP] vswhere.exe 経由で MSBuild.exe のパスを解決する(同期・一度きりの呼び出し)。
		[[nodiscard]] static std::filesystem::path FindMSBuild();

		/// [EN] Launches an async MSBuild.exe build of UserProject.vcxproj. No-op if MSBuild couldn't be resolved.
		/// [JP] UserProject.vcxproj の非同期ビルドを MSBuild.exe で起動する。MSBuildが見つからなければ何もしない。
		void TriggerBuild();

		/// [EN] Checks whether the background build process has finished; logs the result and clears buildProcessHandle_ if so.
		/// [JP] バックグラウンドビルドプロセスが終了したか確認する。終了していれば結果をログに出し buildProcessHandle_ をクリアする。
		void PollBuildProcess();
	};
}
