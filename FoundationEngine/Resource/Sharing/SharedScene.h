#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/GoogleDocument.h>

namespace SeedCore
{
	/**
	* [EN]
	* One independently published piece of a shared scene: one entity, the
	* hierarchy, or the scene-wide settings. Splitting a scene this way is
	* what lets four members work in it at once without their saves
	* colliding.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有 Scene を構成する、独立して Publish される断片1つ。Entity 1体、
	* 階層、あるいは Scene 全体の設定のいずれか。Scene をこう分けることが、
	* 4人が同時に作業しても保存がぶつからない理由になる。
	*/
	struct ScenePart
	{
		/// [EN] Which piece this is: "entity:<UUID>", "structure" or "context".
		/// [JP] どの断片かを表す。"entity:<UUID>"、"structure"、"context" のいずれか。
		String scope_;

		/// [EN] Increases by one each time this piece alone is published.
		/// [JP] この断片だけが Publish されるたびに1つ増える。
		Uint64 revision_ = 0;

		/// [EN] SHA-256 of the piece's contents, used to tell a real change from a re-save.
		/// [JP] 断片の中身の SHA-256。本当の変更と、単なる保存し直しを見分けるために使う。
		String hash_;

		/// [EN] The piece itself, written straight into the document because an entity is normally a few kilobytes.
		/// [JP] 断片そのもの。Entity は通常数キロバイトなので、ドキュメントへそのまま書く。

		/// [EN] Empty when the piece was too large for that, in which case it sits in Drive and driveId_ names it.
		/// [JP] 大きすぎてそうできなかった場合は空になり、その断片は Drive にあって driveId_ が指す。
		String data_;

		/// [EN] Identifier of the Drive file holding those contents, used only for a piece too large to inline.
		/// [JP] その中身を保持している Drive のファイルの識別子。直接書くには大きすぎる断片でのみ使う。
		String driveId_;

		/// [EN] Size in bytes of the stored contents.
		/// [JP] 保存されている中身のバイト数。
		Uint64 size_ = 0;

		/// [EN] Marks an entity the team removed; the entry stays so others learn it was deleted rather than never seen.
		/// [JP] チームが削除した Entity の印。「削除された」と「元から知らない」を区別できるよう項目は残す。
		Bool deleted_ = false;
	};

	/**
	* [EN]
	* One piece a member wants to publish, together with the revision they
	* started from. The revision is what a member's work is checked
	* against, exactly as an ordinary asset's is.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メンバーが Publish しようとしている断片1つと、その作業を始めた時点の
	* Revision。通常のアセットと同じように、この Revision が照合の基準に
	* なる。
	*/
	struct ScenePartChange
	{
		/// [EN] Which piece is being published.
		/// [JP] どの断片を Publish するのか。
		String scope_;

		/// [EN] The revision this member started from; 0 means the piece is new.
		/// [JP] このメンバーが作業を始めた時点の Revision。0 なら新しい断片。
		Uint64 baseRevision_ = 0;

		/// [EN] SHA-256 of the new contents.
		/// [JP] 新しい中身の SHA-256。
		String hash_;

		/// [EN] The new contents, left empty when they were uploaded to Drive instead of being written into the document.
		/// [JP] 新しい中身。ドキュメントへ書かず Drive へアップロードした場合は空にする。
		String data_;

		/// [EN] Identifier of the Drive file the new contents were uploaded to, when they were too large to inline.
		/// [JP] 直接書くには大きすぎて Drive へアップロードした場合の、そのファイルの識別子。
		String driveId_;

		/// [EN] Size in bytes of the new contents.
		/// [JP] 新しい中身のバイト数。
		Uint64 size_ = 0;

		/// [EN] Whether this publish removes the entity rather than changing it.
		/// [JP] この Publish が、変更ではなく Entity の削除であるかどうか。
		Bool deleted_ = false;
	};

	/**
	* [EN]
	* The per-scene document, holding one revision per entity rather than
	* one for the scene as a whole. Two members editing different entities
	* therefore never contend, while two editing the same one are caught by
	* that entity's revision.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Scene ごとのドキュメント。Scene 全体で1つではなく、Entity ごとに
	* Revision を持つ。そのため別々の Entity を編集する2人は競合せず、同じ
	* Entity を編集した2人はその Entity の Revision で検出される。
	*/
	class SEEDCORE_API SharedScene
	{
	public:
		/**
		* [EN]
		* Takes the document holding one shared scene's pieces.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 共有 Scene 1つ分の断片を保持しているドキュメントを受け取る。
		*/
		SharedScene(GoogleDocument& document, const String& documentId);

		/**
		* [EN]
		* Drops the in-memory copy; the pieces themselves live in the
		* document.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メモリ上の写しを捨てるだけ。断片自体はドキュメントにある。
		*/
		~SharedScene();

		/// [EN] Copying is disallowed because the in-memory copy tracks one document at one point in time.
		/// [JP] メモリ上の写しは、ある時点のドキュメント1つを表すものなので、コピーは禁止する。
		SharedScene(const SharedScene&) = delete;
		SharedScene& operator=(const SharedScene&) = delete;

		/**
		* [EN]
		* Re-reads the scene so the Editor sees which entities other
		* members have moved on.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Scene を読み直し、他のメンバーがどの Entity を進めたのかを Editor
		* へ反映する。
		*/
		Bool Refresh();

		/**
		* [EN]
		* Every piece of the scene as of the last read, removed entities
		* included.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の読み取り時点における Scene の全断片。削除済みの Entity も
		* 含む。
		*/
		const DynamicArray<ScenePart>& Parts()const;

		/**
		* [EN]
		* Looks one piece up by its scope, or nullptr when the scene has no
		* such piece.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 範囲を指定して断片を1つ引く。該当が無ければ nullptr。
		*/
		const ScenePart* Find(const String& scope)const;

		/**
		* [EN]
		* Publishes several pieces at once, each only if it still sits at
		* the revision its change was built on. Either all of them are
		* recorded or none are, so the scene is never left half-updated.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 複数の断片をまとめて Publish する。各断片は、その変更の元になった
		* Revision のままである場合にだけ記録される。全て記録されるか、
		* 1つも記録されないかのどちらかなので、Scene が中途半端な状態で
		* 残ることはない。
		*/
		Bool Publish(const DynamicArray<ScenePartChange>& changes);

		/**
		* [EN]
		* Whether the last publish was refused because one of its pieces
		* had already moved on.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直前の Publish が、断片のいずれかが既に先へ進んでいたために拒否
		* されたかどうか。
		*/
		Bool Outdated()const;

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
		* Reads the scene, applies edit to it, and writes it back,
		* retrying from the fresh contents whenever another member wrote
		* first.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Scene を読み、edit を適用して書き戻す。他のメンバーが先に書いて
		* いた場合は、新しい内容から読み直してやり直す。
		*/
		Bool Modify(const std::function<Bool(nlohmann::json&)>& edit);

		/**
		* [EN]
		* Parses the document text into the in-memory piece list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ドキュメントの本文を解釈して、メモリ上の断片一覧にする。
		*/
		void Adopt(const nlohmann::json& scene);

	private:
		/// [EN] Reads and rewrites the document this scene lives in.
		/// [JP] この Scene が置かれているドキュメントの読み書きを行う。
		GoogleDocument& document_;

		/// [EN] Which document that is.
		/// [JP] そのドキュメントがどれかを示す識別子。
		String documentId_;

		/// [EN] The scene's pieces as of the last read.
		/// [JP] 直近の読み取り時点における Scene の断片。
		DynamicArray<ScenePart> parts_;

		/// [EN] Whether the last publish lost to a change that had already happened.
		/// [JP] 直前の Publish が、既に行われていた変更に負けたかどうか。
		Bool outdated_ = false;

		/// [EN] Last failure description; empty while everything is working.
		/// [JP] 直近の失敗の説明。問題なく動いている間は空。
		String error_;
	};
}
