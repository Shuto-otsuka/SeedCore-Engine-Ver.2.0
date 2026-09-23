#include <FoundationEngine/Resource/ResourceSync.h>

namespace SeedCore
{
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
	ResourceSync::ResourceSync(const std::filesystem::path& projectRoot) : projectRoot_(projectRoot)
	{
		/// [EN] The absence of this file is how a project says it does not take part in sharing at all.
		/// [JP] このファイルが無いことが、そのプロジェクトは共有に参加しないという意思表示になる。
		std::ifstream stream(projectRoot_ / ".asset" / "config.json");
		if (!stream)
		{
			return;
		}

		nlohmann::json config = nlohmann::json::parse(stream, nullptr, false);
		if (!config.is_object())
		{
			return;
		}

		/// [EN] These four identifiers are what the team agrees on once and shares among themselves.
		/// [JP] この4つの識別子が、チームが一度決めて共有するもの。
		config_.clientId_ = String(config.value("clientId", ""));
		config_.clientSecret_ = String(config.value("clientSecret", ""));
		config_.catalogDocumentId_ = String(config.value("catalogDocumentId", ""));
		config_.lockDocumentId_ = String(config.value("lockDocumentId", ""));
		config_.blobFolderId_ = String(config.value("blobFolderId", ""));

		/// [EN] Leaving this out simply turns the artist-facing folder off; everything else works the same.
		/// [JP] これを省くと、アーティスト向けのフォルダが無効になるだけで、他の動きは変わらない。
		config_.assetsFolderId_ = String(config.value("assetsFolderId", ""));

		/// [EN] Only content under the workspace is shared, so engine and tool folders never end up in the library.
		/// [JP] 共有するのはワークスペース以下だけ。エンジンやツールのフォルダがライブラリへ入ることはない。
		config_.workspace_ = String(config.value("workspace", "UserProject"));

		/// [EN] The name other members see; falling back to the Windows account keeps it from ever being blank.
		/// [JP] 他のメンバーに見える名前。Windows のアカウント名を代わりに使うことで、空欄になることを避ける。
		config_.owner_ = String(config.value("owner", ""));
		if (config_.owner_.str().empty())
		{
			std::wstring account(256, L'\0');
			DWORD length = static_cast<DWORD>(account.size());
			config_.owner_ = GetUserNameW(account.data(), &length) ? String(std::wstring(account.c_str())) : String("unknown");
		}

		/// [EN] Everything above is settings only; the worker is what actually reaches the network, on its own thread.
		/// [JP] ここまでは設定だけ。実際に通信を行うのはワーカーで、別スレッドで動く。
		configured_ = !config_.catalogDocumentId_.str().empty() && !config_.lockDocumentId_.str().empty();
		if (configured_)
		{
			worker_ = MakePtr<SharingWorker>(projectRoot_, config_);
			worker_->Start();
		}
	}

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
	ResourceSync::~ResourceSync()
	{
		/// [EN] Stopping waits for the worker to finish, which is how a lease release is not cut short by the Editor closing.
		/// [JP] 停止ではワーカーの終了を待つ。Editor が閉じることで Lease の解放が途中で切れないようにするため。
		if (worker_)
		{
			worker_->Stop();
		}
	}

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
	void ResourceSync::Update()
	{
		/// [EN] Taking a copy once a frame means every answer within that frame is consistent with the others.
		/// [JP] 1フレームに1度だけ写しを取ることで、そのフレーム内の回答どうしが食い違わなくなる。
		if (worker_)
		{
			snapshot_ = worker_->Snapshot();
		}
	}

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
	void ResourceSync::ConsumeChangedAsset(DynamicArray<Uint32>& assets)
	{
		/// [EN] Nothing to hand over when this project does not share, which keeps the caller free of a Configured check.
		/// [JP] 共有しないプロジェクトでは渡すものが無い。呼び出し側が Configured の確認をしなくて済むようにしている。
		if (worker_)
		{
			worker_->ConsumeChanged(assets);
		}
	}

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
	void ResourceSync::ConsumeImportedAsset(DynamicArray<String>& paths)
	{
		if (worker_)
		{
			worker_->ConsumeImported(paths);
		}
	}

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
	Bool ResourceSync::Configured()const
	{
		return configured_;
	}

	/**
	* [EN]
	* Whether the last exchange with the library succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近のライブラリとのやり取りが成功したかどうか。
	*/
	Bool ResourceSync::Online()const
	{
		return snapshot_.online_;
	}

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
	Uint64 ResourceSync::Revision()const
	{
		return snapshot_.revision_;
	}

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
	void ResourceSync::Gather(DynamicArray<AssetRecord>& assets)const
	{
		for (const SharedAsset& asset : snapshot_.assets_)
		{
			/// [EN] Retired assets are left out, since the team has decided they are no longer part of the project.
			/// [JP] 廃止済みのアセットは出さない。チームが、もうプロジェクトの一部ではないと決めたものであるため。
			if (asset.deleted_)
			{
				continue;
			}

			/// [EN] An asset already on disk is found by the ordinary scan, so listing it again would show it twice.
			/// [JP] 既にディスクにあるアセットは通常の走査で見つかるため、ここで出すと二重に並んでしまう。
			std::filesystem::path local = projectRoot_ / config_.workspace_.str() / asset.path_.str();
			if (std::filesystem::exists(local))
			{
				continue;
			}

			/// [EN] The record is filled in as if the file were there, which is what lets one browser show both kinds.
			/// [JP] ファイルがあるかのように記録を埋める。1つのブラウザで両方を扱えるのはこのため。
			AssetRecord record;
			record.assetID_ = asset.runtimeId_;
			record.type_ = static_cast<AssetType>(asset.type_);
			record.fullpath_ = String(local.generic_string());
			record.path_ = String(std::filesystem::path(config_.workspace_.str()).generic_string() + "/" + asset.path_.str());
			assets.push_back(record);
		}
	}

	/**
	* [EN]
	* Whether the asset belongs to the shared library, addressed by the
	* engine's identifier.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* そのアセットが共有ライブラリに属しているかどうか。エンジンの
	* 識別子で問い合わせる。
	*/
	Bool ResourceSync::Shared(Uint32 assetId)const
	{
		return GetAsset(assetId) != nullptr;
	}

	/**
	* [EN]
	* Whether the asset belongs to the shared library, addressed by
	* where it sits on disk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* そのアセットが共有ライブラリに属しているかどうか。ディスク上の
	* 位置で問い合わせる。
	*/
	Bool ResourceSync::Shared(const std::filesystem::path& path)const
	{
		/// [EN] A path outside the workspace cannot be shared at all, which is answered without looking any further.
		/// [JP] ワークスペースの外にあるものは共有され得ないので、それ以上調べずに答える。
		String logical = Logical(path);
		if (logical.str().empty())
		{
			return false;
		}

		for (const SharedAsset& asset : snapshot_.assets_)
		{
			if (!asset.deleted_ && asset.path_ == logical)
			{
				return true;
			}
		}
		return false;
	}

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
	Bool ResourceSync::RemoteOnly(Uint32 assetId)const
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		/// [EN] Presence on disk is what separates the two, since the catalog lists an asset either way.
		/// [JP] 両者を分けるのはディスク上に在るかどうか。カタログはどちらの場合も載せているため。
		return !std::filesystem::exists(projectRoot_ / config_.workspace_.str() / asset->path_.str());
	}

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
	Bool ResourceSync::Modified(Uint32 assetId)const
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		/// [EN] The worker lists only the assets that stand apart, so anything absent from it agrees with the library.
		/// [JP] ワーカーが載せるのは食い違っているアセットのみ。そこに無いものはライブラリと一致している。
		for (const SharedProgress& progress : snapshot_.progress_)
		{
			if (progress.assetId_ == asset->id_)
			{
				return progress.modified_;
			}
		}
		return false;
	}

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
	Bool ResourceSync::Conflicted(Uint32 assetId)const
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		for (const SharedProgress& progress : snapshot_.progress_)
		{
			if (progress.assetId_ == asset->id_)
			{
				return progress.conflicted_;
			}
		}
		return false;
	}

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
	Bool ResourceSync::Managed(const std::filesystem::path& path)const
	{
		if (!configured_)
		{
			return false;
		}

		/// [EN] While the library cannot be reached, everything shared is treated as protected rather than as free.
		/// [JP] ライブラリへ届いていない間は、共有されているものを「自由」ではなく「保護されている」として扱う。
		if (!snapshot_.online_)
		{
			return true;
		}

		String logical = Logical(path);
		if (logical.str().empty())
		{
			return false;
		}

		for (const SharedAsset& asset : snapshot_.assets_)
		{
			if (asset.deleted_)
			{
				continue;
			}
			for (const SharedFile& file : asset.files_)
			{
				/// [EN] A folder is protected when anything shared lives inside it, so deleting a folder cannot take shared content with it.
				/// [JP] 共有されたものを含むフォルダも保護する。フォルダごと削除して共有物を巻き込むことを防ぐため。
				if (file.path_ == logical || file.path_.str().rfind(logical.str() + "/", 0) == 0)
				{
					return true;
				}
			}
		}
		return false;
	}

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
	Bool ResourceSync::Editable(Uint32 assetId, const String& scope)const
	{
		/// [EN] An asset the team does not have is nobody else's business, so it is freely editable.
		/// [JP] チームが持っていないアセットは他の誰にも関係しないので、自由に編集してよい。
		const SharedAsset* asset = GetAsset(assetId);
		if (!configured_ || !asset)
		{
			return true;
		}

		/// [EN] Holding the lease is the only thing that grants the right; an unheld scope stays read-only until asked for.
		/// [JP] 編集権を与えるのは Lease を持っていることだけ。誰も持っていない範囲も、要求するまでは読み取り専用のまま。
		for (const EditLease& lease : snapshot_.leases_)
		{
			if (lease.assetId_ == asset->id_ && lease.scope_ == scope)
			{
				return lease.mine_;
			}
		}
		return false;
	}

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
	void ResourceSync::RequestEdit(Uint32 assetId, const String& scope)
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!worker_ || !asset)
		{
			return;
		}

		/// [EN] A member dragging a gizmo asks on every frame, and each ask is a write to the shared table.
		/// [JP] ギズモを掴んでいるメンバーは毎フレーム要求し、その1回ごとが共有表への書き込みになる。
		if (GetTickCount64() < nextEditRequest_)
		{
			return;
		}
		nextEditRequest_ = GetTickCount64() + 1500;

		SharingRequest request;
		request.action_ = SharingAction::Checkout;
		request.assetId_ = asset->id_;
		request.scope_ = scope;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestGet(Uint32 assetId)
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!worker_ || !asset)
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Get;
		request.assetId_ = asset->id_;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestPublish(Uint32 assetId)
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!worker_ || !asset)
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Publish;
		request.assetId_ = asset->id_;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestRelease(Uint32 assetId, const String& scope)
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!worker_ || !asset)
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Release;
		request.assetId_ = asset->id_;
		request.scope_ = scope;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestRegister(const AssetRecord& asset)
	{
		/// [EN] Only content inside the workspace can be shared, so anything else is refused here rather than half-published.
		/// [JP] 共有できるのはワークスペース内のものだけ。それ以外は中途半端に公開せず、ここで断る。
		String logical = Logical(std::filesystem::path(asset.fullpath_.str()));
		if (!worker_ || logical.str().empty())
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Register;
		request.path_ = logical;
		request.runtimeId_ = asset.assetID_;
		request.type_ = static_cast<Int32>(asset.type_);
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestRetire(Uint32 assetId)
	{
		const SharedAsset* asset = GetAsset(assetId);
		if (!worker_ || !asset)
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Retire;
		request.assetId_ = asset->id_;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestRefresh()
	{
		if (!worker_)
		{
			return;
		}

		SharingRequest request;
		request.action_ = SharingAction::Refresh;
		worker_->Enqueue(request);
	}

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
	void ResourceSync::RequestOpenScene(const std::filesystem::path& path)
	{
		if (!worker_)
		{
			return;
		}

		/// [EN] A scene the team does not share is sent as an empty identifier, which is how following is turned off again.
		/// [JP] チームで共有していない Scene は空の識別子として送る。これが、追いかけるのをやめる合図になる。
		const SharedAsset* asset = GetAsset(path);
		SharingRequest request;
		request.action_ = SharingAction::OpenScene;
		request.assetId_ = asset ? asset->id_ : String();
		worker_->Enqueue(request);
	}

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
	const SharedAsset* ResourceSync::GetAsset(Uint32 assetId)const
	{
		/// [EN] The Editor knows assets by the engine's 32-bit identifier, so that is what the lookup takes.
		/// [JP] Editor はアセットをエンジンの32ビット識別子で知っているため、引き当てにもそれを使う。
		for (const SharedAsset& asset : snapshot_.assets_)
		{
			if (asset.runtimeId_ == assetId)
			{
				return &asset;
			}
		}
		return nullptr;
	}

	/**
	* [EN]
	* The library's view of the asset that sits at the given path, or
	* nullptr when the team does not have it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その位置にあるアセットについての、ライブラリから見た情報。チームが
	* 持っていなければ nullptr。
	*/
	const SharedAsset* ResourceSync::GetAsset(const std::filesystem::path& path)const
	{
		/// [EN] A scene is known to the Editor by where it sits rather than by an identifier, so this lookup exists for it.
		/// [JP] Scene は識別子ではなく位置で Editor に知られているため、そのための引き当てとしてこれがある。
		String logical = Logical(path);
		if (logical.str().empty())
		{
			return nullptr;
		}

		for (const SharedAsset& asset : snapshot_.assets_)
		{
			if (asset.path_ == logical)
			{
				return &asset;
			}
		}
		return nullptr;
	}

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
	const DynamicArray<EditLease>& ResourceSync::GetLeases()const
	{
		return snapshot_.leases_;
	}

	/**
	* [EN]
	* What the library is doing right now, or empty when it is idle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ライブラリが今行っていること。何もしていなければ空。
	*/
	const String& ResourceSync::Operation()const
	{
		return snapshot_.operation_;
	}

	/**
	* [EN]
	* The last failure, in a form that can be shown to the user.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の失敗内容。ユーザーへそのまま表示できる形で返す。
	*/
	const String& ResourceSync::Error()const
	{
		return snapshot_.error_;
	}

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
	String ResourceSync::Logical(const std::filesystem::path& path)const
	{
		/// [EN] Both sides are normalised first, so a path written with ".." or mixed separators still matches.
		/// [JP] 先に両方を正規化する。".." や区切り記号の混在があっても一致するようにするため。
		std::filesystem::path workspace = (projectRoot_ / config_.workspace_.str()).lexically_normal();
		std::filesystem::path relative = path.lexically_normal().lexically_relative(workspace);

		/// [EN] A relative path that climbs out of the workspace begins with "..", which is how outside paths are recognised.
		/// [JP] ワークスペースの外へ出る相対パスは ".." で始まる。外にあるものはそれで判別する。
		if (relative.empty() || *relative.begin() == "..")
		{
			return String();
		}
		return String(relative.generic_string());
	}
}
