#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Asset/Asset.h>
#include <FoundationEngine/Resource/Sharing/SharingWorker.h>

namespace SeedCore
{
	/**
	* [EN]
	* The Editor's window onto the shared asset library. It answers what
	* the interface needs to know - what the team has, what may be edited,
	* who is editing what - and hands anything that takes a network round
	* trip to the background worker. Nothing here blocks a frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Editor から見た共有アセットライブラリの窓口。画面が知りたいこと
	* （チームが何を持っているか、何を編集してよいか、誰が何を編集中か）に
	* 答え、通信を伴うものは裏のワーカーへ渡す。ここで1フレームが止まる
	* ことはない。
	*/
	class SEEDCORE_API ResourceSync
	{
	public:
		/**
		* [EN]
		* Reads the project's sharing configuration and, when one is
		* present, starts the background worker.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロジェクトの共有設定を読み、設定があれば裏のワーカーを開始する。
		*/
		explicit ResourceSync(const std::filesystem::path& projectRoot);

		/**
		* [EN]
		* Stops the worker, which releases this Editor's edit leases on the
		* way out.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ワーカーを止める。その過程で、この Editor の編集 Lease が解放
		* される。
		*/
		~ResourceSync();

		/// [EN] Copying is disallowed because one Editor has one connection to the library.
		/// [JP] 1つの Editor が持つライブラリへの接続は1つなので、コピーは禁止する。
		ResourceSync(const ResourceSync&) = delete;
		ResourceSync& operator=(const ResourceSync&) = delete;

		/**
		* [EN]
		* Takes the worker's latest state. Called once a frame; it copies
		* what the worker left behind and never waits for the network.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ワーカーの最新の状態を取り込む。毎フレーム呼ばれ、ワーカーが
		* 残した写しを取るだけで、通信を待つことはない。
		*/
		void Update();

		/**
		* [EN]
		* Hands over the assets the library has replaced on disk since the
		* last call, and forgets them. The Editor reloads each one so a
		* revision that arrives while it is running takes effect without a
		* restart.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の呼び出し以降にライブラリがディスク上で差し替えたアセットを
		* 引き渡し、こちらからは忘れる。Editor は各アセットを読み直し、実行中
		* に届いた Revision が再起動なしで反映されるようにする。
		*/
		void ConsumeChangedAsset(DynamicArray<Uint32>& assets);

		/**
		* [EN]
		* Hands over the workspace paths of files the library has taken in
		* from its Assets folder since the last call, and forgets them. The
		* Editor scans them so they get their identity, then shares them
		* with the team.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の呼び出し以降にライブラリが Assets フォルダから取り込んだ
		* ファイルの位置を引き渡し、こちらからは忘れる。Editor はそれらを
		* 走査して識別情報を与え、チームへ共有する。
		*/
		void ConsumeImportedAsset(DynamicArray<String>& paths);

		/**
		* [EN]
		* Whether this project is set up for sharing at all. Everything
		* else is inert while this is false, so a solo project behaves
		* exactly as it did before sharing existed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このプロジェクトが共有の設定を持っているかどうか。false の間は
		* 他の機能も働かないため、1人のプロジェクトは共有機能が無かった頃と
		* 同じように動く。
		*/
		Bool Configured()const;

		/**
		* [EN]
		* Whether the last exchange with the library succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近のライブラリとのやり取りが成功したかどうか。
		*/
		Bool Online()const;

		/**
		* [EN]
		* A number that changes whenever the shared state does. Panels keep
		* the last value they saw and rebuild their view when it differs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有状態が変わるたびに変化する番号。各パネルは最後に見た値を
		* 保持し、違っていれば表示を作り直す。
		*/
		Uint64 Revision()const;

		/**
		* [EN]
		* Appends the assets the team has but this machine does not, so the
		* content browser can show them before anything is downloaded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* チームは持っているがこの PC には無いアセットを追加する。何も
		* ダウンロードしていない段階で、コンテンツブラウザに出せるように
		* するため。
		*/
		void Gather(DynamicArray<AssetRecord>& assets)const;

		/**
		* [EN]
		* Whether the asset belongs to the shared library, addressed either
		* by the engine's identifier or by where it sits on disk.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* そのアセットが共有ライブラリに属しているかどうか。エンジンの
		* 識別子でも、ディスク上の位置でも問い合わせられる。
		*/
		Bool Shared(Uint32 assetId)const;
		Bool Shared(const std::filesystem::path& path)const;

		/**
		* [EN]
		* Whether the asset is in the library but not yet on this machine.
		* Such an asset can be shown and fetched, but not dragged into a
		* scene.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* そのアセットがライブラリにあり、まだこの PC に無いかどうか。
		* 表示と取得はできるが、Scene へドラッグすることはできない。
		*/
		Bool RemoteOnly(Uint32 assetId)const;

		/**
		* [EN]
		* Whether this machine holds changes to the asset that the team has
		* not been sent yet, which is what makes a publish worth offering.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* そのアセットについて、まだチームへ送っていない変更をこの PC が
		* 抱えているかどうか。Publish を勧める根拠になる。
		*/
		Bool Modified(Uint32 assetId)const;

		/**
		* [EN]
		* Whether the asset changed here and in the library both, which no
		* automatic step can settle and a member has to look at.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* そのアセットが、こちらとライブラリの両方で変わっているかどうか。
		* 自動の処理では決められず、メンバーが見る必要がある状態。
		*/
		Bool Conflicted(Uint32 assetId)const;

		/**
		* [EN]
		* Whether a path holds shared content, in which case the Editor's
		* own delete, rename and move must not touch it - those go through
		* the library instead.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* その位置に共有された内容があるかどうか。ある場合、Editor 側の
		* 削除・リネーム・移動で触ってはならない。それらはライブラリを
		* 通して行う。
		*/
		Bool Managed(const std::filesystem::path& path)const;

		/**
		* [EN]
		* Whether this Editor may change the given scope right now. It asks
		* only; taking the right to edit is RequestEdit's job.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Editor が今その範囲を変更してよいかどうか。問い合わせるだけ
		* で、編集権を取りに行くのは RequestEdit の役目。
		*/
		Bool Editable(Uint32 assetId, const String& scope)const;

		/**
		* [EN]
		* Asks for the right to edit the given scope. Called while a member
		* is reaching for a control, so it is rate-limited and returns at
		* once; whether it was granted shows up in Editable.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* その範囲の編集権を要求する。メンバーが操作に手をかけた時点で
		* 呼ばれるため、回数を抑えたうえで即座に戻る。取得できたかどうかは
		* Editable に現れる。
		*/
		void RequestEdit(Uint32 assetId, const String& scope);

		/**
		* [EN]
		* Asks for the library's copy of an asset to be brought down to
		* this machine.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ライブラリにあるアセットを、この PC へ取得するよう要求する。
		*/
		void RequestGet(Uint32 assetId);

		/**
		* [EN]
		* Asks for local changes to be sent to the team as a new revision.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローカルの変更を、新しい Revision としてチームへ送るよう要求
		* する。
		*/
		void RequestPublish(Uint32 assetId);

		/**
		* [EN]
		* Gives up the right to edit, so another member does not have to
		* wait for it to lapse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 編集権を手放す。他のメンバーが失効を待たなくて済むようにする。
		*/
		void RequestRelease(Uint32 assetId, const String& scope);

		/**
		* [EN]
		* Asks for a local asset to be shared with the team for the first
		* time.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローカルのアセットを、初めてチームへ共有するよう要求する。
		*/
		void RequestRegister(const AssetRecord& asset);

		/**
		* [EN]
		* Asks for a shared asset to be retired, which leaves its history
		* in place rather than erasing it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有アセットの廃止を要求する。履歴は消さずに残す形になる。
		*/
		void RequestRetire(Uint32 assetId);

		/**
		* [EN]
		* Asks for the shared state to be re-read now instead of at the
		* next scheduled check.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 次の定期確認を待たず、今すぐ共有状態を読み直すよう要求する。
		*/
		void RequestRefresh();

		/**
		* [EN]
		* Tells the library which scene the Editor now has open, so the
		* entities other members publish in it are picked up as they
		* appear. An empty path means no scene is open.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Editor が今開いている Scene をライブラリへ伝える。その Scene で
		* 他のメンバーが Publish した Entity を、現れ次第拾うようにするため。
		* 空のパスは、Scene を開いていないことを表す。
		*/
		void RequestOpenScene(const std::filesystem::path& path);

		/**
		* [EN]
		* The library's view of one asset - its revision, its files, who
		* published it - or nullptr when the team does not have it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ライブラリから見たアセット1つ分の情報。Revision、構成ファイル
		* など。チームが持っていなければ nullptr。
		*/
		const SharedAsset* GetAsset(Uint32 assetId)const;
		const SharedAsset* GetAsset(const std::filesystem::path& path)const;

		/**
		* [EN]
		* Every edit lease currently held by anyone, for showing who is
		* working on what.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在、誰かが保持している全ての編集 Lease。誰が何を作業中かを
		* 表示するために使う。
		*/
		const DynamicArray<EditLease>& GetLeases()const;

		/**
		* [EN]
		* What the library is doing right now, or empty when it is idle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ライブラリが今行っていること。何もしていなければ空。
		*/
		const String& Operation()const;

		/**
		* [EN]
		* The last failure, in a form that can be shown to the user.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の失敗内容。ユーザーへそのまま表示できる形で返す。
		*/
		const String& Error()const;

	private:
		/**
		* [EN]
		* Turns a path on this machine into the workspace-relative form the
		* library uses, or an empty string when it lies outside.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この PC 上のパスを、ライブラリが使うワークスペース内の位置へ
		* 変換する。範囲外であれば空文字列を返す。
		*/
		String Logical(const std::filesystem::path& path)const;

	private:
		/// [EN] Where the project sits, which every local path is resolved against.
		/// [JP] プロジェクトの位置。ローカルのパスはすべてここを起点に解決する。
		std::filesystem::path projectRoot_;

		/// [EN] The team's settings, read once at startup.
		/// [JP] チームの設定。起動時に一度だけ読む。
		SharingConfig config_;

		/// [EN] Whether a configuration was found at all.
		/// [JP] 設定が見つかったかどうか。
		Bool configured_ = false;

		/// [EN] The background half; absent when this project does not share.
		/// [JP] 裏側の半分。このプロジェクトが共有しない場合は存在しない。
		ResourcePtr<SharingWorker> worker_;

		/// [EN] The worker's state as of the last Update, which everything here answers from.
		/// [JP] 直近の Update 時点におけるワーカーの状態。ここでの回答は全てこれに基づく。
		SharingSnapshot snapshot_;

		/// [EN] Earliest time another edit request may be sent, since a member holding a slider asks every frame.
		/// [JP] 次に編集の要求を送ってよい時刻。スライダーを掴んでいるメンバーは毎フレーム要求するため。
		Uint64 nextEditRequest_ = 0;
	};
}
