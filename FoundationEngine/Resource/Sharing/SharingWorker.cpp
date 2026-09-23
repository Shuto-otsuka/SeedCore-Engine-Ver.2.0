#include <FoundationEngine/Resource/Sharing/SharingWorker.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>
#include <FoundationEngine/Resource/Sharing/SharedScene.h>

namespace SeedCore
{
	/**
	* [EN]
	* Prepares the worker for a project, without connecting yet.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロジェクト用にワーカーを用意する。この時点ではまだ接続しない。
	*/
	SharingWorker::SharingWorker(const std::filesystem::path& projectRoot, const SharingConfig& config) : projectRoot_(projectRoot), config_(config), auth_(http_, projectRoot / ".asset" / "credentials", config.clientId_, config.clientSecret_), document_(http_, auth_), drive_(http_, auth_), catalog_(document_, config.catalogDocumentId_), locks_(document_, config.lockDocumentId_, config.owner_, String(std::format("{:08x}{:08x}", std::random_device()(), std::random_device()())))
	{
		/// [EN] The session identity is made here and kept for the whole run, which is how this Editor's own leases are recognised.
		/// [JP] セッションの身元はここで作り、起動中ずっと保つ。これによって、この Editor 自身の Lease を見分けられる。
		LoadWorkspace();
	}

	/**
	* [EN]
	* Stops the thread and releases every lease this Editor holds.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スレッドを止め、この Editor が持つ全ての Lease を解放する。
	*/
	SharingWorker::~SharingWorker()
	{
		/// [EN] Stopping here as well as on request means an Editor torn down without warning still frees what it held.
		/// [JP] 要求時だけでなくここでも止めることで、予告なく破棄された Editor でも保持分を解放できる。
		Stop();
	}

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
	void SharingWorker::Start()
	{
		if (running_)
		{
			return;
		}

		/// [EN] The flag is set before the thread begins, so the loop inside it never sees itself as already stopped.
		/// [JP] スレッド開始より先に印を立てる。中のループが、自分を停止済みと見てしまわないようにするため。
		running_ = true;
		thread_ = std::thread(&SharingWorker::Run, this);
	}

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
	void SharingWorker::Stop()
	{
		if (!running_)
		{
			return;
		}

		/// [EN] Clearing the flag lets the loop finish what it is doing and then release on its way out.
		/// [JP] 印を下ろすと、ループは今の作業を終えてから、抜ける際に解放を行う。
		running_ = false;
		if (thread_.joinable())
		{
			thread_.join();
		}
	}

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
	void SharingWorker::Enqueue(const SharingRequest& request)
	{
		/// [EN] The lock is held only long enough to add, so the Editor thread is never made to wait on the network.
		/// [JP] 錠を持つのは追加の間だけ。Editor のスレッドが通信を待たされることはない。
		std::lock_guard<std::mutex> guard(requestMutex_);
		requests_.push(request);
	}

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
	SharingSnapshot SharingWorker::Snapshot()const
	{
		/// [EN] A copy is returned rather than a reference, so the worker may rewrite its own state a moment later.
		/// [JP] 参照ではなく写しを返す。ワーカーが直後に自分の状態を書き換えても構わないようにするため。
		std::lock_guard<std::mutex> guard(snapshotMutex_);
		return snapshot_;
	}

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
	void SharingWorker::ConsumeChanged(DynamicArray<Uint32>& assets)
	{
		/// [EN] Handing over and clearing happen together, so an asset is never reloaded twice or missed entirely.
		/// [JP] 引き渡しと消去を一緒に行う。同じアセットを2回読み直したり、取りこぼしたりしないようにするため。
		std::lock_guard<std::mutex> guard(snapshotMutex_);
		for (Uint32 assetId : changed_)
		{
			assets.push_back(assetId);
		}
		changed_.clear();
	}

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
	void SharingWorker::ConsumeImported(DynamicArray<String>& paths)
	{
		std::lock_guard<std::mutex> guard(snapshotMutex_);
		for (const String& path : imported_)
		{
			paths.push_back(path);
		}
		imported_.clear();
	}

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
	void SharingWorker::Run()
	{
		/// [EN] A member who has signed in before goes straight through; a new one gets the browser opened for them here.
		/// [JP] 以前ログインしたメンバーはそのまま通り、初めてのメンバーはここでブラウザが開かれる。
		if (!auth_.SignedIn() && !auth_.SignIn())
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			snapshot_.error_ = auth_.Error();
			return;
		}

		/// [EN] Timestamps rather than counters, so a slow request does not push everything after it out of step.
		/// [JP] 回数ではなく時刻で管理する。遅いリクエストが1つあっても、以降の間隔が崩れないようにするため。
		Double nextRefresh = 0.0;
		Double nextRenew = 0.0;
		Double nextMirror = 0.0;
		while (running_)
		{
			Double now = std::chrono::duration<Double>(std::chrono::steady_clock::now().time_since_epoch()).count();

			/// [EN] Requests come first, because a member waiting to edit should not sit behind a scheduled read.
			/// [JP] 要求を先に処理する。編集を待っているメンバーを、定期的な読み取りの後ろに並ばせないため。
			SharingRequest request;
			Bool hasRequest = false;
			{
				std::lock_guard<std::mutex> guard(requestMutex_);
				if (!requests_.empty())
				{
					request = requests_.front();
					requests_.pop();
					hasRequest = true;
				}
			}
			if (hasRequest)
			{
				Process(request);
				continue;
			}

			/// [EN] Renewing comes before refreshing, since losing a lease costs more than seeing the catalog a second late.
			/// [JP] 更新を読み直しより先に行う。Lease を失う方が、カタログが1秒古いことより痛いため。
			if (now >= nextRenew)
			{
				nextRenew = now + 40.0;
				locks_.Renew();
			}

			/// [EN] Re-reading on a timer is how another member's lock and another member's publish become visible.
			/// [JP] 定期的な読み直しが、他のメンバーの Lock や Publish が見えるようになる仕組み。
			if (now >= nextRefresh)
			{
				nextRefresh = now + 5.0;
				Bool read = catalog_.Refresh() && locks_.Refresh();
				{
					std::lock_guard<std::mutex> guard(snapshotMutex_);
					snapshot_.online_ = read;
					snapshot_.error_ = read ? String() : catalog_.Error();
				}

				/// [EN] What another member published is pulled down on its own, so nobody has to notice an arrow and press a button.
				/// [JP] 他のメンバーが公開したものは自動で降りてくる。矢印に気づいてボタンを押す必要が無いようにするため。
				if (read)
				{
					for (const SharedAsset& asset : catalog_.Assets())
					{
						/// [EN] Only assets this machine already has are followed; one it has never fetched stays a deliberate choice.
						/// [JP] 追いかけるのは既に持っているアセットだけ。一度も取得していないものを取るかどうかは、あくまで本人の判断。
						auto record = workspace_.find(asset.id_);
						if (asset.deleted_ || record == workspace_.end())
						{
							continue;
						}

						/// [EN] Holding a lease means this member is in the middle of editing it, and replacing the files under them would be wrong.
						/// [JP] Lease を持っているのは、このメンバーが編集の最中ということ。その足元でファイルを入れ替えるのは誤り。
						if (locks_.Held(asset.id_, String("asset")))
						{
							continue;
						}

						/// [EN] Size and time are compared before anything is read, so an untouched asset costs no disk work here.
						/// [JP] 何かを読む前にサイズと時刻を比べる。無変更のアセットに、ここでのディスク作業が発生しないようにするため。
						Bool touched = false;
						Bool missing = false;
						for (const WorkspaceFile& file : record->second.files_)
						{
							std::error_code errorCode;
							std::filesystem::path local = Local(file.path_);

							/// [EN] A file that is gone is not a local edit but a hole, and a hole is worth filling from the library.
							/// [JP] 消えているファイルはローカルの変更ではなく欠けであり、欠けはライブラリから埋める価値がある。
							if (!std::filesystem::exists(local))
							{
								missing = true;
								continue;
							}

							Uint64 size = std::filesystem::file_size(local, errorCode);
							Int64 stamp = std::filesystem::last_write_time(local, errorCode).time_since_epoch().count();
							touched = touched || errorCode || size != file.size_ || stamp != file.stamp_;
						}

						/// [EN] Anything that looks touched is left for the member to resolve, and Get would refuse it anyway.
						/// [JP] 触られたように見えるものはメンバーの判断に任せる。どのみち Get 側が断ることになる。
						if (touched)
						{
							continue;
						}

						/// [EN] So an asset comes down when the library moved ahead, and also when this workspace lost the files it recorded.
						/// [JP] つまり降りてくるのは、ライブラリが先へ進んだときと、このワークスペースが記録済みのファイルを失ったとき。
						if (record->second.revision_ < asset.revision_ || missing)
						{
							Get(asset.id_);
						}
					}

					/// [EN] The open scene is followed on the same timer, which is what makes another member's entity appear on its own.
					/// [JP] 開いている Scene も同じ間隔で追いかける。他のメンバーの Entity が自然に現れるのは、この確認による。

					/// [EN] Its own check finds nothing changed most of the time, so this costs one document read per interval.
					/// [JP] ほとんどの回は「変化なし」で終わるため、費用は間隔ごとのドキュメント1回の読み取りで済む。
					if (!openScene_.str().empty())
					{
						Get(openScene_);
					}

					/// [EN] The library's Assets tree is kept shaped like this workspace, and anything dropped into it is taken in.
					/// [JP] ライブラリの Assets の構成をこのワークスペースと同じ形に保ち、そこへ置かれたものを取り込む。

					/// [EN] It runs less often than the rest, since walking the whole tree costs one listing per folder.
					/// [JP] 他より間隔を空けて動かす。ツリー全体を辿るには、フォルダごとに一覧を1回ずつ取ることになるため。
					if (!config_.assetsFolderId_.str().empty() && now >= nextMirror)
					{
						nextMirror = now + 30.0;
						Mirror(projectRoot_ / config_.workspace_.str() / "Assets", config_.assetsFolderId_);
					}
				}
				Capture();
			}

			/// [EN] Sleeping briefly keeps the thread from spinning while there is nothing to do.
			/// [JP] 何もない間、スレッドが空回りしないよう短く眠る。
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		/// [EN] On the way out every lease is handed back, so another member can edit at once rather than after two minutes.
		/// [JP] 終了時に全ての Lease を返す。他のメンバーが2分待たずにすぐ編集できるようにするため。
		for (const EditLease& lease : snapshot_.leases_)
		{
			locks_.Release(lease.assetId_, lease.scope_);
		}
		SaveWorkspace();
	}

	/**
	* [EN]
	* Carries out one request and records what happened.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 要求を1つ実行し、その結果を記録する。
	*/
	void SharingWorker::Process(const SharingRequest& request)
	{
		/// [EN] What is being done is shown while it runs, because an upload can take a while and silence looks like a hang.
		/// [JP] 実行中はその内容を表示する。アップロードには時間がかかることがあり、無言だと固まって見えるため。
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			switch (request.action_)
			{
			case SharingAction::Refresh:
				snapshot_.operation_ = String("共有ライブラリを確認中");
				break;

			case SharingAction::Checkout:
				snapshot_.operation_ = String("編集権を取得中");
				break;

			case SharingAction::Release:
				snapshot_.operation_ = String("編集権を解放中");
				break;

			case SharingAction::Get:
				snapshot_.operation_ = String("取得中");
				break;

			case SharingAction::Register:
				snapshot_.operation_ = String("共有ライブラリへ登録中");
				break;

			case SharingAction::Publish:
				snapshot_.operation_ = String("公開中");
				break;

			case SharingAction::Retire:
				snapshot_.operation_ = String("共有を終了中");
				break;
			}
		}

		Bool done = false;
		switch (request.action_)
		{
		case SharingAction::Refresh:
			done = catalog_.Refresh() && locks_.Refresh();
			break;

		case SharingAction::Checkout:
			/// [EN] Taking a lease is what the Editor does the moment a member starts changing a shared asset.
			/// [JP] Lease の取得は、メンバーが共有アセットを変更し始めた瞬間に Editor が行うこと。
			done = locks_.Acquire(request.assetId_, request.scope_);
			break;

		case SharingAction::Release:
			done = locks_.Release(request.assetId_, request.scope_);
			break;

		case SharingAction::Get:
			done = Get(request.assetId_);
			break;

		case SharingAction::Register:
			done = Register(request);
			break;

		case SharingAction::Publish:
			done = Publish(request.assetId_);
			break;

		case SharingAction::Retire:
			/// [EN] Retiring is checked against the same revision a publish would be, so nobody removes unseen work.
			/// [JP] 廃止も Publish と同じ Revision で照合する。見ていない成果を誰かが消してしまわないようにするため。
			done = catalog_.Retire(request.assetId_, workspace_[request.assetId_].revision_);
			break;

		case SharingAction::OpenScene:
			/// [EN] Only the scene in front of the member is followed, since reading one document per scene would cost a read for scenes nobody has open.
			/// [JP] 追いかけるのはメンバーが見ている Scene だけ。Scene ごとにドキュメントを読むと、誰も開いていない Scene の分まで読むことになるため。
			openScene_ = request.assetId_;
			sceneRevisions_.clear();
			done = openScene_.str().empty() || Get(openScene_);
			break;
		}

		/// [EN] Each failure already carries its own wording, so the one that happened is simply passed along.
		/// [JP] 失敗にはそれぞれ固有の文言が付いているので、起きたものをそのまま渡す。
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			snapshot_.operation_ = String();
			snapshot_.error_ = done ? String() : (catalog_.Error().str().empty() ? locks_.Error() : catalog_.Error());
		}
		Capture();
	}

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
	Bool SharingWorker::Get(const String& assetId)
	{
		const SharedAsset* asset = catalog_.Find(assetId);
		if (!asset)
		{
			return false;
		}
		/// [EN] A scene is not fetched as a file but rebuilt from its pieces, so that one member's entity does not arrive as a whole scene.
		/// [JP] Scene はファイルとしてではなく断片から組み立て直す。1人の Entity の変更が、Scene 丸ごととして届かないようにするため。
		if (asset->scene_ && !asset->sceneDocumentId_.str().empty())
		{
			SharedScene shared(document_, asset->sceneDocumentId_);
			if (!shared.Refresh())
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = shared.Error();
				return false;
			}

			/// [EN] This runs on a timer while a scene is open, so a scene nobody has changed must cost nothing beyond the read.
			/// [JP] これは Scene を開いている間、定期的に動く。誰も変更していない Scene で、読み取り以上の負担が出ないようにする。
			Bool moved = shared.Parts().size() != sceneRevisions_.size();
			for (const ScenePart& part : shared.Parts())
			{
				auto known = sceneRevisions_.find(part.scope_);
				moved = moved || known == sceneRevisions_.end() || known->second != part.revision_;
			}
			if (!moved)
			{
				return true;
			}

			std::unordered_map<String, nlohmann::json> parts;
			for (const ScenePart& part : shared.Parts())
			{
				/// [EN] A removed entity is skipped rather than rebuilt, which is how a deletion by someone else takes effect here.
				/// [JP] 削除された Entity は組み立てず飛ばす。他の誰かによる削除が、ここで反映される仕組み。
				if (part.deleted_)
				{
					continue;
				}
	
				/// [EN] A piece that came with the document needs nothing further; one kept in Drive is fetched now.
				/// [JP] ドキュメントと一緒に届いた断片はこれ以上何も要らない。Drive に置かれた断片だけ、ここで取りに行く。
				if (!part.data_.str().empty())
				{
					parts[part.scope_] = nlohmann::json::parse(part.data_.str(), nullptr, false);
					continue;
				}
	
				std::filesystem::path staging = projectRoot_ / ".asset" / "incoming" / part.hash_.str();
				if (!drive_.Fetch(part.driveId_, staging))
				{
					std::lock_guard<std::mutex> guard(snapshotMutex_);
					snapshot_.error_ = drive_.Error();
					return false;
				}
				std::ifstream stream(staging, std::ios::binary);
				parts[part.scope_] = nlohmann::json::parse(stream, nullptr, false);
				stream.close();
	
				std::error_code errorCode;
				std::filesystem::remove(staging, errorCode);
			}
	
			/// [EN] The pieces only become a scene once the hierarchy has put them in order, which is where a missing one shows up.
			/// [JP] 断片が Scene になるのは、階層が順序を与えた後。足りない断片があれば、そこで判明する。
			auto context = parts.find(String("context"));
			auto structure = parts.find(String("structure"));
			if (context == parts.end() || structure == parts.end() || !structure->second.is_array())
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = String("Scene の断片が揃っていないため組み立てられませんでした。");
				return false;
			}
	
			/// [EN] The scene starts as everything that was not an actor, and the actors are laid back into it in hierarchy order.
			/// [JP] Scene は Actor 以外の全てから始め、そこへ Actor を階層の順で並べ直す。
			nlohmann::json scene = context->second;
			scene["nodes"] = nlohmann::json::array();
	
			/// [EN] Where each actor ended up is remembered, since a child states its parent by identifier but the file wants a position.
			/// [JP] 各 Actor がどこへ入ったかを覚えておく。子は親を識別子で示すが、ファイル側が求めるのは位置であるため。
			std::unordered_map<std::string, Size> placed;
			for (const nlohmann::json& link : structure->second)
			{
				std::string identifier = link.value("id", "");
				auto entity = parts.find(String(std::format("entity:{}", identifier)));
				if (identifier.empty() || entity == parts.end() || placed.contains(identifier))
				{
					std::lock_guard<std::mutex> guard(snapshotMutex_);
					snapshot_.error_ = String("Scene の階層と断片が食い違っているため組み立てられませんでした。");
					return false;
				}
	
				/// [EN] A parent that has not been placed yet means the hierarchy is out of order or points at something that is gone.
				/// [JP] まだ置かれていない親を指しているのは、階層の順序が崩れているか、既に無いものを指しているということ。
				nlohmann::json node = entity->second;
				node["parentIndex"] = -1;
				if (!link["parent"].is_null())
				{
					auto parent = placed.find(link["parent"].get<std::string>());
					if (parent == placed.end())
					{
						std::lock_guard<std::mutex> guard(snapshotMutex_);
						snapshot_.error_ = String("Scene の親子関係を復元できませんでした。");
						return false;
					}
					node["parentIndex"] = static_cast<Int32>(parent->second);
				}
	
				placed[identifier] = scene["nodes"].size();
				scene["nodes"].push_back(node);
			}
	
			/// [EN] The file is written beside itself and moved into place, so the Editor never reads a half-written scene.
			/// [JP] ファイルは隣に書いてから移す。Editor が書きかけの Scene を読むことのないようにするため。
			std::filesystem::path local = Local(asset->path_);
			std::filesystem::path staging = local;
			staging += ".download";
			std::error_code errorCode;
			std::filesystem::create_directories(local.parent_path(), errorCode);
			std::ofstream writer(staging);
			writer << scene.dump(1, '\t');
			writer.close();
			std::filesystem::rename(staging, local, errorCode);
			if (errorCode)
			{
				std::filesystem::remove(staging, errorCode);
				return false;
			}
	
			/// [EN] What was written is now what this machine has, and the Editor is told so it can pick the scene up.
			/// [JP] 書き出した内容が、この PC の持っている状態になる。Editor にも知らせて、Scene を拾えるようにする。
			sceneRevisions_.clear();
			for (const ScenePart& part : shared.Parts())
			{
				sceneRevisions_[part.scope_] = part.revision_;
			}
	
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			changed_.push_back(asset->runtimeId_);
			return true;
		}


		/// [EN] Work this machine has not published would be overwritten by the incoming files, so it stops the get instead.
		/// [JP] この PC の未公開の作業は、届くファイルで上書きされてしまうため、取得の方を止める。
		auto previous = workspace_.find(assetId);
		if (previous != workspace_.end())
		{
			for (const WorkspaceFile& file : previous->second.files_)
			{
				/// [EN] Size and time are read first, and a file that still matches both is taken as untouched without being read.
				/// [JP] 先にサイズと時刻を見る。両方とも一致するファイルは、中身を読まずに無変更とみなす。
				std::error_code errorCode;
				std::filesystem::path local = Local(file.path_);
				Uint64 size = std::filesystem::file_size(local, errorCode);
				Int64 stamp = std::filesystem::last_write_time(local, errorCode).time_since_epoch().count();
				if (!errorCode && size == file.size_ && stamp == file.stamp_)
				{
					continue;
				}

				/// [EN] A file that is gone holds no work to protect, so it is fetched again rather than treated as an edit.
				/// [JP] 消えているファイルには守るべき作業が無いため、変更とみなさず取得し直す。
				if (!std::filesystem::exists(local))
				{
					continue;
				}

				/// [EN] Otherwise the contents decide, since re-saving a file changes its time without changing what is in it.
				/// [JP] そうでなければ中身で判断する。保存し直しただけでも時刻は変わるが、中身は変わらないため。
				DynamicArray<Byte> digest = Sha256::Hash(local);
				std::string current;
				for (Byte value : digest)
				{
					current += std::format("{:02x}", static_cast<Uint8>(value));
				}

				/// [EN] The comparison is against what the file hashed to when it arrived, not against the library's newest.
				/// [JP] 比べる相手は、そのファイルが届いた時点のハッシュであって、ライブラリの最新ではない。
				if (current != file.hash_.str())
				{
					std::lock_guard<std::mutex> guard(snapshotMutex_);
					snapshot_.error_ = String(std::format("\"{}\" にローカルの変更があるため取得しませんでした。", asset->path_.str()));
					return false;
				}
			}
		}

		/// [EN] Dependencies come first, so a scene is never opened before the models and materials it refers to exist.
		/// [JP] 依存を先に取得する。Scene が、参照するモデルやマテリアルより先に開かれることのないようにするため。
		for (const String& dependency : asset->dependencies_)
		{
			Get(dependency);
		}

		for (const SharedFile& file : asset->files_)
		{
			std::filesystem::path local = Local(file.path_);

			/// [EN] A file whose contents already hash to the same value is skipped, which is what makes a repeated get cheap.
			/// [JP] 中身のハッシュが既に一致するファイルは飛ばす。取得を繰り返しても軽いのはこのため。
			DynamicArray<Byte> digest = Sha256::Hash(local);
			std::string current;
			for (Byte value : digest)
			{
				current += std::format("{:02x}", static_cast<Uint8>(value));
			}
			if (current == file.hash_.str())
			{
				continue;
			}

			/// [EN] The download lands beside the target first, so a half-finished transfer never replaces a good file.
			/// [JP] ダウンロードはまず目的地の隣へ行う。途中で終わった転送が、正常なファイルを置き換えないようにするため。
			std::filesystem::path staging = local;
			staging += ".download";
			if (!drive_.Fetch(file.driveId_, staging))
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = drive_.Error();
				return false;
			}

			/// [EN] Replacing in one step means the Editor either sees the old file or the new one, never something in between.
			/// [JP] 1度の操作で置き換えるため、Editor から見えるのは古いファイルか新しいファイルのどちらかで、途中の状態は無い。
			std::error_code errorCode;
			std::filesystem::rename(staging, local, errorCode);
			if (errorCode)
			{
				std::filesystem::remove(staging, errorCode);
				return false;
			}

			/// [EN] The Editor may already hold this asset in memory, so the replacement is announced for it to reload.
			/// [JP] Editor が既にこのアセットをメモリに持っていることがあるため、差し替えたことを知らせて読み直させる。
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			changed_.push_back(asset->runtimeId_);
		}

		/// [EN] Recording the revision is what later lets a publish say which version this work was built on.
		/// [JP] Revision を記録しておくことが、後の Publish で「どの版を元にした作業か」を言えることにつながる。

		/// [EN] The hashes are recorded with it, so a later edit by this member can be told from an untouched copy.
		/// [JP] ハッシュも一緒に記録する。後でこのメンバーが編集した場合に、無変更の写しと区別できるようにするため。
		WorkspaceRecord record;
		record.revision_ = asset->revision_;
		for (const SharedFile& file : asset->files_)
		{
			/// [EN] The time is read after the file has landed, so it is the one a later check will find on disk.
			/// [JP] 時刻はファイルが置かれた後に読む。後の確認がディスク上で見るのと同じ値にするため。
			std::error_code errorCode;
			record.files_.push_back(WorkspaceFile{ file.path_, file.hash_, file.size_, std::filesystem::last_write_time(Local(file.path_), errorCode).time_since_epoch().count() });
		}
		workspace_[assetId] = record;
		SaveWorkspace();
		return true;
	}

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
	Bool SharingWorker::Publish(const String& assetId)
	{
		const SharedAsset* asset = catalog_.Find(assetId);
		if (!asset)
		{
			return false;
		}
		/// [EN] A scene is published one entity at a time, so what this member touched travels without the rest of the scene.
		/// [JP] Scene は Entity ごとに Publish する。このメンバーが触った分だけが、Scene の残りを巻き込まずに運ばれるようにするため。
		if (asset->scene_ && !asset->sceneDocumentId_.str().empty())
		{
			/// [EN] The scene on disk is read as the plain JSON it is, and taken apart the same way every member takes it apart.
			/// [JP] ディスク上の Scene を、そのままの JSON として読む。分け方はどのメンバーでも同じ手順になる。
			std::ifstream stream(Local(asset->path_));
			nlohmann::json scene = nlohmann::json::parse(stream, nullptr, false);
			if (!scene.is_object() || !scene.contains("nodes") || !scene["nodes"].is_array())
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = String("Scene の形が想定と違うため分割できませんでした。");
				return false;
			}
	
			/// [EN] Everything that is not the actor list travels together, since it describes the scene rather than anyone's actor.
			/// [JP] Actor の一覧以外はまとめて1つに運ぶ。それらは誰かの Actor ではなく、Scene 自体を記述するものであるため。
			std::unordered_map<String, nlohmann::json> parts;
			nlohmann::json context = nlohmann::json::object();
			for (auto& entry : scene.items())
			{
				if (entry.key() != "nodes")
				{
					context[entry.key()] = entry.value();
				}
			}
			parts[String("context")] = context;
	
			/// [EN] The hierarchy is kept as its own piece, so moving an actor is one change rather than a rewrite of two actors.
			/// [JP] 階層は独立した断片として持つ。Actor の移動が、2つの Actor の書き換えではなく1つの変更で済むようにするため。
			nlohmann::json structure = nlohmann::json::array();
			for (Size index = 0; index < scene["nodes"].size(); ++index)
			{
				const nlohmann::json& node = scene["nodes"][index];
	
				/// [EN] The identifier is what a lease and a revision hang on, so an actor without one cannot be shared at all.
				/// [JP] Lease も Revision もこの識別子にぶら下がるため、持たない Actor は共有できない。
				std::string identifier = node.value("collaborationId", "");
				if (identifier.empty() || parts.contains(String(std::format("entity:{}", identifier))))
				{
					std::lock_guard<std::mutex> guard(snapshotMutex_);
					snapshot_.error_ = String("共同編集用のIDを持たない、あるいは重複している Actor があります。");
					return false;
				}
	
				/// [EN] The hierarchy names parents by identifier rather than by position, so it survives actors being added or removed.
				/// [JP] 階層は親を位置ではなく識別子で指す。Actor が増減しても壊れないようにするため。
				Int32 parent = node.value("parentIndex", Int32(-1));
				nlohmann::json link;
				link["id"] = identifier;
				link["parent"] = parent >= 0 && parent < static_cast<Int32>(index) ? nlohmann::json(scene["nodes"][static_cast<Size>(parent)].value("collaborationId", "")) : nlohmann::json(nullptr);
				structure.push_back(link);
	
				/// [EN] The position is dropped from the actor's own piece, because it belongs to the hierarchy and would collide there.
				/// [JP] 位置は Actor 自身の断片から外す。それは階層に属する情報で、そこへ残すと衝突の元になるため。
				nlohmann::json entity = node;
				entity.erase("parentIndex");
				parts[String(std::format("entity:{}", identifier))] = entity;
			}
			parts[String("structure")] = structure;
	
			/// [EN] The library's view of the same scene says which pieces have moved on since this member last saw them.
			/// [JP] 同じ Scene についてのライブラリ側の見え方が、このメンバーが最後に見てから進んだ断片を教えてくれる。
			SharedScene shared(document_, asset->sceneDocumentId_);
			if (!shared.Refresh())
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = shared.Error();
				return false;
			}
	
			DynamicArray<ScenePartChange> changes;
			for (const std::pair<const String, nlohmann::json>& part : parts)
			{
				/// [EN] The hash is taken over the same text that will be stored, so an unchanged piece is recognised as unchanged.
				/// [JP] ハッシュは保存する本文そのものから取る。変わっていない断片が、変わっていないと判定されるようにするため。
				std::string text = part.second.dump();
				DynamicArray<Byte> digest = Sha256::Hash(reinterpret_cast<const Byte*>(text.data()), text.size());
				std::string hash;
				for (Byte value : digest)
				{
					hash += std::format("{:02x}", static_cast<Uint8>(value));
				}
	
				const ScenePart* remote = shared.Find(part.first);
				if (remote && remote->hash_ == String(hash))
				{
					continue;
				}
	
				/// [EN] Only what this member holds the right to change is sent; the rest stays as whoever owns it left it.
				/// [JP] 送るのは、このメンバーが変更する権利を持っている分だけ。残りは、持ち主が残したままにする。
				if (!locks_.Held(assetId, part.first))
				{
					continue;
				}
	
				ScenePartChange change;
				change.scope_ = part.first;
				change.baseRevision_ = remote ? remote->revision_ : 0;
				change.hash_ = String(hash);
				change.size_ = text.size();
	
				/// [EN] A piece small enough travels inside the document, which is what lets it arrive without a second round trip.
				/// [JP] 十分小さい断片はドキュメントの中を運ばれる。2回目の往復なしで届くのはこのため。
				if (text.size() <= 64 * 1024)
				{
					change.data_ = String(text);
				}
				else
				{
					/// [EN] A piece too large for that is stored like any other content, under the hash of what it holds.
					/// [JP] 大きすぎる断片は、他の中身と同じように、その内容のハッシュを名前にして保存する。
					std::filesystem::path staging = projectRoot_ / ".asset" / "outgoing" / hash;
					std::error_code errorCode;
					std::filesystem::create_directories(staging.parent_path(), errorCode);
					std::ofstream writer(staging, std::ios::binary);
					writer << text;
					writer.close();
	
					change.driveId_ = drive_.Find(change.hash_, config_.blobFolderId_);
					if (change.driveId_.str().empty())
					{
						change.driveId_ = drive_.Upload(staging, change.hash_, config_.blobFolderId_);
					}
					std::filesystem::remove(staging, errorCode);
					if (change.driveId_.str().empty())
					{
						std::lock_guard<std::mutex> guard(snapshotMutex_);
						snapshot_.error_ = drive_.Error();
						return false;
					}
				}
				changes.push_back(change);
			}
	
			/// [EN] An entity the library still lists but this scene no longer has was deleted here, which is a structure change.
			/// [JP] ライブラリにはあるがこの Scene に無い Entity は、ここで削除されたということ。これは構造の変更にあたる。
			for (const ScenePart& remote : shared.Parts())
			{
				if (remote.deleted_ || parts.contains(remote.scope_) || !locks_.Held(assetId, String("structure")))
				{
					continue;
				}
	
				ScenePartChange change;
				change.scope_ = remote.scope_;
				change.baseRevision_ = remote.revision_;
				change.deleted_ = true;
				changes.push_back(change);
			}
	
			if (!shared.Publish(changes))
			{
				std::lock_guard<std::mutex> guard(snapshotMutex_);
				snapshot_.error_ = shared.Error();
				return false;
			}
	
			/// [EN] The pieces just sent are what this machine now has, so the next check does not see them as newly arrived.
			/// [JP] 今送った断片が、この PC の持っている状態になる。次の確認で「新しく届いた」と見えないようにするため。
			for (const ScenePartChange& change : changes)
			{
				sceneRevisions_[change.scope_] = change.baseRevision_ + 1;
			}
	
			/// [EN] Holding on after publishing would keep everyone else out, so each piece is handed back as it lands.
			/// [JP] 公開後も持ち続けると他の全員を締め出すことになるため、通った断片から順に返す。
			for (const ScenePartChange& change : changes)
			{
				locks_.Release(assetId, change.scope_);
			}
			return true;
		}


		/// [EN] Publishing without holding the lease would be exactly the overwrite the whole design exists to prevent.
		/// [JP] Lease を持たずに Publish することは、この設計全体が防ごうとしている上書きそのものになる。
		if (!locks_.Held(assetId, String("asset")))
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			snapshot_.error_ = String("編集権を取得してから公開してください。");
			return false;
		}

		/// [EN] The file list is taken from the catalog, so a publish describes the same set of files the team already knows.
		/// [JP] ファイル一覧はカタログから取る。Publish が、チームが既に知っているのと同じ構成を記述するようにするため。
		DynamicArray<SharedFile> files;
		for (const SharedFile& file : asset->files_)
		{
			SharedFile stored;
			if (!Store(file.path_, stored))
			{
				return false;
			}
			files.push_back(stored);
		}

		/// [EN] The revision recorded at get time is what the catalog checks, and a mismatch is reported as someone else being ahead.
		/// [JP] 取得時に記録した Revision をカタログが照合する。食い違えば「他の人が先にいる」として報告される。
		if (!catalog_.Publish(assetId, workspace_[assetId].revision_, files, asset->dependencies_))
		{
			return false;
		}

		/// [EN] The local record moves forward too, so a second publish in a row states the right starting point.
		/// [JP] ローカルの記録も進める。続けてもう一度 Publish する際に、正しい起点を示せるようにするため。

		/// [EN] The hashes just uploaded become the new baseline, so what was published no longer counts as a local change.
		/// [JP] 今アップロードしたハッシュが新しい基準になる。公開した内容が、以後ローカルの変更として数えられないようにするため。
		WorkspaceRecord record;
		record.revision_ = workspace_[assetId].revision_ + 1;
		for (const SharedFile& file : files)
		{
			std::error_code errorCode;
			record.files_.push_back(WorkspaceFile{ file.path_, file.hash_, file.size_, std::filesystem::last_write_time(Local(file.path_), errorCode).time_since_epoch().count() });
		}
		workspace_[assetId] = record;
		SaveWorkspace();

		/// [EN] The lease is handed back on success, which is what turns "editing" back into "available" for the team.
		/// [JP] 成功したら Lease を返す。これがチームにとって「編集中」を「空いている」に戻す操作になる。
		locks_.Release(assetId, String("asset"));
		return true;
	}

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
	Bool SharingWorker::Register(const SharingRequest& request)
	{
		SharedAsset asset;

		/// [EN] The shared identifier is made here and never changes again, even if the asset is later renamed or moved.
		/// [JP] 共有識別子はここで作り、以後変わらない。後からリネームや移動をしても同じままになる。
		std::random_device randomDevice;
		asset.id_ = String(std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:08x}{:04x}", randomDevice(), randomDevice() >> 16, randomDevice() >> 16, randomDevice() >> 16, randomDevice(), randomDevice() >> 16));
		asset.path_ = request.path_;
		asset.runtimeId_ = request.runtimeId_;
		asset.type_ = request.type_;

		/// [EN] The asset and its .meta travel together, because the .meta is what carries the identity the engine loads it by.
		/// [JP] アセットと .meta は一緒に運ぶ。エンジンが読み込む際の識別情報を持っているのは .meta の側であるため。
		SharedFile main;
		SharedFile meta;
		if (!Store(request.path_, main) || !Store(String(std::format("{}.meta", request.path_.str())), meta))
		{
			return false;
		}
		asset.files_.push_back(main);
		asset.files_.push_back(meta);

		if (!catalog_.Register(asset))
		{
			return false;
		}

		/// [EN] A freshly registered asset starts at revision 1, which is also what this machine now has.
		/// [JP] 登録直後のアセットは Revision 1 から始まり、この PC が持っているのもその状態になる。
		WorkspaceRecord record;
		record.revision_ = 1;
		for (const SharedFile& file : asset.files_)
		{
			std::error_code errorCode;
			record.files_.push_back(WorkspaceFile{ file.path_, file.hash_, file.size_, std::filesystem::last_write_time(Local(file.path_), errorCode).time_since_epoch().count() });
		}
		workspace_[asset.id_] = record;
		SaveWorkspace();
		return true;
	}


	/**
	* [EN]
	* Keeps the library's Assets folder shaped like the workspace's
	* own, and takes in whatever has been dropped into it. Folders are
	* created where they are missing, and a file whose contents this
	* machine does not already have is downloaded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ライブラリの Assets フォルダを、ワークスペース側と同じ形に保ち、
	* そこへ置かれたものを取り込む。足りないフォルダは作り、この PC が
	* まだ持っていない中身のファイルをダウンロードする。
	*/
	void SharingWorker::Mirror(const std::filesystem::path& localFolder, const String& driveFolderId)
	{
		DynamicArray<DriveEntry> entries;
		if (!drive_.List(driveFolderId, entries))
		{
			return;
		}

		/// [EN] What the library already shows is noted first, so the folders created below are only the ones missing.
		/// [JP] 先にライブラリ側にあるものを控える。下で作るフォルダが、足りないものだけになるようにするため。
		std::unordered_map<std::string, DriveEntry> remote;
		for (const DriveEntry& entry : entries)
		{
			remote[entry.name_.str()] = entry;
		}

		/// [EN] A file sitting in the library's Assets tree is something an artist put there, and belongs at the matching place here.
		/// [JP] ライブラリの Assets の中にあるファイルは、アーティストが置いたもので、こちら側の対応する位置に属する。
		for (const DriveEntry& entry : entries)
		{
			if (entry.folder_)
			{
				continue;
			}

			/// [EN] A .meta beside it is the Editor's own bookkeeping, and is made here rather than taken from the library.
			/// [JP] 隣の .meta は Editor 側の管理情報で、ライブラリから取るのではなくこちらで作る。
			std::filesystem::path local = localFolder / entry.name_.str();
			if (local.extension() == ".meta")
			{
				continue;
			}

			/// [EN] Comparing hashes is what keeps this from downloading the same drop on every check, and what notices a replacement.
			/// [JP] ハッシュを比べることで、確認のたびに同じものを取り直さずに済み、差し替えにも気づける。
			DynamicArray<Byte> digest = Sha256::Hash(local);
			std::string current;
			for (Byte value : digest)
			{
				current += std::format("{:02x}", static_cast<Uint8>(value));
			}
			if (!entry.hash_.str().empty() && current == entry.hash_.str())
			{
				continue;
			}

			/// [EN] Drive does not hash every file, and a size that already matches is taken as the same contents.
			/// [JP] Drive は全てのファイルにハッシュを持つわけではない。サイズが既に一致するものは同じ中身とみなす。
			std::error_code errorCode;
			if (entry.hash_.str().empty() && std::filesystem::file_size(local, errorCode) == entry.size_ && !errorCode)
			{
				continue;
			}

			std::filesystem::path staging = local;
			staging += ".download";
			if (!drive_.Fetch(entry.id_, staging))
			{
				continue;
			}
			std::filesystem::rename(staging, local, errorCode);
			if (errorCode)
			{
				std::filesystem::remove(staging, errorCode);
				continue;
			}

			String logical = String(std::filesystem::relative(local, projectRoot_ / config_.workspace_.str()).generic_string());

			/// [EN] A drop onto an asset the team already has is a replacement, and it is published from here so everyone receives the fix.
			/// [JP] チームが既に持っているアセットへの投下は差し替えであり、ここから公開して修正が全員へ届くようにする。

			/// [EN] Its identity stays as it was: the .meta beside it is untouched, so every scene and prefab pointing at it keeps pointing at it.
			/// [JP] 同一性はそのまま保つ。隣の .meta に触れないため、それを指している Scene や Prefab の参照は切れない。
			/// [EN] Only a machine that already holds the asset can publish a replacement, since a publish states which revision it was built on.
			/// [JP] 差し替えを公開できるのは、そのアセットを既に持っている PC だけ。公開はどの Revision を元にしたかを示すものであるため。
			const SharedAsset* existing = catalog_.FindPath(logical);
			if (existing && workspace_.contains(existing->id_))
			{
				/// [EN] Taking the lease first is what keeps this from overwriting a member who is editing that same asset right now.
				/// [JP] 先に編集権を取ることが、今まさにそのアセットを編集しているメンバーを上書きしない条件になる。
				if (locks_.Acquire(existing->id_, String("asset")))
				{
					Publish(existing->id_);
				}
				continue;
			}

			/// [EN] Otherwise the Editor is told, because a file that arrived this way has no identity yet and is not in the catalog.
			/// [JP] そうでなければ Editor へ知らせる。この経路で届いたファイルはまだ識別情報を持たず、カタログにも載っていないため。
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			imported_.push_back(logical);
		}

		/// [EN] Folders are walked after the files, so a drop at this level is taken in before descending.
		/// [JP] フォルダはファイルの後に辿る。この階層に置かれたものを先に取り込むため。
		for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(localFolder))
		{
			if (!child.is_directory())
			{
				continue;
			}

			/// [EN] A folder the library does not have yet is created, which is what keeps its tree shaped like this one.
			/// [JP] ライブラリ側にまだ無いフォルダは作る。これがライブラリの構成をこちらと同じ形に保つ動き。
			std::string name = child.path().filename().string();
			auto found = remote.find(name);
			String childId = found == remote.end() ? drive_.CreateFolder(String(name), driveFolderId) : found->second.id_;
			if (!childId.str().empty())
			{
				Mirror(child.path(), childId);
			}
		}
	}

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
	Bool SharingWorker::Store(const String& logicalPath, SharedFile& stored)
	{
		std::filesystem::path local = Local(logicalPath);

		/// [EN] The hash of the contents is both the check that they arrived intact and the name they are stored under.
		/// [JP] 中身のハッシュは、正しく届いたかの確認であると同時に、保存する際の名前でもある。
		DynamicArray<Byte> digest = Sha256::Hash(local);
		if (digest.empty())
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			snapshot_.error_ = String(std::format("\"{}\" を読み取れませんでした。", logicalPath.str()));
			return false;
		}

		std::string hash;
		for (Byte value : digest)
		{
			hash += std::format("{:02x}", static_cast<Uint8>(value));
		}

		std::error_code errorCode;
		stored.path_ = logicalPath;
		stored.hash_ = String(hash);
		stored.size_ = std::filesystem::file_size(local, errorCode);

		/// [EN] Contents already in the library are reused rather than sent again, so an unchanged file costs nothing to publish.
		/// [JP] 既にライブラリにある中身は送り直さず再利用する。変更のないファイルの Publish に費用がかからないのはこのため。
		stored.driveId_ = drive_.Find(stored.hash_, config_.blobFolderId_);
		if (!stored.driveId_.str().empty())
		{
			return true;
		}

		stored.driveId_ = drive_.Upload(local, stored.hash_, config_.blobFolderId_);
		if (stored.driveId_.str().empty())
		{
			std::lock_guard<std::mutex> guard(snapshotMutex_);
			snapshot_.error_ = drive_.Error();
			return false;
		}
		return true;
	}

	/**
	* [EN]
	* Turns a workspace-relative path into a path on this machine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ワークスペース内の位置を、この PC 上のパスへ変換する。
	*/
	std::filesystem::path SharingWorker::Local(const String& logicalPath)const
	{
		/// [EN] Shared paths are written the same way on every machine, and only this step makes them local.
		/// [JP] 共有される位置はどの PC でも同じ書き方で、ローカルの形になるのはこの一手だけ。
		return projectRoot_ / config_.workspace_.str() / logicalPath.str();
	}

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
	void SharingWorker::Capture()
	{
		/// [EN] The files are examined before the lock is taken, so the Editor thread never waits on disk work.
		/// [JP] ロックを取る前にファイルを調べる。Editor のスレッドがディスク作業を待つことがないようにするため。
		DynamicArray<SharedProgress> progress;
		for (const std::pair<const String, WorkspaceRecord>& record : workspace_)
		{
			const SharedAsset* asset = catalog_.Find(record.first);
			if (!asset || asset->deleted_)
			{
				continue;
			}

			Bool modified = false;
			for (const WorkspaceFile& file : record.second.files_)
			{
				std::error_code errorCode;
				std::filesystem::path local = Local(file.path_);

				/// [EN] Size and time are read first, and a file matching both is untouched without its contents being read.
				/// [JP] 先にサイズと時刻を読む。両方一致するファイルは、中身を読まずに無変更とする。
				Uint64 size = std::filesystem::file_size(local, errorCode);
				Int64 stamp = std::filesystem::last_write_time(local, errorCode).time_since_epoch().count();
				if (!errorCode && size == file.size_ && stamp == file.stamp_)
				{
					continue;
				}

				/// [EN] A file that is gone is a hole the library can fill, not work of this member's to report.
				/// [JP] 消えているファイルはライブラリが埋められる欠けであって、報告すべきこのメンバーの作業ではない。
				if (!std::filesystem::exists(local))
				{
					continue;
				}

				/// [EN] Otherwise the contents decide, so merely re-saving a file does not read as a change.
				/// [JP] そうでなければ中身で判断する。保存し直しただけの場合に変更として見えないようにするため。
				DynamicArray<Byte> digest = Sha256::Hash(local);
				std::string current;
				for (Byte value : digest)
				{
					current += std::format("{:02x}", static_cast<Uint8>(value));
				}
				modified = modified || current != file.hash_.str();
			}

			/// [EN] An asset that matches both sides needs no entry, which keeps this list as short as the trouble is.
			/// [JP] 両側と一致しているアセットには項目が要らない。この一覧が、問題の数と同じ長さで済むようにするため。
			Bool behind = record.second.revision_ < asset->revision_;
			if (!modified && !behind)
			{
				continue;
			}

			SharedProgress entry;
			entry.assetId_ = record.first;
			entry.modified_ = modified;

			/// [EN] Changed on both sides is the conflict: neither copy can simply replace the other.
			/// [JP] 両側で変わっているのが競合。どちらの写しも、もう一方を単純に置き換えられない。
			entry.conflicted_ = modified && behind;
			progress.push_back(entry);
		}

		std::lock_guard<std::mutex> guard(snapshotMutex_);
		snapshot_.assets_ = catalog_.Assets();
		snapshot_.progress_ = progress;

		/// [EN] Leases are flattened out of the table so the Editor can show them without knowing how the table is stored.
		/// [JP] Lease は表から平らに取り出す。Editor が、表の保持形式を知らずに表示できるようにするため。
		snapshot_.leases_.clear();
		for (const SharedAsset& asset : snapshot_.assets_)
		{
			for (const String& scope : { String("asset"), String("structure"), String("context") })
			{
				const EditLease* lease = locks_.Find(asset.id_, scope);
				if (lease)
				{
					snapshot_.leases_.push_back(*lease);
				}
			}
		}

		/// [EN] Moving this number is the signal the Editor watches to know its own view has gone stale.
		/// [JP] この番号を動かすことが、Editor が「自分の表示が古くなった」と知るための合図になる。
		++snapshot_.revision_;
	}

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
	void SharingWorker::LoadWorkspace()
	{
		/// [EN] This record is per machine, never shared, and losing it only means every asset looks not-yet-fetched.
		/// [JP] この記録は PC ごとのもので共有しない。失っても「まだ取得していない」に見えるだけで済む。
		std::ifstream stream(projectRoot_ / ".asset" / "workspace.json");
		if (!stream)
		{
			return;
		}

		nlohmann::json workspace = nlohmann::json::parse(stream, nullptr, false);
		if (!workspace.is_object() || !workspace.contains("assets"))
		{
			return;
		}

		for (auto& entry : workspace["assets"].items())
		{
			WorkspaceRecord record;
			record.revision_ = entry.value().value("revision", Uint64(0));

			/// [EN] Where the contents live in Drive is left out, since that is the catalog's business rather than this machine's.
			/// [JP] 中身が Drive のどこにあるかは書かない。それはカタログ側の管轄で、この PC の記録ではないため。
			for (const nlohmann::json& file : entry.value()["files"])
			{
				WorkspaceFile stored;
				stored.path_ = String(file.value("path", ""));
				stored.hash_ = String(file.value("hash", ""));
				stored.size_ = file.value("size", Uint64(0));
				stored.stamp_ = file.value("stamp", Int64(0));
				record.files_.push_back(stored);
			}
			workspace_[String(entry.key())] = record;
		}
	}

	/**
	* [EN]
	* Writes that record back, so it survives the Editor being closed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その記録を書き戻す。Editor を閉じても残るようにするため。
	*/
	void SharingWorker::SaveWorkspace()const
	{
		nlohmann::json workspace;
		workspace["version"] = 1;
		workspace["assets"] = nlohmann::json::object();
		for (const std::pair<const String, WorkspaceRecord>& entry : workspace_)
		{
			nlohmann::json files = nlohmann::json::array();
			for (const WorkspaceFile& file : entry.second.files_)
			{
				files.push_back({ { "path", file.path_.str() }, { "hash", file.hash_.str() }, { "size", file.size_ }, { "stamp", file.stamp_ } });
			}
			workspace["assets"][entry.first.str()] = { { "revision", entry.second.revision_ }, { "files", files } };
		}

		/// [EN] The folder may not exist before the first get, so it is created rather than assumed.
		/// [JP] 最初の取得より前はフォルダが無いことがあるため、あるものとせず作ってから書く。
		std::error_code errorCode;
		std::filesystem::create_directories(projectRoot_ / ".asset", errorCode);
		std::ofstream stream(projectRoot_ / ".asset" / "workspace.json");
		if (stream)
		{
			stream << workspace.dump(1, '\t');
		}
	}
}
