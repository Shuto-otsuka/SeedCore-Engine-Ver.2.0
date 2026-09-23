#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/GoogleAuth.h>
#include <FoundationEngine/Resource/Sharing/HttpClient.h>

namespace SeedCore
{
	/**
	* [EN]
	* One reading of a shared document: its text and the revision that text
	* came from. Writing back requires handing this revision in, which is
	* how two members are prevented from overwriting each other.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有ドキュメントを1回読んだ結果。本文と、その本文が属する版を持つ。
	* 書き戻すときはこの版を渡す必要があり、それが2人による上書きを防ぐ
	* 仕組みになっている。
	*/
	struct GoogleDocumentSnapshot
	{
		/// [EN] Google's identifier for the state this text was read from.
		/// [JP] この本文を読み取った時点の状態を指す、Google 側の識別子。
		String revisionId_;

		/// [EN] The document's entire body as plain text.
		/// [JP] ドキュメント本文の全体をプレーンテキストにしたもの。
		String text_;

		/// [EN] Position just past the last character, needed to clear the body before rewriting it.
		/// [JP] 最後の文字の次の位置。本文を書き換える前に消去する範囲を決めるのに使う。
		Uint32 endIndex_ = 1;

		/// [EN] False when the read failed, in which case the other fields mean nothing.
		/// [JP] 読み取りに失敗した場合は false。そのとき他のフィールドには意味がない。
		Bool valid_ = false;
	};

	/**
	* [EN]
	* Reads and rewrites the Google documents that act as the shared
	* library's arbiter. A write is accepted only while the document still
	* sits at the revision it was read from, so of several members writing
	* at once exactly one succeeds and the rest are told to read again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有ライブラリの判定役となる Google ドキュメントを読み書きする。書き
	* 込みは、読み取った時の版のままである間だけ受理される。そのため複数の
	* メンバーが同時に書いても成功するのはちょうど1人で、残りは読み直しを
	* 促される。
	*/
	class SEEDCORE_API GoogleDocument
	{
	public:
		/**
		* [EN]
		* Takes the transport and the sign-in that every request depends
		* on.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* すべてのリクエストが依存する、通信手段とログイン情報を受け取る。
		*/
		GoogleDocument(HttpClient& http, GoogleAuth& auth);

		/**
		* [EN]
		* Nothing to undo: this class holds no handle of its own.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 後始末は要らない。このクラス自身は何のハンドルも持たないため。
		*/
		~GoogleDocument();

		/// [EN] Copying is disallowed because the conflict flag and the server clock belong to one caller.
		/// [JP] 競合の印とサーバー時刻は1つの呼び出し元に属するものなので、コピーは禁止する。
		GoogleDocument(const GoogleDocument&) = delete;
		GoogleDocument& operator=(const GoogleDocument&) = delete;

		/**
		* [EN]
		* Reads the whole document and the revision it is currently at.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ドキュメント全体と、現在の版を読み取る。
		*/
		GoogleDocumentSnapshot Read(const String& documentId);

		/**
		* [EN]
		* Replaces the body with text, but only while the document is still
		* at base's revision. Returns false both for a refused write and
		* for a failed one; Conflicted() tells the two apart.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* base の版のままである間に限り、本文を text で置き換える。拒否され
		* た場合も失敗した場合も false を返す。区別は Conflicted() で行う。
		*/
		Bool Write(const String& documentId, const GoogleDocumentSnapshot& base, const String& text);

		/**
		* [EN]
		* Whether the last Write was refused because someone else had
		* written first. The caller should read again and retry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直前の Write が、他の誰かが先に書いたために拒否されたかどうか。
		* この場合、呼び出し側は読み直してやり直せばよい。
		*/
		Bool Conflicted()const;

		/**
		* [EN]
		* Google's own clock, in seconds since the Unix epoch, as of the
		* last request. Lease deadlines are expressed against this rather
		* than against each member's own clock.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近のリクエスト時点における Google 側の時刻（Unixエポックからの
		* 秒数）。Lease の期限は各メンバーの時計ではなく、こちらを基準に
		* 表す。
		*/
		Double ServerTime()const;

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
		* Sends one authenticated request to the Docs API and parses its
		* JSON answer, recording the server clock and any failure.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 認証付きのリクエストを Docs API へ1回送り、JSON の応答を解釈する。
		* サーバー側の時刻と、失敗した場合はその内容も記録する。
		*/
		Bool Send(const String& method, const String& url, const nlohmann::json& request, nlohmann::json& result);

		/**
		* [EN]
		* Flattens the structural elements of a document into plain text.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ドキュメントの構造要素を、プレーンテキストへ平坦化する。
		*/
		static String ExtractText(const nlohmann::json& document);

	private:
		/// [EN] Transport shared with the rest of the sharing layer.
		/// [JP] 共有機能の他の部分と共用する通信手段。
		HttpClient& http_;

		/// [EN] Supplies the access token every request carries.
		/// [JP] 各リクエストに付けるアクセストークンの供給元。
		GoogleAuth& auth_;

		/// [EN] Google's clock as of the last request; 0 until one has been made.
		/// [JP] 直近のリクエスト時点の Google 側の時刻。まだ何も送っていなければ 0。
		Double serverTime_ = 0.0;

		/// [EN] Whether the last write lost the race to another member.
		/// [JP] 直前の書き込みが、他のメンバーとの競争に負けたかどうか。
		Bool conflicted_ = false;

		/// [EN] Last failure description; empty while everything is working.
		/// [JP] 直近の失敗の説明。問題なく動いている間は空。
		String error_;
	};
}
