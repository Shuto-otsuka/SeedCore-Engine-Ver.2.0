#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/GoogleAuth.h>
#include <FoundationEngine/Resource/Sharing/GoogleDocument.h>
#include <FoundationEngine/Resource/Sharing/GoogleDrive.h>
#include <FoundationEngine/Resource/Sharing/HttpClient.h>
#include <FoundationEngine/Resource/Sharing/SharedCatalog.h>
#include <FoundationEngine/Resource/Sharing/SharedLockTable.h>

namespace SeedCore
{
	/**
	* [EN]
	* Everything the sharing layer needs that the team has to agree on
	* beforehand, read from the project's own configuration file.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有機能が必要とするもののうち、チームで事前に決めておく必要がある
	* 設定。プロジェクト内の設定ファイルから読み込む。
	*/
	struct SharingConfig
	{
		/// [EN] OAuth client of the Editor as registered with Google.
		/// [JP] Google に登録した、Editor としての OAuth クライアント。
		String clientId_;
		String clientSecret_;

		/// [EN] The document holding the catalog of shared assets.
		/// [JP] 共有アセットのカタログを保持しているドキュメント。
		String catalogDocumentId_;

		/// [EN] The document holding the edit leases.
		/// [JP] 編集 Lease を保持しているドキュメント。
		String lockDocumentId_;

		/// [EN] The Drive folder that stores asset contents, one file per content hash.
		/// [JP] アセットの中身を保存する Drive のフォルダ。中身のハッシュごとに1ファイル。
		String blobFolderId_;

		/// [EN] The Drive folder that mirrors the workspace's Assets tree, so an artist can drop a file straight into the folder it belongs in.
		/// [JP] ワークスペースの Assets のフォルダ構成を映した Drive のフォルダ。アーティストが、属するフォルダへそのまま置けるようにするため。
		String assetsFolderId_;

		/// [EN] Folder inside the project that shared content belongs to, normally "UserProject".
		/// [JP] 共有対象の内容が属する、プロジェクト内のフォルダ。通常は "UserProject"。
		String workspace_;

		/// [EN] Name other members see beside a lock this Editor holds.
		/// [JP] この Editor が持つ Lock の横に、他のメンバーが見る名前。
		String owner_;
	};

	/**
	* [EN]
	* What this machine holds for one shared asset: which revision it was
	* fetched or published at, and what its files hashed to at that
	* moment. Comparing the files against these hashes is how local
	* changes are told apart from an untouched copy.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この PC が共有アセット1件について保持している内容。どの Revision で
	* 取得・公開したかと、その時点でファイルがどのハッシュだったか。この
	* ハッシュと突き合わせることで、手を加えた写しと無変更の写しを区別する。
	*/
	struct WorkspaceFile
	{
		/// [EN] Where the file sits inside the workspace.
		/// [JP] ワークスペース内でのそのファイルの位置。
		String path_;

		/// [EN] What its contents hashed to when it was fetched or published.
		/// [JP] 取得・公開した時点で、その中身がどのハッシュだったか。
		String hash_;

		/// [EN] Its size and last-write time at that same moment, which a check can compare before hashing anything.
		/// [JP] 同じ時点でのサイズと最終更新時刻。ハッシュを計算する前の比較に使える。

		/// [EN] Re-hashing every shared file on a timer would read gigabytes from disk; these two rule most files out first.
		/// [JP] 定期的に全共有ファイルをハッシュし直すとギガバイト単位の読み込みになる。この2つが、ほとんどのファイルを先に除外する。
		Uint64 size_ = 0;
		Int64 stamp_ = 0;
	};

	struct WorkspaceRecord
	{
		/// [EN] The revision this machine is based on, which a publish later states as its starting point.
		/// [JP] この PC が元にしている Revision。後の Publish が起点として示す値。
		Uint64 revision_ = 0;

		/// [EN] Every file of the asset as this machine last saw it.
		/// [JP] そのアセットの各ファイルを、この PC が最後に見た時点の姿で持つ。
		DynamicArray<WorkspaceFile> files_;
	};

	/**
	* [EN]
	* What the worker wants done. Requests are handed over from the Editor
	* thread and carried out in the background, since each one is a
	* network round trip that must not stall drawing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ワーカーに依頼する内容。Editor のスレッドから渡され、裏で実行される。
	* どれも通信を伴い、描画を止めてはならないため。
	*/
	enum class SharingAction
	{
		/// [EN] Re-read the catalog and the lock table.
		/// [JP] カタログと Lock 表を読み直す。
		Refresh,

		/// [EN] Take the edit lease on one scope.
		/// [JP] ある範囲の編集 Lease を取得する。
		Checkout,

		/// [EN] Give up a held lease.
		/// [JP] 保持している Lease を手放す。
		Release,

		/// [EN] Bring the local copy up to the catalog's revision.
		/// [JP] ローカルの写しを、カタログの Revision まで引き上げる。
		Get,

		/// [EN] Share a local asset for the first time.
		/// [JP] ローカルのアセットを初めて共有する。
		Register,

		/// [EN] Send local changes to the team as a new revision.
		/// [JP] ローカルの変更を、新しい Revision としてチームへ送る。
		Publish,

		/// [EN] Mark a shared asset as retired.
		/// [JP] 共有アセットを廃止済みにする。
		Retire,

		/// [EN] Follow a scene from now on, so the pieces other members publish in it are picked up as they appear.
		/// [JP] その Scene を以後追いかける。他のメンバーがその中で Publish した断片を、現れ次第拾うようにするため。
		OpenScene,
	};

	/**
	* [EN]
	* One queued piece of work, named by what it acts on rather than by
	* how it is carried out.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 待ち行列に入る作業1つ。どう実行するかではなく、何に対して行うかで
	* 表す。
	*/
	struct SharingRequest
	{
		/// [EN] What to do.
		/// [JP] 何をするか。
		SharingAction action_ = SharingAction::Refresh;

		/// [EN] Which shared asset it concerns; empty for a plain refresh.
		/// [JP] どの共有アセットに関するものか。単なる再読み込みでは空。
		String assetId_;

		/// [EN] Which scope of it, for a checkout or a release.
		/// [JP] その中のどの範囲か。Checkout と Release で使う。
		String scope_;

		/// [EN] Where the asset sits locally, for a first-time share.
		/// [JP] 初めて共有する際の、ローカルでの位置。
		String path_;

		/// [EN] The engine's 32-bit identifier and asset kind, for a first-time share.
		/// [JP] 初めて共有する際の、エンジンの32ビット識別子とアセットの種類。
		Uint32 runtimeId_ = 0;
		Int32 type_ = 0;
	};

	/**
	* [EN]
	* How one shared asset stands between this workspace and the library.
	* Only the two states the catalog cannot state by itself are kept here,
	* since the rest is already in the catalog and the lock table.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有アセット1つが、このワークスペースとライブラリの間でどう立って
	* いるか。カタログだけでは言えない2つの状態のみを持つ。残りはカタログ
	* と Lock 表に既にあるため。
	*/
	struct SharedProgress
	{
		/// [EN] Which shared asset this describes.
		/// [JP] どの共有アセットについてのものか。
		String assetId_;

		/// [EN] Local files differ from what arrived, so this workspace holds work the team has not seen.
		/// [JP] ローカルのファイルが、届いた時点と違う。つまりチームがまだ見ていない作業を抱えている。
		Bool modified_ = false;

		/// [EN] Modified locally while the library also moved ahead, which is the one state that needs a person.
		/// [JP] ローカルで変更した上に、ライブラリも先へ進んでいる。人の判断が必要な唯一の状態。
		Bool conflicted_ = false;
	};

	/**
	* [EN]
	* What the Editor thread is allowed to look at: a copy of the shared
	* state taken while the worker was not writing to it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Editor のスレッドが見てよいもの。ワーカーが書き換えていない瞬間に
	* 取った、共有状態の写し。
	*/
	struct SharingSnapshot
	{
		/// [EN] Whether the last exchange with Google succeeded.
		/// [JP] 直近の Google とのやり取りが成功したかどうか。
		Bool online_ = false;

		/// [EN] What the worker is doing right now, for the Editor to show.
		/// [JP] ワーカーが今行っていること。Editor で表示するために使う。
		String operation_;

		/// [EN] The last failure, if any.
		/// [JP] 直近の失敗内容。
		String error_;

		/// [EN] The shared catalog as last read.
		/// [JP] 直近に読み取った共有カタログ。
		DynamicArray<SharedAsset> assets_;

		/// [EN] Every lease currently held by anyone.
		/// [JP] 現在、誰かが保持している全ての Lease。
		DynamicArray<EditLease> leases_;

		/// [EN] Only the assets that stand apart from the library; one that matches it is left out.
		/// [JP] ライブラリと食い違っているアセットのみ。一致しているものは載せない。
		DynamicArray<SharedProgress> progress_;

		/// [EN] Changes whenever the contents above change, so the Editor knows when to rebuild its view.
		/// [JP] 上の内容が変わるたびに変化する。Editor が表示を作り直す契機にする。
		Uint64 revision_ = 0;
	};

	/**
	* [EN]
	* The background half of asset sharing. It owns every connection to
	* Google, carries out queued work one item at a time, keeps the edit
	* leases alive while the Editor runs, and leaves a snapshot behind for
	* the Editor thread to read.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット共有の裏側。Google との接続を全て所有し、待ち行列の作業を
	* 1つずつ実行し、Editor が動いている間は編集 Lease を生かし続け、
	* Editor のスレッドが読むための写しを残す。
	*/
	class SEEDCORE_API SharingWorker
	{
	public:
		/**
		* [EN]
		* Prepares the worker for a project, without connecting yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロジェクト用にワーカーを用意する。この時点ではまだ接続しない。
		*/
		SharingWorker(const std::filesystem::path& projectRoot, const SharingConfig& config);

		/**
		* [EN]
		* Stops the thread and releases every lease this Editor holds.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スレッドを止め、この Editor が持つ全ての Lease を解放する。
		*/
		~SharingWorker();

		/// [EN] Copying is disallowed because one worker owns one thread and one set of leases.
		/// [JP] 1つのワーカーが1つのスレッドと1組の Lease を所有するため、コピーは禁止する。
		SharingWorker(const SharingWorker&) = delete;
		SharingWorker& operator=(const SharingWorker&) = delete;

		/**
		* [EN]
		* Starts the background thread. Signing in happens on that thread,
		* so the Editor window never waits for a browser.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 裏のスレッドを開始する。ログインもそのスレッドで行うため、Editor
		* のウィンドウがブラウザを待つことはない。
		*/
		void Start();

		/**
		* [EN]
		* Stops the thread, releasing leases so other members do not have
		* to wait for them to lapse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スレッドを止める。その際に Lease を解放し、他のメンバーが失効を
		* 待たなくて済むようにする。
		*/
		void Stop();

		/**
		* [EN]
		* Hands one piece of work to the background thread and returns at
		* once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 作業を1つ裏のスレッドへ渡し、すぐに戻る。
		*/
		void Enqueue(const SharingRequest& request);

		/**
		* [EN]
		* Returns a copy of the shared state for the Editor thread to read
		* without holding anything up.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有状態の写しを返す。Editor のスレッドが、何も待たせずに読める
		* ようにするため。
		*/
		SharingSnapshot Snapshot()const;

		/**
		* [EN]
		* Hands over the assets whose files this worker has replaced since
		* the last call, and forgets them. The Editor uses this to reload
		* what it already has open rather than keeping the old contents
		* until it is restarted.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の呼び出し以降にこのワーカーがファイルを差し替えたアセットを
		* 引き渡し、こちらからは忘れる。Editor はこれを使って、開いたままの
		* ものを読み直す。再起動まで古い中身を持ち続けないようにするため。
		*/
		void ConsumeChanged(DynamicArray<Uint32>& assets);

		/**
		* [EN]
		* Hands over the workspace paths of files taken in from the
		* library's Assets folder since the last call, and forgets them.
		* The Editor scans them so they get their identity, then shares
		* them with the team.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の呼び出し以降にライブラリの Assets フォルダから取り込んだ
		* ファイルの位置を引き渡し、こちらからは忘れる。Editor はそれらを
		* 走査して識別情報を与え、チームへ共有する。
		*/
		void ConsumeImported(DynamicArray<String>& paths);

	private:
		/**
		* [EN]
		* The background thread itself: signs in, then loops between
		* carrying out requests, renewing leases and re-reading the shared
		* state until asked to stop.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 裏のスレッド本体。ログインし、その後は停止を求められるまで、要求の
		* 実行・Lease の更新・共有状態の読み直しを繰り返す。
		*/
		void Run();

		/**
		* [EN]
		* Carries out one request and records what happened.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要求を1つ実行し、その結果を記録する。
		*/
		void Process(const SharingRequest& request);

		/**
		* [EN]
		* Brings the local copy of an asset up to the catalog's revision,
		* downloading only the files whose contents differ.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセットのローカルの写しを、カタログの Revision まで引き上げる。
		* 中身が異なるファイルだけをダウンロードする。
		*/
		Bool Get(const String& assetId);

		/**
		* [EN]
		* Uploads whichever of an asset's files changed and records them as
		* the next revision.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセットのファイルのうち変更されたものをアップロードし、次の
		* Revision として記録する。
		*/
		Bool Publish(const String& assetId);

		/**
		* [EN]
		* Shares a local asset for the first time, together with the .meta
		* beside it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローカルのアセットを、隣の .meta と一緒に初めて共有する。
		*/
		Bool Register(const SharingRequest& request);

		/**
		* [EN]
		* Keeps the library's Assets folder shaped like the workspace's
		* own, and takes in whatever has been dropped into it. Folders
		* are created where they are missing; files are downloaded to the
		* matching place and moved aside so they arrive only once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ライブラリの Assets フォルダを、ワークスペース側と同じ形に保ち、
		* そこへ置かれたものを取り込む。足りないフォルダは作り、この PC が
		* まだ持っていない中身のファイルをダウンロードする。
		*/
		void Mirror(const std::filesystem::path& localFolder, const String& driveFolderId);

		/**
		* [EN]
		* Uploads one local file unless its contents are already stored,
		* and describes where they ended up.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローカルのファイル1つを、中身がまだ保存されていない場合に限り
		* アップロードし、その置き場所を返す。
		*/
		Bool Store(const String& logicalPath, SharedFile& stored);

		/**
		* [EN]
		* Turns a workspace-relative path into a path on this machine.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ワークスペース内の位置を、この PC 上のパスへ変換する。
		*/
		std::filesystem::path Local(const String& logicalPath)const;

		/**
		* [EN]
		* Copies the current catalog and leases into the snapshot the
		* Editor thread reads.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のカタログと Lease を、Editor のスレッドが読む写しへ写し取る。
		*/
		void Capture();

		/**
		* [EN]
		* Reads which revision of each asset this machine last got or
		* published, so a publish can state what it was built on.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この PC が各アセットのどの Revision を最後に取得・公開したかを
		* 読み込む。Publish 時に「何を元にしたか」を示すために要る。
		*/
		void LoadWorkspace();

		/**
		* [EN]
		* Writes that record back, so it survives the Editor being closed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* その記録を書き戻す。Editor を閉じても残るようにするため。
		*/
		void SaveWorkspace()const;

	private:
		/// [EN] Where the project sits, which every local path is resolved against.
		/// [JP] プロジェクトの位置。ローカルのパスはすべてここを起点に解決する。
		std::filesystem::path projectRoot_;

		/// [EN] The agreed settings this worker runs with.
		/// [JP] このワーカーが従う、取り決め済みの設定。
		SharingConfig config_;

		/// [EN] The connection layer, in the order they depend on each other.
		/// [JP] 通信の各層。互いに依存する順に並べている。
		HttpClient http_;
		GoogleAuth auth_;
		GoogleDocument document_;
		GoogleDrive drive_;
		SharedCatalog catalog_;
		SharedLockTable locks_;

		/// [EN] The background thread and the flag that asks it to finish.
		/// [JP] 裏のスレッドと、終了を促すための印。
		std::thread thread_;
		std::atomic<Bool> running_ = false;

		/// [EN] What this machine holds for each shared asset it has ever fetched or published.
		/// [JP] この PC が、取得・公開したことのある各共有アセットについて保持している内容。
		std::unordered_map<String, WorkspaceRecord> workspace_;

		/// [EN] The scene the Editor currently has open, which is the only one whose pieces are followed.
		/// [JP] Editor が今開いている Scene。断片を追いかける対象はこの1つだけ。

		/// [EN] Following every scene would mean reading one document per scene on every check, for scenes nobody is looking at.
		/// [JP] 全ての Scene を追うと、誰も見ていない Scene の分まで、確認のたびにドキュメントを1つずつ読むことになる。
		String openScene_;

		/// [EN] Which revision of each piece of that scene this machine last wrote out.
		/// [JP] その Scene の各断片について、この PC が最後に書き出した Revision。
		std::unordered_map<String, Uint64> sceneRevisions_;

		/// [EN] Work waiting to be carried out, oldest first.
		/// [JP] 実行を待っている作業。古いものから順に並ぶ。
		std::queue<SharingRequest> requests_;

		/// [EN] Guards the queue against the Editor thread adding while the worker takes.
		/// [JP] Editor のスレッドが追加している最中にワーカーが取り出すことを防ぐ。
		mutable std::mutex requestMutex_;

		/// [EN] The copy the Editor thread reads, and the lock that keeps it whole while it is read.
		/// [JP] Editor のスレッドが読む写しと、読んでいる間それを壊さないための錠。
		SharingSnapshot snapshot_;
		mutable std::mutex snapshotMutex_;

		/// [EN] Assets whose files were replaced and not yet handed to the Editor, guarded by the same lock as the snapshot.
		/// [JP] ファイルを差し替えたが、まだ Editor へ渡していないアセット。写しと同じ錠で守る。
		DynamicArray<Uint32> changed_;

		/// [EN] Workspace paths taken in from the library's Assets folder and not yet handed to the Editor.
		/// [JP] ライブラリの Assets フォルダから取り込み、まだ Editor へ渡していない位置。
		DynamicArray<String> imported_;
	};
}
