#include <FoundationEngine/Resource/Sharing/SharedLockTable.h>

namespace SeedCore
{
	/**
	* [EN]
	* Takes the document this table lives in and the identity that will
	* be recorded as the holder of any lease taken here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この表が置かれているドキュメントと、ここで取得する Lease の保持者
	* として記録される身元を受け取る。
	*/
	SharedLockTable::SharedLockTable(GoogleDocument& document, const String& documentId, const String& owner, const String& session) : document_(document), documentId_(documentId), owner_(owner), session_(session)
	{
		/// No Code
	}

	/**
	* [EN]
	* Drops the in-memory copy of the table. The leases themselves are
	* left in the document on purpose: they lapse on their own once
	* nothing renews them, which is what makes a crash recoverable.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メモリ上の写しを捨てるだけ。Lease 自体はあえてドキュメントに残す。
	* 誰も更新しなくなれば自然に失効するので、クラッシュしても復帰できる
	* のはそのため。
	*/
	SharedLockTable::~SharedLockTable()
	{
		/// [EN] A normal shutdown releases leases before this point, so what is left here is the crash path.
		/// [JP] 正常終了では、ここへ来る前に Lease を解放している。ここに残るのはクラッシュ時の経路。

		/// No Code
	}

	/**
	* [EN]
	* Re-reads the table so the Editor sees who currently holds what.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 表を読み直し、今どの範囲を誰が持っているかを Editor へ反映する。
	*/
	Bool SharedLockTable::Refresh()
	{
		/// [EN] Reading is how another member's lock becomes visible here; nothing is pushed to us.
		/// [JP] 他のメンバーの Lock がこちらに見えるのは読み取ったとき。向こうから知らせが来ることはない。
		GoogleDocumentSnapshot snapshot = document_.Read(documentId_);
		if (!snapshot.valid_)
		{
			error_ = document_.Error();
			return false;
		}

		/// [EN] A document that is empty or damaged is treated as an empty table rather than as a failure.
		/// [JP] 空、あるいは壊れたドキュメントは、失敗ではなく空の表として扱う。
		nlohmann::json table = nlohmann::json::parse(snapshot.text_.str(), nullptr, false);
		Adopt(table);
		error_ = String();
		return true;
	}

	/**
	* [EN]
	* Takes the lease for one scope of one asset, unless another member
	* holds it and has not let it lapse.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* あるアセットのある範囲の Lease を取得する。他のメンバーが保持
	* していて、かつ失効していない場合は取得しない。
	*/
	Bool SharedLockTable::Acquire(const String& assetId, const String& scope)
	{
		/// [EN] A lease is stored under the asset and the scope joined by a slash, which the scope itself never contains.
		/// [JP] Lease はアセットと範囲を斜線でつないだ見出しに格納する。範囲の側に斜線が入ることはない。
		String key = String(std::format("{}/{}", assetId.str(), scope.str()));

		/// [EN] The check and the write happen inside one read-modify-write, which is what makes "first one wins" hold.
		/// [JP] 確認と書き込みを1回の読み取り・変更・書き戻しの中で行う。これが「先着1人」を成り立たせている。
		return Modify([this, key](nlohmann::json& table)
		{
			Double now = document_.ServerTime();
			nlohmann::json& leases = table["leases"];

			/// [EN] An entry held by someone else that has not lapsed is the one case where the attempt gives up.
			/// [JP] 他人が保持していて、まだ失効していない場合だけ、取得をあきらめる。
			if (leases.contains(key.str()))
			{
				const nlohmann::json& existing = leases[key.str()];
				Bool mine = existing.value("session", "") == session_.str();
				Bool alive = existing.value("expiresAt", 0.0) > now;
				if (!mine && alive)
				{
					error_ = String(std::format("{} さんが編集中です。", existing.value("owner", "")));
					return false;
				}

				/// [EN] Re-taking a lease this Editor already holds just pushes its deadline back.
				/// [JP] この Editor が既に持っている Lease を取り直す場合は、期限を延ばすだけになる。
				if (mine)
				{
					leases[key.str()]["expiresAt"] = now + leaseSeconds_;
					return true;
				}
			}

			/// [EN] Writing a whole new entry also replaces a lapsed one, which is how an abandoned lock is taken over.
			/// [JP] 新しい項目を丸ごと書くことで失効した項目も置き換わる。放置された Lock はこうして引き継がれる。
			nlohmann::json lease;
			lease["owner"] = owner_.str();
			lease["session"] = session_.str();

			/// [EN] The token only has to be unguessable and unique; 128 bits of randomness covers both.
			/// [JP] トークンに必要なのは推測できないことと重複しないことだけで、128ビットの乱数で両方を満たす。
			std::random_device randomDevice;
			lease["token"] = std::format("{:08x}{:08x}{:08x}{:08x}", randomDevice(), randomDevice(), randomDevice(), randomDevice());
			lease["expiresAt"] = now + leaseSeconds_;
			leases[key.str()] = lease;
			return true;
		});
	}

	/**
	* [EN]
	* Pushes back the deadline of every lease this Editor holds, in one
	* write. Called on a timer while the Editor runs; stopping is what
	* eventually frees everything it held.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この Editor が持つ全 Lease の期限を、1回の書き込みで延ばす。実行中
	* は一定間隔で呼ばれ、呼ばれなくなることが、保持していた全てを解放
	* することにつながる。
	*/
	Bool SharedLockTable::Renew()
	{
		/// [EN] Holding nothing means there is nothing to say, and the table is left untouched.
		/// [JP] 何も持っていなければ言うべきこともないので、表には触れない。
		if (leases_.empty())
		{
			return true;
		}

		/// [EN] All of this Editor's leases move together, so five members cost five writes rather than one per lock.
		/// [JP] この Editor の全 Lease をまとめて動かす。そのため5人なら書き込みも5回で、Lock の数には比例しない。
		return Modify([this](nlohmann::json& table)
		{
			Double now = document_.ServerTime();
			Bool changed = false;
			for (nlohmann::json& lease : table["leases"])
			{
				if (lease.value("session", "") == session_.str())
				{
					lease["expiresAt"] = now + leaseSeconds_;
					changed = true;
				}
			}
			return changed;
		});
	}

	/**
	* [EN]
	* Gives up one scope, or every scope of one asset when scope is
	* empty.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 範囲を1つ手放す。scope が空なら、そのアセットの全範囲を手放す。
	*/
	Bool SharedLockTable::Release(const String& assetId, const String& scope)
	{
		/// [EN] Releasing on purpose frees the scope at once, rather than making others wait out the deadline.
		/// [JP] 明示的に手放すと、その範囲はすぐ空く。他のメンバーが期限切れを待つ必要がなくなる。
		return Modify([this, assetId, scope](nlohmann::json& table)
		{
			/// [EN] Releasing everything matches on the asset part alone, while one scope matches the whole key.
			/// [JP] 全部を手放す場合はアセット側だけで照合し、範囲を1つだけの場合は見出し全体で照合する。
			std::string prefix = std::format("{}/", assetId.str());
			std::string exact = std::format("{}/{}", assetId.str(), scope.str());
			Bool changed = false;

			/// [EN] Erasing while iterating is why the loop advances by hand instead of with a range-for.
			/// [JP] 走査しながら削除するため、範囲for ではなく手動で進める形にしている。
			for (auto it = table["leases"].begin(); it != table["leases"].end();)
			{
				Bool matches = scope.str().empty() ? it.key().rfind(prefix, 0) == 0 : it.key() == exact;

				/// [EN] Only this Editor's own leases are removed; another member's are none of our business.
				/// [JP] 削除するのはこの Editor 自身の Lease だけ。他のメンバーのものには手を出さない。
				if (matches && it.value().value("session", "") == session_.str())
				{
					it = table["leases"].erase(it);
					changed = true;
				}
				else
				{
					++it;
				}
			}
			return changed;
		});
	}

	/**
	* [EN]
	* Whether this Editor currently holds an unexpired lease for the
	* given scope. A held structure lease also covers the entities of
	* that scene.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この Editor が、その範囲の有効な Lease を今持っているか。structure
	* を持っている場合は、その Scene の Entity も対象に含む。
	*/
	Bool SharedLockTable::Held(const String& assetId, const String& scope)const
	{
		return !Token(assetId, scope).str().empty();
	}

	/**
	* [EN]
	* Returns the lease on the given scope whoever holds it, or nullptr
	* when the scope is free.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その範囲の Lease を、保持者が誰であれ返す。空いていれば nullptr。
	*/
	const EditLease* SharedLockTable::Find(const String& assetId, const String& scope)const
	{
		/// [EN] Lapsed entries are dropped when the table is read, so anything found here is still live.
		/// [JP] 失効した項目は読み取り時に落としているため、ここで見つかるものは今も有効。
		auto it = leases_.find(String(std::format("{}/{}", assetId.str(), scope.str())));
		return it == leases_.end() ? nullptr : &it->second;
	}

	/**
	* [EN]
	* The token proving this Editor's lease on the given scope, which a
	* publish has to present. Empty when no such lease is held.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その範囲について、この Editor の Lease を示すトークン。Publish の
	* 際に提示する。該当の Lease が無ければ空。
	*/
	String SharedLockTable::Token(const String& assetId, const String& scope)const
	{
		const EditLease* lease = Find(assetId, scope);
		if (lease && lease->session_ == session_)
		{
			return lease->token_;
		}

		/// [EN] Rearranging a scene is held as one structure lease, which stands in for each entity it moves.
		/// [JP] Scene の構造変更は1つの structure Lease で持つ。それが、動かす各 Entity の代わりを務める。
		if (scope.str().rfind("entity:", 0) == 0)
		{
			const EditLease* structure = Find(assetId, String("structure"));
			if (structure && structure->session_ == session_)
			{
				return structure->token_;
			}
		}
		return String();
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
	const String& SharedLockTable::Error()const
	{
		return error_;
	}

	/**
	* [EN]
	* Reads the table, applies edit to it, and writes it back, retrying
	* from the fresh contents whenever another member wrote first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 表を読み、edit を適用して書き戻す。他のメンバーが先に書いていた
	* 場合は、新しい内容から読み直してやり直す。
	*/
	Bool SharedLockTable::Modify(const std::function<Bool(nlohmann::json&)>& edit)
	{
		/// [EN] Losing the race is expected rather than exceptional, so a few attempts are made before giving up.
		/// [JP] 競争に負けるのは例外ではなく想定内なので、あきらめる前に何度か試す。
		for (Int attempt = 0; attempt < 5; ++attempt)
		{
			GoogleDocumentSnapshot snapshot = document_.Read(documentId_);
			if (!snapshot.valid_)
			{
				error_ = document_.Error();
				return false;
			}

			/// [EN] The document is rebuilt into a known shape first, so a first-ever run starts from an empty table.
			/// [JP] まず決まった形に整えるため、初回の実行でも空の表から始められる。
			nlohmann::json table = nlohmann::json::parse(snapshot.text_.str(), nullptr, false);
			if (!table.is_object())
			{
				table = nlohmann::json::object();
			}
			table["version"] = 1;
			if (!table.contains("leases") || !table["leases"].is_object())
			{
				table["leases"] = nlohmann::json::object();
			}

			/// [EN] Clearing out what has lapsed on every write keeps the table from growing without limit.
			/// [JP] 書き込みのたびに失効分を掃除することで、表が際限なく膨らむのを防ぐ。
			Double now = document_.ServerTime();
			for (auto it = table["leases"].begin(); it != table["leases"].end();)
			{
				it = it.value().value("expiresAt", 0.0) <= now ? table["leases"].erase(it) : std::next(it);
			}

			/// [EN] An edit that changes nothing still leaves the freshly read table in memory, which is all it needed.
			/// [JP] 何も変えない編集でも、読み直した表がメモリに残る。必要なのはそれだけの場合もある。
			if (!edit(table))
			{
				Adopt(table);
				return false;
			}

			/// [EN] The write carries the revision the read came from, so it is refused if anyone wrote in between.
			/// [JP] 書き込みには読み取り時の版を添える。その間に誰かが書いていれば拒否される。
			if (document_.Write(documentId_, snapshot, String(table.dump())))
			{
				Adopt(table);
				error_ = String();
				return true;
			}

			/// [EN] A refusal means someone else got there first, so the attempt starts over from their version.
			/// [JP] 拒否されたのは他の誰かが先に通ったということなので、その版から読み直してやり直す。
			if (!document_.Conflicted())
			{
				error_ = document_.Error();
				return false;
			}
		}

		/// [EN] Failing every attempt means the table is under constant contention, which is worth telling the member about.
		/// [JP] 全ての試行に失敗するのは、表が絶えず取り合いになっている状態。メンバーに伝える価値がある。
		error_ = String("共有ロック表が混み合っています。少し待ってからやり直してください。");
		return false;
	}

	/**
	* [EN]
	* Parses the document text into the in-memory leases, dropping the
	* ones that have already lapsed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ドキュメントの本文を解釈してメモリ上の Lease 一覧にする。すでに
	* 失効しているものは取り除く。
	*/
	void SharedLockTable::Adopt(const nlohmann::json& table)
	{
		/// [EN] The in-memory copy is rebuilt from scratch, so a lease released elsewhere disappears here as well.
		/// [JP] メモリ上の写しは毎回作り直す。他所で解放された Lease は、こちらからも消えることになる。
		leases_.clear();
		if (!table.is_object() || !table.contains("leases") || !table["leases"].is_object())
		{
			return;
		}

		Double now = document_.ServerTime();
		/// [EN] items() is used because both halves are needed, and its element type cannot be written out by hand.
		/// [JP] 見出しと値の両方が要るため items() を使う。この要素の型は書き下せないので auto にしている。
		for (auto& entry : table["leases"].items())
		{
			/// [EN] Anything already past its deadline is left out, so the rest of the Editor never sees a stale lock.
			/// [JP] 期限を過ぎたものは取り込まない。Editor の他の部分が古い Lock を見ることはない。
			if (entry.value().value("expiresAt", 0.0) <= now)
			{
				continue;
			}

			/// [EN] The key holds the asset and the scope joined by a slash, and the scope itself never contains one.
			/// [JP] 見出しはアセットと範囲を斜線でつないだもの。範囲の側に斜線が入ることはない。
			Size separator = entry.key().find('/');
			if (separator == std::string::npos)
			{
				continue;
			}

			EditLease lease;
			lease.assetId_ = String(entry.key().substr(0, separator));
			lease.scope_ = String(entry.key().substr(separator + 1));
			lease.owner_ = String(entry.value().value("owner", ""));
			lease.session_ = String(entry.value().value("session", ""));
			lease.token_ = String(entry.value().value("token", ""));
			lease.expiresAt_ = entry.value().value("expiresAt", 0.0);

			/// [EN] Comparing the session here means nothing outside this class has to know what a session is.
			/// [JP] ここでセッションを比べておけば、このクラスの外がセッションを知る必要はなくなる。
			lease.mine_ = lease.session_ == session_;
			leases_[String(entry.key())] = lease;
		}
	}

}
