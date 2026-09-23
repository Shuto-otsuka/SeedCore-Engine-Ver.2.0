#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/GoogleDocument.h>

namespace SeedCore
{
	/**
	* [EN]
	* One member's right to edit one scope of one shared asset, for a
	* limited time. The right lapses on its own if the Editor stops saying
	* it is still there, so a crash never leaves anything locked forever.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* あるメンバーが、ある共有アセットの、ある範囲を編集する権利。期限付き
	* で、Editor が「まだ編集中」と言い続けなければ自然に失効するため、
	* クラッシュしても永久に Lock が残ることはない。
	*/
	struct EditLease
	{
		/// [EN] Which shared asset the lease is on, so the Editor can show it next to that asset.
		/// [JP] どの共有アセットに対する Lease か。Editor がそのアセットの横に表示するために使う。
		String assetId_;

		/// [EN] What is being edited: "asset", "entity:<UUID>", "structure" or "context".
		/// [JP] 編集対象の範囲。"asset"、"entity:<UUID>"、"structure"、"context" のいずれか。
		String scope_;

		/// [EN] Who holds it, as a name to show in the Editor.
		/// [JP] 誰が持っているか。Editor に表示するための名前。
		String owner_;

		/// [EN] Which Editor run holds it, so two machines of one person stay distinct.
		/// [JP] どの Editor の起動が持っているか。同じ人の2台を区別するために使う。
		String session_;

		/// [EN] Random value proving a publish comes from the run that took this lease.
		/// [JP] Publish がこの Lease を取った起動から来たことを示す乱数。

		/// [EN] An old Editor coming back after its lease lapsed therefore cannot publish.
		/// [JP] 失効後に復帰した古い Editor が Publish できないのは、この値が合わないため。
		String token_;

		/// [EN] When it lapses, in seconds on Google's clock rather than any member's own.
		/// [JP] 失効する時刻。メンバー各自の時計ではなく、Google の時計での秒数。
		Double expiresAt_ = 0.0;

		/// [EN] Whether this Editor is the holder, so the interface can tell "you may edit" from "someone else is editing".
		/// [JP] この Editor が保持者かどうか。「編集できる」と「他の人が編集中」を画面側で区別するために使う。
		Bool mine_ = false;
	};

	/**
	* [EN]
	* The shared lock table, held in one Google document that every member
	* rewrites. Each change is a read followed by a write that is accepted
	* only if nobody wrote in between, so of several members asking for the
	* same scope at once exactly one gets it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有の Lock 表。全メンバーが書き換える1つの Google ドキュメントに
	* 置かれる。変更は読み取りと書き込みの組で、その間に誰も書いていない
	* 場合だけ受理される。そのため同じ範囲を同時に要求しても、得られるのは
	* ちょうど1人。
	*/
	class SEEDCORE_API SharedLockTable
	{
	public:
		/**
		* [EN]
		* Takes the document this table lives in and the identity that will
		* be recorded as the holder of any lease taken here.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この表が置かれているドキュメントと、ここで取得する Lease の
		* 保持者として記録される身元を受け取る。
		*/
		SharedLockTable(GoogleDocument& document, const String& documentId, const String& owner, const String& session);

		/**
		* [EN]
		* Drops the in-memory copy only. Leases stay in the document and
		* lapse on their own once nothing renews them.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メモリ上の写しを捨てるだけ。Lease はドキュメントに残り、誰も
		* 更新しなくなれば自然に失効する。
		*/
		~SharedLockTable();

		/// [EN] Copying is disallowed because the session identity must name exactly one Editor run.
		/// [JP] セッションの身元はちょうど1つの Editor の起動を指す必要があるため、コピーは禁止する。
		SharedLockTable(const SharedLockTable&) = delete;
		SharedLockTable& operator=(const SharedLockTable&) = delete;

		/**
		* [EN]
		* Re-reads the table so the Editor sees who currently holds what.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 表を読み直し、今どの範囲を誰が持っているかを Editor へ反映する。
		*/
		Bool Refresh();

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
		Bool Acquire(const String& assetId, const String& scope);

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
		Bool Renew();

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
		Bool Release(const String& assetId, const String& scope);

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
		Bool Held(const String& assetId, const String& scope)const;

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
		const EditLease* Find(const String& assetId, const String& scope)const;

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
		String Token(const String& assetId, const String& scope)const;

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
		* Reads the table, applies edit to it, and writes it back, retrying
		* from the fresh contents whenever another member wrote first.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 表を読み、edit を適用して書き戻す。他のメンバーが先に書いていた
		* 場合は、新しい内容から読み直してやり直す。
		*/
		Bool Modify(const std::function<Bool(nlohmann::json&)>& edit);

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
		void Adopt(const nlohmann::json& table);

	private:
		/// [EN] Reads and rewrites the document this table lives in.
		/// [JP] この表が置かれているドキュメントの読み書きを行う。
		GoogleDocument& document_;

		/// [EN] Which document that is.
		/// [JP] そのドキュメントがどれかを示す識別子。
		String documentId_;

		/// [EN] Name shown to other members as the holder of a lease.
		/// [JP] Lease の保持者として他のメンバーに表示される名前。
		String owner_;

		/// [EN] Identifies this Editor run among all of them.
		/// [JP] すべての起動の中で、この Editor の起動を識別する値。
		String session_;

		/// [EN] Every lease currently in the table, by key, as of the last read.
		/// [JP] 直近の読み取り時点で表にある全 Lease。見出しごとに引ける。
		std::unordered_map<String, EditLease> leases_;

		/// [EN] How long a lease lasts, in seconds, before it has to be renewed.
		/// [JP] Lease が更新なしで有効な長さ（秒）。
		Double leaseSeconds_ = 120.0;

		/// [EN] Last failure description; empty while everything is working.
		/// [JP] 直近の失敗の説明。問題なく動いている間は空。
		String error_;
	};
}
