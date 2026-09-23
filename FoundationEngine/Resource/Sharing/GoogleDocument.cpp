#include <FoundationEngine/Resource/Sharing/GoogleDocument.h>

namespace SeedCore
{
	/**
	* [EN]
	* Takes the transport and the sign-in that every request depends on.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべてのリクエストが依存する、通信手段とログイン情報を受け取る。
	*/
	GoogleDocument::GoogleDocument(HttpClient& http, GoogleAuth& auth) : http_(http), auth_(auth)
	{
		/// No Code
	}

	/**
	* [EN]
	* Nothing to undo: this class holds no handle of its own, only
	* references to the transport and the sign-in that outlive it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 後始末は要らない。このクラス自身は何のハンドルも持たず、自分より
	* 長生きする通信手段とログイン情報への参照しか持たないため。
	*/
	GoogleDocument::~GoogleDocument()
	{
		/// [EN] A write in flight is a blocking call, so no request can still be running once this is reached.
		/// [JP] 書き込みは呼び出しが戻るまで待つ作りなので、ここへ来た時点で進行中のリクエストは存在しない。

		/// No Code
	}

	/**
	* [EN]
	* Reads the whole document and the revision it is currently at.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ドキュメント全体と、現在の版を読み取る。
	*/
	GoogleDocumentSnapshot GoogleDocument::Read(const String& documentId)
	{
		/// [EN] The snapshot stays invalid until both the revision and the text have been obtained.
		/// [JP] 版と本文の両方を取得できるまで、この結果は無効なままにしておく。
		GoogleDocumentSnapshot snapshot;

		/// [EN] The fields parameter asks Google to send only these two parts, leaving out the styling a document also carries.
		/// [JP] fields 指定で、この2つの部分だけを送るよう Google に頼む。ドキュメントが併せ持つ書式の情報は省かれる。
		nlohmann::json result;
		if (!Send(String("GET"), String(std::format("https://docs.googleapis.com/v1/documents/{}?fields=revisionId,body", documentId.str())), nlohmann::json(), result))
		{
			return snapshot;
		}

		/// [EN] This identifier is what a later write hands back to prove which state it was based on.
		/// [JP] この識別子は、後で書き込むときに「どの状態を元にしたか」を示すために渡すもの。
		snapshot.revisionId_ = String(result.value("revisionId", ""));
		snapshot.text_ = ExtractText(result);

		/// [EN] A document is addressed by character positions, and each element states where it ends.
		/// [JP] ドキュメントは文字位置で場所を指す作りで、各要素は自分の終端位置を持っている。

		/// [EN] The largest of those endings is therefore the end of the whole body, which a rewrite needs to know.
		/// [JP] その終端のうち最大のものが本文全体の終わりで、書き換えの際にこれが必要になる。
		snapshot.endIndex_ = 1;
		if (result.contains("body") && result["body"].contains("content"))
		{
			for (const nlohmann::json& element : result["body"]["content"])
			{
				snapshot.endIndex_ = std::max(snapshot.endIndex_, element.value("endIndex", Uint32(1)));
			}
		}

		/// [EN] Without a revision there is nothing to write against, so that is what decides validity.
		/// [JP] 版が無ければ書き込みの基準が無いため、有効かどうかはそこで決まる。
		snapshot.valid_ = !snapshot.revisionId_.str().empty();
		return snapshot;
	}

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
	Bool GoogleDocument::Write(const String& documentId, const GoogleDocumentSnapshot& base, const String& text)
	{
		/// [EN] The flag belongs to this attempt alone, so it is cleared before anything is sent.
		/// [JP] この印は今回の試行だけのものなので、何かを送る前に消しておく。
		conflicted_ = false;
		if (!base.valid_)
		{
			error_ = String("書き込みの前に、ドキュメントを読み取れていません。");
			return false;
		}

		/// [EN] There is no "replace the document" operation, so a rewrite is expressed as two edits in one request.
		/// [JP] 「丸ごと置き換える」操作は無いため、書き換えは1回のリクエスト内の2つの編集として表す。

		/// [EN] Both edits succeed or neither does, which is why the document is never left half-cleared.
		/// [JP] 2つの編集はまとめて成功か失敗かになる。そのため本文が消えかけのまま残ることはない。
		nlohmann::json request;
		request["requests"] = nlohmann::json::array();

		/// [EN] Positions start at 1, and the body always keeps a final newline that cannot be removed.
		/// [JP] 位置は1から始まり、本文の末尾には削除できない改行が必ず残る。

		/// [EN] So the cleared range runs from 1 to one short of the end, and an empty document has nothing to clear.
		/// [JP] したがって消去範囲は1から終端の1つ手前まで。空のドキュメントには消すものが無い。
		if (base.endIndex_ > 2)
		{
			nlohmann::json remove;
			remove["deleteContentRange"]["range"]["startIndex"] = 1;
			remove["deleteContentRange"]["range"]["endIndex"] = base.endIndex_ - 1;
			request["requests"].push_back(remove);
		}

		/// [EN] The new text goes in at the very start, which is position 1 once the old text is gone.
		/// [JP] 新しい本文は先頭へ入れる。古い本文が消えた後では、そこが位置1にあたる。
		nlohmann::json insert;
		insert["insertText"]["location"]["index"] = 1;
		insert["insertText"]["text"] = text.str();
		request["requests"].push_back(insert);

		/// [EN] This single field is the whole safety mechanism: the edits apply only while the document is still at this revision.
		/// [JP] 安全装置の全体がこの1フィールド。ドキュメントがこの版のままである間だけ、編集が適用される。

		/// [EN] When two members write at once, the first one moves the revision and the second is refused.
		/// [JP] 2人が同時に書いた場合、先に通った方が版を進め、後から来た方は拒否される。
		request["writeControl"]["requiredRevisionId"] = base.revisionId_.str();

		nlohmann::json result;
		return Send(String("POST"), String(std::format("https://docs.googleapis.com/v1/documents/{}:batchUpdate", documentId.str())), request, result);
	}

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
	Bool GoogleDocument::Conflicted()const
	{
		return conflicted_;
	}

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
	Double GoogleDocument::ServerTime()const
	{
		return serverTime_;
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
	const String& GoogleDocument::Error()const
	{
		return error_;
	}

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
	Bool GoogleDocument::Send(const String& method, const String& url, const nlohmann::json& request, nlohmann::json& result)
	{
		/// [EN] Asking for the token here is also what renews it, so callers never deal with expiry themselves.
		/// [JP] ここでトークンを求める動作が更新も兼ねる。そのため呼び出し側が期限を気にすることはない。
		String token = auth_.AccessToken();
		if (token.str().empty())
		{
			error_ = auth_.Error();
			return false;
		}

		/// [EN] "Bearer" means the token alone proves the right to act; whoever holds it can make this call.
		/// [JP] "Bearer" は、そのトークンを持っていること自体が権限の証明になる方式。持つ者が呼び出せる。
		DynamicArray<HttpHeader> headers;
		headers.push_back(HttpHeader{ String("Authorization"), String(std::format("Bearer {}", token.str())) });

		/// [EN] A read carries no body at all, which is why the request object is allowed to be null here.
		/// [JP] 読み取りではボディを一切送らない。request が null であってよいのはそのため。
		DynamicArray<Byte> payload;
		if (!request.is_null())
		{
			headers.push_back(HttpHeader{ String("Content-Type"), String("application/json; charset=utf-8") });
			std::string body = request.dump();
			payload.resize(body.size());
			std::memcpy(payload.data(), body.data(), body.size());
		}

		HttpResponse response = http_.Send(method, url, headers, payload);
		if (response.status_ == 0)
		{
			/// [EN] Nothing reached Google, so this is a network problem rather than anything about the document.
			/// [JP] Google まで届いていないので、これはドキュメントの問題ではなく通信の問題。
			error_ = response.error_;
			return false;
		}

		/// [EN] Every answer carries Google's clock, so keeping it means lease deadlines never rely on the local clock being right.
		/// [JP] どの応答にも Google 側の時刻が入っている。これを持っておけば、Lease の期限がローカルの時計に依存しなくなる。
		if (response.serverTime_ > 0.0)
		{
			serverTime_ = response.serverTime_;
		}

		/// [EN] Both successes and failures answer with JSON, so the body is parsed before the status is examined.
		/// [JP] 成功も失敗も JSON で返るため、ステータスを見る前に本文を解析しておく。
		result = nlohmann::json::parse(response.body_.begin(), response.body_.end(), nullptr, false);
		if (response.status_ == 200)
		{
			error_ = String();
			return result.is_object();
		}

		/// [EN] A refused write comes back as 400 with a message about the revision; that is an ordinary race, not a fault.
		/// [JP] 拒否された書き込みは、版について述べた 400 として返る。これは異常ではなく通常の競争の結果。

		/// [EN] It is reported without an error text, because the caller is expected to read again and retry quietly.
		/// [JP] エラー文言を残さずに報告するのは、呼び出し側が黙って読み直し、やり直すことを想定しているため。
		std::string message = result.is_object() && result.contains("error") ? result["error"].value("message", "") : "";
		if (response.status_ == 400 && message.find("revision") != std::string::npos)
		{
			conflicted_ = true;
			error_ = String();
			return false;
		}

		/// [EN] Anything else is worth showing: no permission, document deleted, quota, and so on.
		/// [JP] それ以外は表示する価値のある失敗。権限が無い、ドキュメントが消えている、上限に達した、など。
		error_ = String(std::format("共有ドキュメントへのアクセスに失敗しました ({}): {}", response.status_, message));
		return false;
	}

	/**
	* [EN]
	* Flattens the structural elements of a document into plain text.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ドキュメントの構造要素を、プレーンテキストへ平坦化する。
	*/
	String GoogleDocument::ExtractText(const nlohmann::json& document)
	{
		/// [EN] A document is a list of paragraphs, and each paragraph is a list of runs of text.
		/// [JP] ドキュメントは段落の列で、段落はさらに文字列片の列になっている。

		/// [EN] What this layer stores is plain text, so everything is simply joined back together in order.
		/// [JP] この層が保存するのはプレーンテキストなので、それらを順につなぎ直すだけでよい。
		std::string text;
		if (!document.contains("body") || !document["body"].contains("content"))
		{
			return String(text);
		}
		for (const nlohmann::json& element : document["body"]["content"])
		{
			/// [EN] Elements that are not paragraphs, such as tables, never appear in the documents this layer writes.
			/// [JP] 表などの段落以外の要素は、この層が書くドキュメントには現れない。
			if (!element.contains("paragraph") || !element["paragraph"].contains("elements"))
			{
				continue;
			}
			for (const nlohmann::json& run : element["paragraph"]["elements"])
			{
				/// [EN] A run may instead be a page break or a footnote mark, which carry no text of their own.
				/// [JP] 文字列片の代わりに改ページや脚注の印が入ることがあり、それらは本文を持たない。
				if (run.contains("textRun"))
				{
					text += run["textRun"].value("content", "");
				}
			}
		}
		return String(text);
	}
}
