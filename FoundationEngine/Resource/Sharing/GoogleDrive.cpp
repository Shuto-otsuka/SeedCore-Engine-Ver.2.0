#include <FoundationEngine/Resource/Sharing/GoogleDrive.h>

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
	GoogleDrive::GoogleDrive(HttpClient& http, GoogleAuth& auth) : http_(http), auth_(auth)
	{
		/// No Code
	}

	/**
	* [EN]
	* Nothing to undo: uploads and downloads finish before their call
	* returns, and the transport and sign-in belong to someone else.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 後始末は要らない。アップロードもダウンロードも呼び出しが戻る前に
	* 終わっており、通信手段とログイン情報の持ち主は別にいるため。
	*/
	GoogleDrive::~GoogleDrive()
	{
		/// [EN] An upload session abandoned by a crash is discarded by Drive on its own after about a week.
		/// [JP] クラッシュで放置されたアップロードの窓口は、1週間ほどで Drive 側が自分で破棄する。

		/// No Code
	}

	/**
	* [EN]
	* Returns the identifier of the child of parentId named name, or an
	* empty string when the folder holds no such child.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* parentId の中にある name という名前の子の識別子を返す。該当する
	* 子が無ければ空文字列を返す。
	*/
	String GoogleDrive::Find(const String& name, const String& parentId)
	{
		/// [EN] Drive has no lookup by path, so a search by name within one parent takes its place.
		/// [JP] Drive にはパスで引く仕組みが無いため、親を1つ指定して名前で検索する形で代用する。

		/// [EN] trashed=false keeps a deleted leftover of the same name from being found.
		/// [JP] trashed=false を付けるのは、同名の削除済みファイルが見つからないようにするため。
		String query = String(std::format("name = '{}' and '{}' in parents and trashed = false", EscapeQuery(name).str(), EscapeQuery(parentId).str()));

		/// [EN] Only the identifier is wanted back, and one match is enough since names are unique in these folders.
		/// [JP] 欲しいのは識別子だけ。これらのフォルダでは名前が一意なので、1件見つかれば十分。
		nlohmann::json result;
		if (!Send(String("GET"), String(std::format("https://www.googleapis.com/drive/v3/files?q={}&fields=files(id)&pageSize=1", HttpClient::Escape(query).str())), nlohmann::json(), result))
		{
			return String();
		}

		/// [EN] An empty list is a normal answer: it simply means this content has not been uploaded yet.
		/// [JP] 空の一覧も正常な応答で、この中身がまだアップロードされていないというだけのこと。
		if (!result.contains("files") || result["files"].empty())
		{
			return String();
		}
		return String(result["files"][0].value("id", ""));
	}

	/**
	* [EN]
	* Lists what sits directly inside parentId. Used to see what
	* artists have dropped into the library's inbox.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* parentId の直下にあるものを一覧する。アーティストがライブラリの
	* 受け取り用フォルダへ置いたものを見るために使う。
	*/
	Bool GoogleDrive::List(const String& parentId, DynamicArray<DriveEntry>& entries)
	{
		/// [EN] A folder of a shared library holds tens of entries at most, so one page is enough and no paging is kept.
		/// [JP] 共有ライブラリのフォルダに入る項目はせいぜい数十なので、1ページで足り、続きの管理は持たない。
		/// [EN] The hash is asked for here so that deciding whether a file is already held costs no download.
		/// [JP] ここでハッシュまで求めておくことで、「既に持っているか」の判断にダウンロードが要らなくなる。
		String query = String(std::format("'{}' in parents and trashed = false", EscapeQuery(parentId).str()));
		nlohmann::json result;
		if (!Send(String("GET"), String(std::format("https://www.googleapis.com/drive/v3/files?q={}&fields=files(id,name,mimeType,size,sha256Checksum)&pageSize=200", HttpClient::Escape(query).str())), nlohmann::json(), result))
		{
			return false;
		}

		for (const nlohmann::json& file : result["files"])
		{
			DriveEntry entry;
			entry.id_ = String(file.value("id", ""));
			entry.name_ = String(file.value("name", ""));

			/// [EN] Folders are told apart by their type rather than by their name, since a file may be named like one.
			/// [JP] フォルダかどうかは名前ではなく種類で判断する。フォルダのような名前のファイルもあり得るため。
			entry.folder_ = file.value("mimeType", "") == "application/vnd.google-apps.folder";
			entry.hash_ = String(file.value("sha256Checksum", ""));

			/// [EN] Drive reports the size as text, since it does not fit a JSON number for very large files.
			/// [JP] Drive はサイズを文字列で返す。非常に大きいファイルでは JSON の数値に収まらないため。
			entry.size_ = file.contains("size") ? std::strtoull(file["size"].get<std::string>().c_str(), nullptr, 10) : 0;
			entries.push_back(entry);
		}
		return true;
	}

	/**
	* [EN]
	* Moves a file from one folder to another. Used to take an
	* imported file out of the inbox so it is not imported twice.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ファイルをフォルダ間で移す。取り込み済みのファイルを受け取り用
	* フォルダから出し、二度取り込まないようにするために使う。
	*/
	Bool GoogleDrive::Move(const String& fileId, const String& fromParentId, const String& toParentId)
	{
		/// [EN] A Drive file can belong to several folders at once, so a move is stated as one parent added and one removed.
		/// [JP] Drive のファイルは複数のフォルダに同時に属せるため、移動は「親を1つ足して1つ外す」として表す。
		nlohmann::json result;
		return Send(String("PATCH"), String(std::format("https://www.googleapis.com/drive/v3/files/{}?addParents={}&removeParents={}&fields=id", fileId.str(), toParentId.str(), fromParentId.str())), nlohmann::json::object(), result);
	}

	/**
	* [EN]
	* Creates a folder named name inside parentId and returns its
	* identifier.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* parentId の中に name という名前のフォルダを作り、その識別子を
	* 返す。
	*/
	String GoogleDrive::CreateFolder(const String& name, const String& parentId)
	{
		/// [EN] In Drive a folder is an ordinary file that happens to carry the folder type.
		/// [JP] Drive ではフォルダも普通のファイルで、種類がフォルダになっているだけ。
		nlohmann::json request;
		request["name"] = name.str();
		request["mimeType"] = "application/vnd.google-apps.folder";

		/// [EN] Parents is a list because a Drive file can sit in several folders at once.
		/// [JP] 親が配列なのは、Drive のファイルが複数のフォルダに同時に属せるため。
		request["parents"] = nlohmann::json::array({ parentId.str() });

		nlohmann::json result;
		if (!Send(String("POST"), String("https://www.googleapis.com/drive/v3/files?fields=id"), request, result))
		{
			return String();
		}
		return String(result.value("id", ""));
	}

	/**
	* [EN]
	* Uploads source into parentId under name and returns the new
	* file's identifier. The transfer is resumable, so a connection
	* dropping midway does not mean starting over.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* source を parentId の中へ name という名前でアップロードし、新しい
	* ファイルの識別子を返す。転送は再開可能なので、途中で接続が切れても
	* 最初からやり直しにはならない。
	*/
	String GoogleDrive::Upload(const std::filesystem::path& source, const String& name, const String& parentId)
	{
		/// [EN] Drive is told the size up front, which is what lets it tell a finished upload from a truncated one.
		/// [JP] 先に大きさを伝えておくことで、Drive 側は完了した転送と途中で切れた転送を区別できる。
		std::error_code errorCode;
		Uint64 size = std::filesystem::file_size(source, errorCode);
		if (errorCode)
		{
			error_ = String(std::format("\"{}\" を読み取れませんでした。", source.string()));
			return String();
		}

		/// [EN] The upload happens in two stages: a session is opened, then the contents are pushed into it.
		/// [JP] アップロードは2段階。まず窓口を開き、その窓口へ中身を送り込む。
		String session = BeginUpload(name, parentId, size);
		if (session.str().empty())
		{
			return String();
		}
		return SendChunks(session, source, size);
	}

	/**
	* [EN]
	* Downloads the file named by fileId to destination, writing it
	* straight to disk rather than holding it in memory.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* fileId のファイルを destination へダウンロードする。メモリに
	* 載せず、ディスクへ直接書き出す。
	*/
	Bool GoogleDrive::Fetch(const String& fileId, const std::filesystem::path& destination)
	{
		String token = auth_.AccessToken();
		if (token.str().empty())
		{
			error_ = auth_.Error();
			return false;
		}

		/// [EN] alt=media is what asks for the file's contents; without it Drive answers with its metadata instead.
		/// [JP] alt=media がファイルの中身を求める指定。これが無いと Drive は代わりに情報の方を返す。
		DynamicArray<HttpHeader> headers;
		headers.push_back(HttpHeader{ String("Authorization"), String(std::format("Bearer {}", token.str())) });
		HttpResponse response = http_.Download(String("GET"), String(std::format("https://www.googleapis.com/drive/v3/files/{}?alt=media", fileId.str())), headers, destination);

		if (response.serverTime_ > 0.0)
		{
			serverTime_ = response.serverTime_;
		}
		if (response.status_ == 200 && response.error_.str().empty())
		{
			error_ = String();
			return true;
		}

		/// [EN] A partly written file would look like a valid asset later, so it is removed before reporting the failure.
		/// [JP] 書きかけのファイルは後から正常なアセットに見えてしまうため、失敗を報告する前に消しておく。
		std::error_code errorCode;
		std::filesystem::remove(destination, errorCode);
		error_ = response.error_.str().empty() ? String(std::format("ファイルの取得に失敗しました ({})。", response.status_)) : response.error_;
		return false;
	}

	/**
	* [EN]
	* Moves the file named by fileId to the owner's trash. Used only to
	* clean up content no revision refers to any more.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* fileId のファイルを、所有者のゴミ箱へ移す。どの Revision からも
	* 参照されなくなった中身の掃除にだけ使う。
	*/
	Bool GoogleDrive::Trash(const String& fileId)
	{
		/// [EN] Trashing rather than deleting leaves a way back if the cleanup turns out to have been wrong.
		/// [JP] 完全削除ではなくゴミ箱へ入れるのは、掃除の判断が誤っていた場合に戻せるようにするため。
		nlohmann::json request;
		request["trashed"] = true;
		nlohmann::json result;
		return Send(String("PATCH"), String(std::format("https://www.googleapis.com/drive/v3/files/{}?fields=id", fileId.str())), request, result);
	}

	/**
	* [EN]
	* Google's own clock, in seconds since the Unix epoch, as of the
	* last request.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近のリクエスト時点における Google 側の時刻（Unixエポックからの
	* 秒数）。
	*/
	Double GoogleDrive::ServerTime()const
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
	const String& GoogleDrive::Error()const
	{
		return error_;
	}

	/**
	* [EN]
	* Sends one authenticated request to the Drive API and parses its
	* JSON answer, recording the server clock and any failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 認証付きのリクエストを Drive API へ1回送り、JSON の応答を解釈
	* する。サーバー側の時刻と、失敗した場合はその内容も記録する。
	*/
	Bool GoogleDrive::Send(const String& method, const String& url, const nlohmann::json& request, nlohmann::json& result)
	{
		/// [EN] Asking for the token here is also what renews it, so callers never deal with expiry themselves.
		/// [JP] ここでトークンを求める動作が更新も兼ねる。そのため呼び出し側が期限を気にすることはない。
		String token = auth_.AccessToken();
		if (token.str().empty())
		{
			error_ = auth_.Error();
			return false;
		}

		DynamicArray<HttpHeader> headers;
		headers.push_back(HttpHeader{ String("Authorization"), String(std::format("Bearer {}", token.str())) });

		/// [EN] A search carries no body, which is why the request object is allowed to be null here.
		/// [JP] 検索ではボディを送らない。request が null であってよいのはそのため。
		DynamicArray<Byte> payload;
		if (!request.is_null())
		{
			headers.push_back(HttpHeader{ String("Content-Type"), String("application/json; charset=utf-8") });
			std::string body = request.dump();
			payload.resize(body.size());
			std::memcpy(payload.data(), body.data(), body.size());
		}

		HttpResponse response = http_.Send(method, url, headers, payload);
		if (response.serverTime_ > 0.0)
		{
			serverTime_ = response.serverTime_;
		}
		if (response.status_ == 0)
		{
			error_ = response.error_;
			return false;
		}

		result = nlohmann::json::parse(response.body_.begin(), response.body_.end(), nullptr, false);
		if (response.status_ == 200)
		{
			error_ = String();
			return result.is_object();
		}

		/// [EN] 403 here usually means the member's own Drive is full, since uploads are charged to whoever sends them.
		/// [JP] ここでの 403 は、たいていメンバー自身の Drive が一杯という意味。アップロードは送った本人の容量から引かれる。
		std::string message = result.is_object() && result.contains("error") ? result["error"].value("message", "") : "";
		error_ = String(std::format("Drive へのアクセスに失敗しました ({}): {}", response.status_, message));
		return false;
	}

	/**
	* [EN]
	* Opens a resumable upload session for a file of size bytes and
	* returns the URL the contents are then sent to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* size バイトのファイル用に再開可能アップロードの窓口を開き、中身を
	* 送り込む先のURLを返す。
	*/
	String GoogleDrive::BeginUpload(const String& name, const String& parentId, Uint64 size)
	{
		String token = auth_.AccessToken();
		if (token.str().empty())
		{
			error_ = auth_.Error();
			return String();
		}

		/// [EN] This first request carries only the description of the file: its name and where it belongs.
		/// [JP] 最初のリクエストが運ぶのはファイルの説明だけ。名前と、どこに属するか。
		nlohmann::json request;
		request["name"] = name.str();
		request["parents"] = nlohmann::json::array({ parentId.str() });
		std::string body = request.dump();
		DynamicArray<Byte> payload(body.size());
		std::memcpy(payload.data(), body.data(), body.size());

		/// [EN] The two X-Upload headers let Drive refuse an oversized file before a single byte of content is sent.
		/// [JP] 2つの X-Upload ヘッダにより、中身を1バイトも送らないうちに、大きすぎるファイルを Drive が断れる。
		DynamicArray<HttpHeader> headers;
		headers.push_back(HttpHeader{ String("Authorization"), String(std::format("Bearer {}", token.str())) });
		headers.push_back(HttpHeader{ String("Content-Type"), String("application/json; charset=utf-8") });
		headers.push_back(HttpHeader{ String("X-Upload-Content-Type"), String("application/octet-stream") });
		headers.push_back(HttpHeader{ String("X-Upload-Content-Length"), String(std::format("{}", size)) });

		HttpResponse response = http_.Send(String("POST"), String("https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable&fields=id"), headers, payload);
		if (response.serverTime_ > 0.0)
		{
			serverTime_ = response.serverTime_;
		}

		/// [EN] The session URL comes back in the Location header, and it already carries its own secret, so it needs no token.
		/// [JP] 窓口のURLは Location ヘッダで返る。URL自体が固有の鍵を含むため、以後トークンは要らない。
		if (response.status_ != 200 || response.location_.str().empty())
		{
			error_ = String(std::format("アップロードを開始できませんでした ({})。", response.status_));
			return String();
		}
		return response.location_;
	}

	/**
	* [EN]
	* Sends source to an upload session piece by piece and returns the
	* identifier of the file Drive ends up with.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* source をアップロードの窓口へ少しずつ送り、最終的に Drive 側に
	* できたファイルの識別子を返す。
	*/
	String GoogleDrive::SendChunks(const String& session, const std::filesystem::path& source, Uint64 size)
	{
		std::ifstream stream(source, std::ios::binary);
		if (!stream)
		{
			error_ = String(std::format("\"{}\" を開けませんでした。", source.string()));
			return String();
		}

		/// [EN] Sending in pieces keeps memory flat and means a break costs only the piece in flight.
		/// [JP] 分割して送ることでメモリの使用量が一定に保たれ、切断で失うのも送信中の1つ分だけで済む。

		/// [EN] Drive requires every piece except the last to be a multiple of 256 KiB; 8 MiB is such a multiple.
		/// [JP] Drive は最後以外の各片が 256KiB の倍数であることを求める。8MiB はその条件を満たす。
		const Uint64 chunkSize = 8 * 1024 * 1024;
		DynamicArray<Byte> chunk;
		Uint64 offset = 0;
		while (offset < size)
		{
			Uint64 remaining = size - offset;
			Uint64 length = remaining < chunkSize ? remaining : chunkSize;
			chunk.resize(static_cast<Size>(length));
			stream.read(chunk.data(), static_cast<std::streamsize>(length));

			/// [EN] Content-Range states which slice of the whole file this piece is, so Drive can reassemble it.
			/// [JP] Content-Range は、この片がファイル全体のどの範囲かを示す。Drive はこれを見て組み立て直す。
			DynamicArray<HttpHeader> headers;
			headers.push_back(HttpHeader{ String("Content-Range"), String(std::format("bytes {}-{}/{}", offset, offset + length - 1, size)) });
			HttpResponse response = http_.Send(String("PUT"), session, headers, chunk);
			if (response.serverTime_ > 0.0)
			{
				serverTime_ = response.serverTime_;
			}

			/// [EN] 308 means the piece landed and Drive is waiting for more; it is the normal answer mid-transfer.
			/// [JP] 308 は「この片は届いた、続きを待っている」の意味。転送の途中では、これが普通の応答。
			if (response.status_ == 308)
			{
				offset += length;
				continue;
			}

			/// [EN] The last piece is answered with the finished file's description instead.
			/// [JP] 最後の片に対しては、代わりに完成したファイルの情報が返る。
			if (response.status_ == 200 || response.status_ == 201)
			{
				nlohmann::json result = nlohmann::json::parse(response.body_.begin(), response.body_.end(), nullptr, false);
				error_ = String();
				return String(result.is_object() ? result.value("id", "") : "");
			}

			error_ = String(std::format("アップロードの途中で失敗しました ({})。", response.status_));
			return String();
		}

		/// [EN] Reaching here means the file ended before Drive reported completion, so nothing usable was stored.
		/// [JP] ここへ来るのは、Drive が完了を告げる前にファイルが尽きた場合。使える形では保存されていない。
		error_ = String("アップロードが完了しませんでした。");
		return String();
	}

	/**
	* [EN]
	* Escapes the quotes and backslashes that would otherwise break out
	* of a string literal inside a Drive search query.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Drive の検索クエリでは、引用符と逆斜線がそのままだと文字列リテラル
	* を抜け出してしまうため、それらを打ち消す。
	*/
	String GoogleDrive::EscapeQuery(const String& value)
	{
		/// [EN] Names here are content hashes and folder identifiers, so this never fires in practice; it is a guard, not a feature.
		/// [JP] ここで扱う名前は内容のハッシュとフォルダの識別子なので実際には作動しない。機能ではなく用心のための処理。
		std::string source = value.str();
		std::string result;
		result.reserve(source.size());
		for (Char character : source)
		{
			if (character == '\\' || character == '\'')
			{
				result += '\\';
			}
			result += character;
		}
		return String(result);
	}
}
