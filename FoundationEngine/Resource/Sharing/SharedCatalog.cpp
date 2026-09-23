#include <FoundationEngine/Resource/Sharing/SharedCatalog.h>

namespace SeedCore
{
	/**
	* [EN]
	* Takes the document this catalog lives in.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このカタログが置かれているドキュメントを受け取る。
	*/
	SharedCatalog::SharedCatalog(GoogleDocument& document, const String& documentId) : document_(document), documentId_(documentId)
	{
		/// No Code
	}

	/**
	* [EN]
	* Drops the in-memory copy; the catalog itself lives in the
	* document and outlives any Editor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メモリ上の写しを捨てるだけ。カタログ自体はドキュメントにあり、
	* どの Editor よりも長く残る。
	*/
	SharedCatalog::~SharedCatalog()
	{
		/// [EN] Nothing is written on the way out, so a crash mid-session leaves the catalog exactly as it was.
		/// [JP] 終了時に書き込むものは無いため、作業中にクラッシュしてもカタログはそのままの状態で残る。

		/// No Code
	}

	/**
	* [EN]
	* Re-reads the catalog so the Editor sees what the team currently
	* has and at which revisions.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カタログを読み直し、チームが今何を持っていて、それぞれどの
	* Revision なのかを Editor へ反映する。
	*/
	Bool SharedCatalog::Refresh()
	{
		/// [EN] This read is what makes an asset another member published appear in this Editor's browser.
		/// [JP] 他のメンバーが Publish したアセットが、この Editor のブラウザに現れるのは、この読み取りによる。
		GoogleDocumentSnapshot snapshot = document_.Read(documentId_);
		if (!snapshot.valid_)
		{
			error_ = document_.Error();
			return false;
		}

		/// [EN] An empty or damaged document is taken as an empty catalog, which is also the state before the first publish.
		/// [JP] 空や壊れたドキュメントは空のカタログとして扱う。これは最初の Publish より前の状態でもある。
		nlohmann::json catalog = nlohmann::json::parse(snapshot.text_.str(), nullptr, false);
		Adopt(catalog);
		error_ = String();
		return true;
	}

	/**
	* [EN]
	* Every asset in the catalog as of the last read, retired ones
	* included.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の読み取り時点でカタログにある全アセット。廃止済みのものも
	* 含む。
	*/
	const DynamicArray<SharedAsset>& SharedCatalog::Assets()const
	{
		return assets_;
	}

	/**
	* [EN]
	* Looks an asset up by its shared identifier.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有識別子でアセットを引く。
	*/
	const SharedAsset* SharedCatalog::Find(const String& assetId)const
	{
		/// [EN] The catalog holds a few thousand entries at most, so a straight scan is cheaper than keeping an index in step.
		/// [JP] カタログの項目はせいぜい数千なので、索引を維持するより素直に走査する方が安く済む。
		for (const SharedAsset& asset : assets_)
		{
			if (asset.id_ == assetId)
			{
				return &asset;
			}
		}
		return nullptr;
	}

	/**
	* [EN]
	* Looks an asset up by the 32-bit identifier the engine uses locally.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンがローカルで使う32ビットの識別子でアセットを引く。
	*/
	const SharedAsset* SharedCatalog::Find(Uint32 runtimeId)const
	{
		/// [EN] This is the lookup the Editor's own panels need, since they know assets by that number rather than by the shared one.
		/// [JP] Editor の各パネルが必要とするのはこちら。パネル側はアセットを共有識別子ではなくこの番号で知っているため。
		for (const SharedAsset& asset : assets_)
		{
			if (asset.runtimeId_ == runtimeId)
			{
				return &asset;
			}
		}
		return nullptr;
	}

	/**
	* [EN]
	* Looks an asset up by its workspace path.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ワークスペース上の位置でアセットを引く。
	*/
	const SharedAsset* SharedCatalog::FindPath(const String& path)const
	{
		/// [EN] Paths are compared as written, since every one of them is produced the same way from the workspace root.
		/// [JP] 位置は書かれたまま比較する。どれもワークスペースの起点から同じ方法で作られているため。
		for (const SharedAsset& asset : assets_)
		{
			if (asset.path_ == path)
			{
				return &asset;
			}
		}
		return nullptr;
	}

	/**
	* [EN]
	* Adds a local asset to the catalog as revision 1. Fails when its
	* path or its 32-bit identifier is already taken, which is how two
	* members creating the same thing separately is caught.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ローカルのアセットを Revision 1 としてカタログへ加える。位置か
	* 32ビット識別子が既に使われていれば失敗する。2人が別々に同じものを
	* 作ってしまった場合は、これで検出できる。
	*/
	Bool SharedCatalog::Register(const SharedAsset& asset)
	{
		outdated_ = false;
		return Modify([this, asset](nlohmann::json& catalog)
		{
			/// [EN] Both checks run against the freshly read catalog, so a member who published a moment ago is seen here.
			/// [JP] 2つの確認は読み直したカタログに対して行う。直前に Publish したメンバーの分もここで見える。
			for (auto& entry : catalog["assets"].items())
			{
				Bool samePath = entry.value().value("path", "") == asset.path_.str();
				Bool sameRuntimeId = entry.value().value("runtimeId", Uint32(0)) == asset.runtimeId_;

				/// [EN] A path clash means two members made assets at the same place; a number clash means their .meta files agree by accident.
				/// [JP] 位置の衝突は2人が同じ場所にアセットを作ったということ。番号の衝突は .meta が偶然一致したということ。
				if (samePath || sameRuntimeId)
				{
					error_ = String(std::format("\"{}\" は既に共有されています。", entry.value().value("path", "")));
					return false;
				}
			}

			/// [EN] Files are written out one by one because each carries both where it belongs and which blob holds it.
			/// [JP] ファイルは1つずつ書き出す。それぞれが、属する位置と中身を持つ blob の両方を持っているため。
			nlohmann::json files = nlohmann::json::array();
			for (const SharedFile& file : asset.files_)
			{
				files.push_back({ { "path", file.path_.str() }, { "hash", file.hash_.str() }, { "driveId", file.driveId_.str() }, { "size", file.size_ } });
			}

			nlohmann::json dependencies = nlohmann::json::array();
			for (const String& dependency : asset.dependencies_)
			{
				dependencies.push_back(dependency.str());
			}

			/// [EN] Revision starts at 1, so 0 can keep meaning "this Editor has never published it".
			/// [JP] Revision は1から始める。0を「この Editor はまだ Publish していない」の意味に残しておくため。
			nlohmann::json entry;
			entry["path"] = asset.path_.str();
			entry["runtimeId"] = asset.runtimeId_;
			entry["type"] = asset.type_;
			entry["revision"] = 1;
			entry["files"] = files;
			entry["dependencies"] = dependencies;
			entry["scene"] = asset.scene_;
			entry["sceneDocumentId"] = asset.sceneDocumentId_.str();
			entry["deleted"] = false;
			catalog["assets"][asset.id_.str()] = entry;
			return true;
		});
	}

	/**
	* [EN]
	* Records new contents for an asset and moves it to the next
	* revision. baseRevision is the revision the member started from,
	* and a mismatch means someone else published first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセットの新しい中身を記録し、次の Revision へ進める。
	* baseRevision はそのメンバーが作業を始めた時点の Revision で、
	* 食い違っていれば他の誰かが先に Publish したということ。
	*/
	Bool SharedCatalog::Publish(const String& assetId, Uint64 baseRevision, const DynamicArray<SharedFile>& files, const DynamicArray<String>& dependencies)
	{
		outdated_ = false;
		return Modify([this, assetId, baseRevision, files, dependencies](nlohmann::json& catalog)
		{
			if (!catalog["assets"].contains(assetId.str()))
			{
				error_ = String("そのアセットは共有ライブラリにありません。");
				return false;
			}
			nlohmann::json& entry = catalog["assets"][assetId.str()];

			/// [EN] This comparison is the guard that keeps work built on an old copy from replacing newer work.
			/// [JP] この比較が、古い写しを元にした作業が新しい成果を置き換えてしまうのを防ぐ関門。
			if (entry.value("revision", Uint64(0)) != baseRevision)
			{
				outdated_ = true;
				error_ = String("他のメンバーが先に公開しています。先に最新を取得してください。");
				return false;
			}

			/// [EN] The file list is replaced rather than merged, since a publish describes the asset in full.
			/// [JP] ファイル一覧は統合ではなく置き換えにする。Publish はアセットの全体を記述するものであるため。
			nlohmann::json list = nlohmann::json::array();
			for (const SharedFile& file : files)
			{
				list.push_back({ { "path", file.path_.str() }, { "hash", file.hash_.str() }, { "driveId", file.driveId_.str() }, { "size", file.size_ } });
			}
			entry["files"] = list;

			nlohmann::json links = nlohmann::json::array();
			for (const String& dependency : dependencies)
			{
				links.push_back(dependency.str());
			}
			entry["dependencies"] = links;

			/// [EN] Moving the number forward is what tells every other member that their copy is now behind.
			/// [JP] この番号を進めることが、他の全メンバーに「手元の写しは古くなった」と伝える手段になる。
			entry["revision"] = baseRevision + 1;

			/// [EN] Publishing over a retired asset brings it back, which is how a deletion is undone.
			/// [JP] 廃止済みのアセットに Publish すると復活する。削除を取り消す手段がこれにあたる。
			entry["deleted"] = false;
			return true;
		});
	}

	/**
	* [EN]
	* Marks an asset as retired without removing its entry, so members
	* holding an old copy can tell it was deleted on purpose.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 項目は残したままアセットを廃止済みにする。古い写しを持っている
	* メンバーが、意図して削除されたものだと分かるようにするため。
	*/
	Bool SharedCatalog::Retire(const String& assetId, Uint64 baseRevision)
	{
		outdated_ = false;
		return Modify([this, assetId, baseRevision](nlohmann::json& catalog)
		{
			if (!catalog["assets"].contains(assetId.str()))
			{
				error_ = String("そのアセットは共有ライブラリにありません。");
				return false;
			}
			nlohmann::json& entry = catalog["assets"][assetId.str()];

			/// [EN] Retiring is checked against the revision as well, so nobody deletes work they have not seen.
			/// [JP] 廃止も Revision を確認したうえで行う。見ていない成果を誰かが削除してしまわないようにするため。
			if (entry.value("revision", Uint64(0)) != baseRevision)
			{
				outdated_ = true;
				error_ = String("他のメンバーが先に公開しています。先に最新を取得してください。");
				return false;
			}

			/// [EN] The entry stays with its file list intact, which is what makes bringing it back possible later.
			/// [JP] 項目はファイル一覧を保ったまま残す。これが後から復活させられる理由。
			entry["deleted"] = true;
			entry["revision"] = baseRevision + 1;
			return true;
		});
	}

	/**
	* [EN]
	* Whether the last change was refused because the asset had already
	* moved on. The caller should get the newer revision first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直前の変更が、アセットが既に先へ進んでいたために拒否されたか
	* どうか。その場合、呼び出し側は先に新しい Revision を取得する。
	*/
	Bool SharedCatalog::Outdated()const
	{
		return outdated_;
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
	const String& SharedCatalog::Error()const
	{
		return error_;
	}

	/**
	* [EN]
	* Reads the catalog, applies edit to it, and writes it back,
	* retrying from the fresh contents whenever another member wrote
	* first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カタログを読み、edit を適用して書き戻す。他のメンバーが先に
	* 書いていた場合は、新しい内容から読み直してやり直す。
	*/
	Bool SharedCatalog::Modify(const std::function<Bool(nlohmann::json&)>& edit)
	{
		/// [EN] Two members publishing different assets at the same moment both belong here; one simply goes second.
		/// [JP] 別々のアセットを同時に Publish する2人は、どちらもここを通る。片方が2周目に回るだけのこと。
		for (Int attempt = 0; attempt < 5; ++attempt)
		{
			GoogleDocumentSnapshot snapshot = document_.Read(documentId_);
			if (!snapshot.valid_)
			{
				error_ = document_.Error();
				return false;
			}

			/// [EN] The document is put into a known shape first, so the very first publish starts from an empty catalog.
			/// [JP] まず決まった形に整えるため、最初の Publish でも空のカタログから始められる。
			nlohmann::json catalog = nlohmann::json::parse(snapshot.text_.str(), nullptr, false);
			if (!catalog.is_object())
			{
				catalog = nlohmann::json::object();
			}
			catalog["version"] = 1;
			if (!catalog.contains("assets") || !catalog["assets"].is_object())
			{
				catalog["assets"] = nlohmann::json::object();
			}

			/// [EN] An edit that refuses still leaves the freshly read catalog in memory, which is what the caller inspects next.
			/// [JP] 編集が拒否された場合も、読み直したカタログはメモリに残る。呼び出し側が次に見るのはそれ。
			if (!edit(catalog))
			{
				Adopt(catalog);
				return false;
			}

			/// [EN] The write carries the revision the read came from, so it is refused if anyone wrote in between.
			/// [JP] 書き込みには読み取り時の版を添える。その間に誰かが書いていれば拒否される。
			if (document_.Write(documentId_, snapshot, String(catalog.dump())))
			{
				Adopt(catalog);
				error_ = String();
				return true;
			}

			/// [EN] Being refused over the document's own revision is not a conflict over the asset, so the attempt just starts over.
			/// [JP] ドキュメント自体の版で拒否されたのはアセットの競合ではないので、単純にやり直す。
			if (!document_.Conflicted())
			{
				error_ = document_.Error();
				return false;
			}
		}

		error_ = String("共有カタログが混み合っています。少し待ってからやり直してください。");
		return false;
	}

	/**
	* [EN]
	* Parses the document text into the in-memory asset list.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ドキュメントの本文を解釈して、メモリ上のアセット一覧にする。
	*/
	void SharedCatalog::Adopt(const nlohmann::json& catalog)
	{
		/// [EN] The list is rebuilt from scratch, so an asset retired elsewhere shows up as retired here too.
		/// [JP] 一覧は毎回作り直す。他所で廃止されたアセットは、こちらでも廃止済みとして現れる。
		assets_.clear();
		if (!catalog.is_object() || !catalog.contains("assets") || !catalog["assets"].is_object())
		{
			return;
		}

		/// [EN] items() is used because the identifier is the key rather than a field inside the entry.
		/// [JP] 識別子が項目の中ではなく見出しの側にあるため、items() を使う。
		for (auto& entry : catalog["assets"].items())
		{
			SharedAsset asset;
			asset.id_ = String(entry.key());
			asset.path_ = String(entry.value().value("path", ""));
			asset.runtimeId_ = entry.value().value("runtimeId", Uint32(0));
			asset.type_ = entry.value().value("type", Int32(0));
			asset.revision_ = entry.value().value("revision", Uint64(0));
			asset.scene_ = entry.value().value("scene", false);
			asset.sceneDocumentId_ = String(entry.value().value("sceneDocumentId", ""));
			asset.deleted_ = entry.value().value("deleted", false);

			/// [EN] A missing files array would mean a damaged catalog, so the loop is written to simply produce nothing.
			/// [JP] files が無いのはカタログが壊れている場合なので、その時は何も作らずに済む書き方にしている。
			if (entry.value().contains("files"))
			{
				for (const nlohmann::json& file : entry.value()["files"])
				{
					SharedFile shared;
					shared.path_ = String(file.value("path", ""));
					shared.hash_ = String(file.value("hash", ""));
					shared.driveId_ = String(file.value("driveId", ""));
					shared.size_ = file.value("size", Uint64(0));
					asset.files_.push_back(shared);
				}
			}

			if (entry.value().contains("dependencies"))
			{
				for (const nlohmann::json& dependency : entry.value()["dependencies"])
				{
					asset.dependencies_.push_back(String(dependency.get<std::string>()));
				}
			}
			assets_.push_back(asset);
		}
	}
}
