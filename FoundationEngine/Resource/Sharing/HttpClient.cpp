#include <FoundationEngine/Resource/Sharing/HttpClient.h>

namespace SeedCore
{
	/**
	* [EN]
	* Opens the WinHTTP session and applies the timeouts every sharing
	* request uses.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WinHTTP セッションを開き、共有機能の全リクエストで使うタイムアウトを
	* 設定する。
	*/
	HttpClient::HttpClient()
	{
		/// [EN] The session is the outermost WinHTTP object: it holds settings and reuses connections across requests.
		/// [JP] セッションは WinHTTP の一番外側の入れ物で、設定を保持し、リクエストをまたいで接続を使い回す。

		/// [EN] The first argument is the user agent, the name this program gives when it introduces itself to a server.
		/// [JP] 第1引数はユーザーエージェント。このプログラムがサーバーへ名乗る名前にあたる。

		/// [EN] AUTOMATIC_PROXY follows whatever proxy Windows is configured to use, which school networks often require.
		/// [JP] AUTOMATIC_PROXY は Windows のプロキシ設定に従う指定。学校のネットワークではこれが要ることが多い。
		session_ = WinHttpOpen(L"SeedCore/2.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (session_)
		{
			/// [EN] The four values are, in order: name lookup, connecting, sending, and waiting for the answer.
			/// [JP] 4つの値は順に、名前解決・接続・送信・応答待ちにかける上限時間。

			/// [EN] Only the last is generous, because a large asset download keeps the connection busy for a while.
			/// [JP] 最後だけ長いのは、大きいアセットのダウンロードでは接続がしばらく塞がるため。
			WinHttpSetTimeouts(session_, 15000, 15000, 30000, 120000);
		}
	}

	/**
	* [EN]
	* Closes the WinHTTP session.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WinHTTP セッションを閉じる。
	*/
	HttpClient::~HttpClient()
	{
		/// [EN] Every per-request handle is already closed inside Send, so only the session itself is left here.
		/// [JP] リクエストごとのハンドルは Send の中で閉じ終えているため、ここに残るのはセッション本体だけ。
		if (session_)
		{
			WinHttpCloseHandle(session_);
		}
	}

	/**
	* [EN]
	* Sends one request and reads the whole response into memory.
	* method is an uppercase verb such as "GET" or "POST", url must be
	* absolute, and body may be empty for verbs that carry none.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リクエストを1回送り、レスポンス全体をメモリへ読み込む。method は
	* "GET" や "POST" のような大文字の動詞、url は絶対URL。ボディを持た
	* ない動詞では body は空でよい。
	*/
	HttpResponse HttpClient::Send(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const DynamicArray<Byte>& body)
	{
		/// [EN] Passing no destination is what makes the exchange keep the body in memory.
		/// [JP] 書き出し先を渡さないことが、本文をメモリに保持するという指定になる。
		return Exchange(method, url, headers, body, nullptr);
	}

	/**
	* [EN]
	* Sends a request and writes the response body straight to
	* destination instead of holding it in memory, so an asset of any
	* size can be fetched. The body of the returned response is empty
	* on success and carries the server's message on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リクエストを送り、レスポンスの本文をメモリに溜めず destination へ
	* 直接書き出す。どんな大きさのアセットでも取得できる。返り値の本文
	* は成功時は空で、失敗時はサーバーのメッセージが入る。
	*/
	HttpResponse HttpClient::Download(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const std::filesystem::path& destination)
	{
		/// [EN] The folder is created first, because a download often lands in a directory the workspace does not have yet.
		/// [JP] 先にフォルダを作る。ダウンロード先は、ワークスペースにまだ無いディレクトリであることが多いため。
		HttpResponse response;
		std::error_code errorCode;
		std::filesystem::create_directories(destination.parent_path(), errorCode);

		std::ofstream stream(destination, std::ios::binary);
		if (!stream)
		{
			response.error_ = String(std::format("\"{}\" を書き込み用に開けませんでした。", destination.string()));
			return response;
		}
		return Exchange(method, url, headers, DynamicArray<Byte>(), &stream);
	}

	/**
	* [EN]
	* Performs one exchange. destination, when given, receives the
	* response body as it arrives; otherwise the body is collected in
	* memory. Send and Download are both thin wrappers over this.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* やり取りを1回行う。destination が与えられていれば、届いた本文を
	* そこへ書き出し、無ければメモリへ溜める。Send と Download は
	* どちらもこれを薄く包んだもの。
	*/
	HttpResponse HttpClient::Exchange(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const DynamicArray<Byte>& body, std::ofstream* destination)
	{
		/// [EN] The result is filled in as the exchange proceeds and returned as-is at every early exit.
		/// [JP] 結果はやり取りを進めながら埋めていき、途中で抜ける場合もそのまま返す。
		HttpResponse response;
		if (!session_)
		{
			/// [EN] status_ stays 0, which is how callers tell "never reached the server" from a real HTTP status.
			/// [JP] status_ は 0 のまま。呼び出し側はこれで「サーバーへ届いていない」と本物のHTTPステータスを区別する。
			response.error_ = String("WinHTTP セッションを利用できません。");
			return response;
		}

		/// [EN] WinHTTP wants the host, the path and the query separately, so the URL is taken apart first.
		/// [JP] WinHTTP はホスト・パス・クエリを別々に要求するため、まずURLを分解する。

		/// [EN] WinHttpCrackUrl writes into buffers the caller provides, so those buffers are made up front.
		/// [JP] WinHttpCrackUrl は呼び出し側が用意したバッファへ書き込むため、先にその領域を作っておく。
		std::wstring wideUrl = url.w_str();
		std::wstring host(256, L'\0');
		std::wstring path(4096, L'\0');
		std::wstring extra(2048, L'\0');

		/// [EN] dwStructSize is how this API checks which version of the structure it was handed.
		/// [JP] dwStructSize は、このAPIが「どの版の構造体を渡されたか」を判断するための項目。
		URL_COMPONENTS components{};
		components.dwStructSize = sizeof(components);

		/// [EN] Each pointer plus length pair tells the call where to put that part of the URL and how much room there is.
		/// [JP] ポインタと長さの組は、URLのその部分をどこへ、どれだけの余裕で書いてよいかを伝える。
		components.lpszHostName = host.data();
		components.dwHostNameLength = static_cast<DWORD>(host.size());
		components.lpszUrlPath = path.data();
		components.dwUrlPathLength = static_cast<DWORD>(path.size());
		components.lpszExtraInfo = extra.data();
		components.dwExtraInfoLength = static_cast<DWORD>(extra.size());

		/// [EN] A failure here means the text was not a usable absolute URL at all.
		/// [JP] ここで失敗するのは、そもそも使える絶対URLの文字列ではなかった場合。
		if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
		{
			response.error_ = String(std::format("URL を解釈できませんでした: {}", url.str()));
			return response;
		}

		/// [EN] Connecting names only the host and the port; nothing is sent yet at this point.
		/// [JP] 接続で指定するのはホストとポートだけ。この時点ではまだ何も送っていない。
		HINTERNET connection = WinHttpConnect(session_, components.lpszHostName, components.nPort, 0);
		if (!connection)
		{
			response.error_ = String(std::format("{} へ接続できませんでした。", String(std::wstring(components.lpszHostName, components.dwHostNameLength)).str()));
			return response;
		}

		/// [EN] The path and the query are joined back together, because the request wants them as one target string.
		/// [JP] パスとクエリをつなぎ直す。リクエスト側は両者を1つの文字列として受け取るため。
		std::wstring target = std::wstring(components.lpszUrlPath, components.dwUrlPathLength) + std::wstring(components.lpszExtraInfo, components.dwExtraInfoLength);

		/// [EN] SECURE switches TLS on; every Google endpoint this layer talks to is https, so this is the normal path.
		/// [JP] SECURE を付けると TLS が有効になる。この層が話す Google のエンドポイントは全て https なので常にこちら。
		DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;

		/// [EN] Opening a request builds the message; it is still only prepared, not sent.
		/// [JP] リクエストを開く操作はメッセージを組み立てるだけで、まだ送信はしない。
		HINTERNET request = WinHttpOpenRequest(connection, method.w_str().c_str(), target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!request)
		{
			WinHttpCloseHandle(connection);
			response.error_ = String("HTTP リクエストを作成できませんでした。");
			return response;
		}

		/// [EN] Sending hands over the headers and the body together.
		/// [JP] 送信ではヘッダとボディをまとめて渡す。

		/// [EN] The body length appears twice because WinHTTP also supports sending a body in several pieces.
		/// [JP] ボディの長さを2回渡すのは、WinHTTP がボディを何回かに分けて送る使い方も許しているため。
		std::wstring headerBlock = BuildHeaders(headers);
		Bool sent = WinHttpSendRequest(request, headerBlock.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBlock.c_str(), headerBlock.empty() ? 0 : static_cast<DWORD>(headerBlock.size()), body.empty() ? nullptr : const_cast<Byte*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) != FALSE;

		/// [EN] Receiving waits for the status line and the headers; the body is read afterwards.
		/// [JP] 受信の待機で得られるのはステータス行とヘッダまで。ボディはその後に読む。
		if (!sent || !WinHttpReceiveResponse(request, nullptr))
		{
			/// [EN] Reaching here means the network or TLS failed, so there is no HTTP status to report at all.
			/// [JP] ここへ来るのは通信か TLS の失敗で、報告できるHTTPステータス自体が存在しない。
			response.error_ = String(std::format("応答が届く前にリクエストが失敗しました (Windows エラー {})。", GetLastError()));
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connection);
			return response;
		}

		/// [EN] QUERY_FLAG_NUMBER asks for the status as a number rather than as the text "200".
		/// [JP] QUERY_FLAG_NUMBER は、ステータスを "200" という文字列ではなく数値で受け取る指定。
		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
		response.status_ = status;

		/// [EN] The server's clock is captured on every exchange; lease deadlines are expressed against it.
		/// [JP] やり取りのたびにサーバー側の時刻を拾っておく。Lease の期限はこれを基準に表す。
		response.serverTime_ = ReadServerTime(request);

		/// [EN] Location matters only for a resumable upload, where it names the session to send the file to.
		/// [JP] Location が意味を持つのは再開可能アップロードのときだけで、ファイルを送る先の窓口を示す。
		response.location_ = ReadLocation(request);

		/// [EN] A failed download still answers with a short JSON explanation, which is worth keeping in memory.
		/// [JP] 失敗したダウンロードも短い JSON の説明を返すため、それはメモリへ残す価値がある。
		Bool toFile = destination != nullptr && response.status_ == 200;

		/// [EN] The body arrives in chunks of unpredictable size, so it is taken one chunk at a time.
		/// [JP] ボディは大きさの読めない塊で届くため、塊ごとに受け取っていく。
		DynamicArray<Byte> chunk;
		DWORD available = 0;
		while (WinHttpQueryDataAvailable(request, &available) && available > 0)
		{
			/// [EN] Room is made for the announced amount, either in the scratch chunk or at the end of the kept body.
			/// [JP] 予告された分の場所を空ける。書き出す場合は作業用の塊に、溜める場合は本文の末尾に。
			Size offset = toFile ? 0 : response.body_.size();
			DynamicArray<Byte>& buffer = toFile ? chunk : response.body_;
			buffer.resize(offset + available);
			DWORD read = 0;
			if (!WinHttpReadData(request, buffer.data() + offset, available, &read))
			{
				/// [EN] A partial body is kept rather than discarded, since it often carries the server's error message.
				/// [JP] 途中までのボディも捨てずに残す。そこにサーバー側のエラーメッセージが入っていることが多いため。
				response.error_ = String("応答の本文を最後まで読み取れませんでした。");
				break;
			}

			/// [EN] A read may return less than was announced, so only what actually arrived is used.
			/// [JP] 読み取りは予告より少なく返ることがあるため、実際に届いた分だけを使う。
			buffer.resize(offset + read);
			if (toFile)
			{
				/// [EN] Writing each chunk straight out is what keeps a multi-gigabyte asset from sitting in memory.
				/// [JP] 塊ごとにそのまま書き出すことで、数ギガバイトのアセットをメモリに載せずに済む。
				destination->write(buffer.data(), buffer.size());
			}
		}

		/// [EN] Both handles are closed in the reverse order they were opened; the session itself stays alive.
		/// [JP] 2つのハンドルを開いた順と逆に閉じる。セッション自体はそのまま生かしておく。
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connection);
		return response;
	}

	/**
	* [EN]
	* Sends a request without a body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディなしでリクエストを送る。
	*/
	HttpResponse HttpClient::Send(const String& method, const String& url, const DynamicArray<HttpHeader>& headers)
	{
		/// [EN] An empty body is a valid body, so this just names the common case for readability.
		/// [JP] 空のボディも正しいボディなので、これはよくある呼び方に名前を付けているだけ。
		return Send(method, url, headers, DynamicArray<Byte>());
	}

	/**
	* [EN]
	* Whether the WinHTTP session was created successfully. Every Send
	* fails immediately while this is false.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WinHTTP セッションの作成に成功しているか。false の間は Send が
	* すべて即座に失敗する。
	*/
	Bool HttpClient::Ready()const
	{
		return session_ != nullptr;
	}

	/**
	* [EN]
	* Percent-encodes value so it can sit inside a URL query or a form
	* body without being read as punctuation of the query itself.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* URLのクエリやフォーム本文の中に置いても、クエリ自体の記号として
	* 読まれないよう value をパーセントエンコードする。
	*/
	String HttpClient::Escape(const String& value)
	{
		/// [EN] Characters such as ':' '/' and ' ' would otherwise be read as query structure rather than as part of a value.
		/// [JP] ':' '/' ' ' などをそのまま置くと、値の一部ではなくクエリの構造として読まれてしまう。

		/// [EN] Three bytes per character is the worst case, so reserving that much avoids repeated regrowth.
		/// [JP] 1文字あたり最大3バイトになるため、その分を先に確保して伸ばし直しを避ける。
		std::string source = value.str();
		std::string result;
		result.reserve(source.size() * 3);
		for (Char character : source)
		{
			/// [EN] The unreserved set of RFC 3986 is the only part that may be written as-is.
			/// [JP] そのまま書いてよいのは RFC 3986 の unreserved 集合だけ。
			Bool unreserved = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' || character == '~';
			if (unreserved)
			{
				result += character;
			}
			else
			{
				/// [EN] Everything else becomes a percent sign followed by the byte written as two hexadecimal digits.
				/// [JP] それ以外は、パーセント記号に続けてそのバイトを16進2桁で書いた形になる。
				result += std::format("%{:02X}", static_cast<Uint8>(character));
			}
		}
		return String(result);
	}

	/**
	* [EN]
	* Reverses Escape: turns the %XX sequences of a URL query value back
	* into the bytes they stand for.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Escape の逆。URLのクエリ値に含まれる %XX の並びを、元のバイトへ
	* 戻す。
	*/
	String HttpClient::Unescape(const String& value)
	{
		/// [EN] Authorization codes routinely contain '/', which the browser sends back as %2F.
		/// [JP] 認可コードにはよく '/' が含まれ、ブラウザはそれを %2F として返してくる。
		std::string source = value.str();
		std::string result;
		result.reserve(source.size());
		for (Size index = 0; index < source.size(); ++index)
		{
			/// [EN] A percent sign introduces two hexadecimal digits, which together name one byte.
			/// [JP] パーセント記号に続く16進2桁が、1バイトを表している。
			if (source[index] == '%' && index + 2 < source.size())
			{
				result += static_cast<Char>(std::strtoul(source.substr(index + 1, 2).c_str(), nullptr, 16));
				index += 2;
			}
			else if (source[index] == '+')
			{
				/// [EN] In a query string a plus sign is an older way of writing a space.
				/// [JP] クエリ文字列における '+' は、空白の古い書き方。
				result += ' ';
			}
			else
			{
				result += source[index];
			}
		}
		return String(result);
	}

	/**
	* [EN]
	* Returns the Location response header of request, or an empty
	* string when it carries none.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* request の Location レスポンスヘッダを返す。持っていない場合は
	* 空文字列。
	*/
	String HttpClient::ReadLocation(HINTERNET request)
	{
		/// [EN] The call is made twice: the first asks how much room the value needs, the second fetches it.
		/// [JP] 呼び出しは2回行う。1回目で値に必要な長さを尋ね、2回目で実際に受け取る。
		DWORD size = 0;
		WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
		if (size == 0)
		{
			/// [EN] Most answers have no Location at all, so this is the ordinary path rather than a failure.
			/// [JP] Location を持たない応答がほとんどなので、これは失敗ではなく普通の経路。
			return String();
		}

		/// [EN] The size is in bytes and includes the terminator, while the buffer counts wide characters.
		/// [JP] 返る長さはバイト単位で終端を含む一方、バッファは文字単位で数えるため割って使う。
		std::wstring value(size / sizeof(wchar_t), L'\0');
		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size, WINHTTP_NO_HEADER_INDEX))
		{
			return String();
		}

		/// [EN] The terminator is trimmed so the value can be used as a plain URL.
		/// [JP] 終端文字を落として、そのまま URL として使える形にする。
		return String(std::wstring(value.c_str()));
	}

	/**
	* [EN]
	* Converts the Date response header of request into seconds since
	* the Unix epoch, or 0 when the header is missing or unparsable.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* request の Date レスポンスヘッダをUnixエポックからの秒数へ変換
	* する。ヘッダが無い、または解釈できない場合は 0。
	*/
	Double HttpClient::ReadServerTime(HINTERNET request)
	{
		/// [EN] The Date header is text such as "Tue, 23 Sep 2026 12:34:56 GMT"; the SYSTEMTIME flag has WinHTTP parse it for us.
		/// [JP] Date ヘッダは "Tue, 23 Sep 2026 12:34:56 GMT" のような文字列。SYSTEMTIME 指定で WinHTTP に解釈させる。
		SYSTEMTIME systemTime{};
		DWORD size = sizeof(systemTime);
		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_DATE | WINHTTP_QUERY_FLAG_SYSTEMTIME, WINHTTP_HEADER_NAME_BY_INDEX, &systemTime, &size, WINHTTP_NO_HEADER_INDEX))
		{
			/// [EN] Not every answer carries a Date; the caller keeps the previous reading in that case.
			/// [JP] Date を持たない応答もある。その場合、呼び出し側は前回の値を保つ。
			return 0.0;
		}

		/// [EN] SYSTEMTIME is a calendar breakdown, so it is turned into a single counter before any arithmetic.
		/// [JP] SYSTEMTIME は年月日時分秒に分かれた形なので、計算の前に1つの通し数値へ直す。
		FILETIME fileTime{};
		if (!SystemTimeToFileTime(&systemTime, &fileTime))
		{
			return 0.0;
		}

		/// [EN] FILETIME is a 64-bit count split across two 32-bit halves, so the halves are rejoined here.
		/// [JP] FILETIME は64ビットの値を32ビット2つに分けて持つため、ここで元の1つへ戻す。
		Uint64 ticks = (static_cast<Uint64>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;

		/// [EN] It counts 100ns ticks from 1601-01-01, while the rest of the engine counts seconds from 1970-01-01.
		/// [JP] FILETIME は 1601-01-01 からの100ナノ秒刻み、エンジン側は 1970-01-01 からの秒数を使う。
		return static_cast<Double>(ticks) / 10000000.0 - 11644473600.0;
	}

	/**
	* [EN]
	* Builds the CRLF-separated header block WinHttpSendRequest takes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WinHttpSendRequest が受け取る、CRLF区切りのヘッダ列を組み立てる。
	*/
	std::wstring HttpClient::BuildHeaders(const DynamicArray<HttpHeader>& headers)
	{
		/// [EN] HTTP writes headers as "Name: value" lines, and that raw form is exactly what WinHTTP expects here.
		/// [JP] HTTP はヘッダを "名前: 値" の行として書く。WinHTTP がここで求めるのも、その生の形そのもの。
		std::wstring block;
		for (const HttpHeader& header : headers)
		{
			block += header.name_.w_str() + L": " + header.value_.w_str() + L"\r\n";
		}
		return block;
	}
}
