#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Adds newly created source files to a project inside an already-running
	* Visual Studio instance, via COM automation (EnvDTE, located through the
	* Running Object Table) — the same call path Solution Explorer's own
	* "Add Existing Item" uses. Because Visual Studio itself performs the
	* file-system/project-file write, it never sees the change as an
	* external modification, so the "this project has been modified outside
	* the source editor" reload prompt (and the debug-session interruption
	* that accepting it would cause) never appears.
	*
	* No COM object is retained between calls: every call re-resolves the
	* Running Object Table from scratch, since the running Visual Studio
	* instance (or whether it still has this solution open at all) can
	* change between one script creation and the next.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既に起動しているVisual StudioインスタンスのCOM自動化(EnvDTE、Running
	* Object Table経由で探す)を使い、新規作成したソースファイルをプロジェクトへ
	* 追加する — Solution Explorerの「既存の項目を追加」自体が使っているのと
	* 同じ呼び出し経路。Visual Studio自身がファイルシステム/プロジェクト
	* ファイルへの書き込みを行うため、その変更が外部からの変更として
	* 検知されることが無く、「プロジェクトが外部で変更されました」という
	* 再読み込み確認(それを受け入れた際に発生するデバッグセッションの中断)が
	* 一切発生しない。
	*
	* 呼び出し間でCOMオブジェクトは保持しない — 毎回 Running Object Table を
	* 最初から解決し直す。実行中のVisual Studioインスタンス(そもそも
	* このソリューションを開いているかどうかを含む)は、スクリプト作成の
	* 都度変わりうるため。
	*/
	class VisualStudioAutomation
	{
	public:
		/**
		* [EN]
		* Finds a running Visual Studio instance with solutionPath open and
		* adds headerPath/cppPath to the project named projectName within
		* it. Returns false (doing nothing to the project file) if no
		* matching instance is found or any COM step fails — the caller is
		* expected to fall back to direct project-file registration in
		* that case.
		*
		* Something else can register a filter/file directly into
		* UserProject.vcxproj.filters on disk without this running Visual
		* Studio instance ever finding out (chiefly SyncScript.py's own
		* PreBuildEvent step) — Visual Studio only refreshes its in-memory
		* project model from disk on an explicit reload, which this class
		* deliberately never forces (that's the whole point of using COM
		* automation in the first place: no reload prompt, no interrupted
		* debug session). So before creating any new filter, this reads
		* UserProject.vcxproj.filters fresh from disk and treats a filter
		* that's on disk but missing from Visual Studio's live model as a
		* sign that the model is stale, refusing to create a duplicate and
		* returning false instead — pushing the whole call back to the
		* file-based fallback, which is always correct because it re-reads
		* the file itself.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* solutionPath を開いている実行中のVisual Studioインスタンスを探し、
		* その中の projectName という名前のプロジェクトへ headerPath/cppPath を
		* 追加する。一致するインスタンスが見つからない、またはCOM呼び出しの
		* いずれかが失敗した場合は(プロジェクトファイルには何もせず) false を
		* 返す — その場合、呼び出し側はプロジェクトファイルへの直接登録に
		* フォールバックする想定。
		*
		* 今動いているこのVisual Studioインスタンスが気づかないまま、他の何か
		* (主に SyncScript.py 自身の PreBuildEvent)が UserProject.vcxproj.filters
		* へ直接フィルタ/ファイルを登録することがある — Visual Studio は明示的な
		* リロードでしかディスクからメモリ上のプロジェクトモデルを更新しない。
		* このクラスは意図的にそのリロードを一切強制しない(そもそもCOM自動化を
		* 使っている理由そのもの: 再読み込み確認を出さず、デバッグセッションも
		* 中断させないため)。そのため新しいフィルタを作成する前に、
		* UserProject.vcxproj.filters をディスクから読み直し、ディスク上には
		* あるのに Visual Studio のライブなモデルには無いフィルタを「モデルが
		* 古い」サインとみなし、重複作成を拒否して false を返す — 呼び出し全体を
		* ファイルベースのフォールバックへ押し戻す。そちらは常にファイル自体を
		* 読み直すため、常に正しい。
		*/
		[[nodiscard]] static Bool TryAddFilesToProject(const std::filesystem::path& solutionPath, const std::string& projectName, const std::filesystem::path& headerPath, const std::filesystem::path& cppPath);

	private:
		/// [EN] Invokes a zero/one-arg property-get, property-put, or method member by name via IDispatch, using late binding (GetIDsOfNames + Invoke) so no generated type-library wrapper is needed.
		/// [JP] IDispatch経由で、プロパティ取得/設定またはメソッドを名前指定で(引数0〜1個)呼び出す。型ライブラリから生成したラッパーを使わず、レイトバインディング(GetIDsOfNames + Invoke)で行う。
		[[nodiscard]] static Bool InvokeMember(IDispatch* dispatch, const Wchar* name, WORD flags, VARIANT* arg, VARIANT* result);

		/// [EN] Convenience wrapper over InvokeMember for a no-argument property-get that returns an IDispatch (e.g. DTE.Solution, Project.ProjectItems). Returns nullptr on failure or if the property wasn't itself a dispatchable object.
		/// [JP] 引数無しのプロパティ取得で IDispatch を返すもの(DTE.Solution、Project.ProjectItems 等)向けの、InvokeMember の簡易ラッパー。失敗時、またはプロパティ自体がディスパッチ可能なオブジェクトでなかった場合は nullptr を返す。
		[[nodiscard]] static IDispatch* GetDispatchProperty(IDispatch* dispatch, const Wchar* name);

		/// [EN] Convenience wrapper over InvokeMember for a no-argument property-get that returns a string (e.g. Solution.FullName, Project.Name). Returns an empty string on failure.
		/// [JP] 引数無しのプロパティ取得で文字列を返すもの(Solution.FullName、Project.Name 等)向けの、InvokeMember の簡易ラッパー。失敗時は空文字列を返す。
		[[nodiscard]] static std::wstring GetStringProperty(IDispatch* dispatch, const Wchar* name);

		/// [EN] Convenience wrapper over InvokeMember for a no-argument property-get that returns a count (e.g. Projects.Count, Filters.Count). Coerces via VariantChangeType instead of requiring the property to already be VT_I4 — a strict type check would silently read as "0 items" for any collection whose Count happens to come back as VT_I2/VT_UI4/etc, which would make an existing filter look absent and get recreated as a duplicate every time. Returns 0 on failure.
		/// [JP] 引数無しのプロパティ取得で件数を返すもの(Projects.Count、Filters.Count 等)向けの、InvokeMember の簡易ラッパー。プロパティが最初から VT_I4 であることを要求せず、VariantChangeType で型強制して読む — 型の完全一致だけを見ていると、Count がたまたま VT_I2/VT_UI4 等で返ってきたコレクションを「0件」と誤読し、既存のフィルタが見えず毎回重複生成してしまう。失敗時は 0 を返す。
		[[nodiscard]] static Int GetInt32Property(IDispatch* dispatch, const Wchar* name);

		/// [EN] Enumerates the Running Object Table for "!VisualStudio.DTE.*" monikers, returning the IDispatch of the first one whose Solution.FullName matches solutionPath (case-insensitively). The caller owns the returned pointer (must Release() it) and must CoUninitialize() only after it's done using it. Returns nullptr if none match.
		/// [JP] Running Object Table を "!VisualStudio.DTE.*" というモニカで列挙し、Solution.FullName が solutionPath と(大文字小文字を無視して)一致する最初のものの IDispatch を返す。返されたポインタの所有権は呼び出し側にあり(Release() が必要)、使い終わるまでは CoUninitialize() してはならない。一致するものが無ければ nullptr を返す。
		[[nodiscard]] static IDispatch* FindDTEForSolution(const std::filesystem::path& solutionPath);

		/// [EN] Finds a child filter named name directly under owner (a VCProject or VCFilter — both expose a Filters collection and an AddFilter(name) method in the VCProjectEngine object model), creating it via AddFilter() if it doesn't exist yet. onDiskFiltersContent is the raw text of UserProject.vcxproj.filters as last read from disk (see TryAddFilesToProject's own doc comment for why this matters): if name doesn't turn up in owner's live Filters collection but its Include="..." does appear literally in onDiskFiltersContent, that means some process other than this running Visual Studio instance (typically SyncScript.py's PreBuildEvent) already registered it — Visual Studio's in-memory project model just hasn't caught up, and calling AddFilter() here would write a second, duplicate <Filter> node for the same name. In that situation this returns nullptr instead, refusing to create anything, which makes the whole call chain fail and the caller fall back to registering directly against the file, which is always disk-truth-correct. The caller owns the returned pointer. Returns nullptr on a COM failure (e.g. AddFilter() itself failing) too.
		/// [JP] owner(VCProject または VCFilter — どちらも VCProjectEngine オブジェクトモデルにおいて Filters コレクションと AddFilter(name) メソッドを持つ)の直下から name という名前の子フィルタを探す。無ければ AddFilter() で作成する。onDiskFiltersContent は UserProject.vcxproj.filters を直近にディスクから読み込んだ生テキスト(理由は TryAddFilesToProject 自身のドキュメントコメントを参照): owner のライブな Filters コレクションには name が見当たらないのに、その Include="..." が onDiskFiltersContent 内に文字列として存在する場合、今動いているVisual Studioインスタンス以外の何か(典型的には SyncScript.py の PreBuildEvent)が既にそれを登録済みであることを意味する — Visual Studioのメモリ上のプロジェクトモデルが単に追いついていないだけであり、ここで AddFilter() を呼ぶと同じ名前の <Filter> ノードが重複して書き込まれてしまう。この状況では何も作成せず nullptr を返す — これにより呼び出し連鎖全体が失敗し、呼び出し側はファイルへの直接登録(常にディスクの実態と一致する)にフォールバックする。返されたポインタの所有権は呼び出し側にある。COM呼び出し自体が失敗した場合(AddFilter() 自体の失敗など)も nullptr を返す。
		[[nodiscard]] static IDispatch* FindOrCreateFilter(IDispatch* owner, const std::wstring& name, const std::wstring& onDiskFiltersContent);

		/// [EN] Walks/creates the full filter chain for relativeDirectory (backslash-joined path segments, relative to the project root, e.g. L"Script\\Player") starting from vcProject, returning the leaf filter. Returns vcProject itself (AddRef'd) if relativeDirectory is empty, so the return value can always be AddFile()'d against directly regardless of depth. onDiskFiltersContent is forwarded to FindOrCreateFilter at every level (see its doc comment). The caller owns the returned pointer; returns nullptr if any level's FindOrCreateFilter does.
		/// [JP] vcProject を起点に、relativeDirectory(バックスラッシュ区切りのパス、プロジェクトルートからの相対、例: L"Script\\Player")の階層をたどり(無ければ作成し)、末端のフィルタを返す。relativeDirectory が空文字列なら vcProject 自身を(AddRef して)返す — これにより、深さに関わらず戻り値へそのまま AddFile() を呼べる。onDiskFiltersContent は各階層の FindOrCreateFilter へそのまま渡す(詳細はそちらのドキュメントコメントを参照)。返されたポインタの所有権は呼び出し側にある。いずれかの階層で FindOrCreateFilter が nullptr を返せば、こちらも nullptr を返す。
		[[nodiscard]] static IDispatch* EnsureFilterChain(IDispatch* vcProject, const std::wstring& relativeDirectory, const std::wstring& onDiskFiltersContent);
	};
}
