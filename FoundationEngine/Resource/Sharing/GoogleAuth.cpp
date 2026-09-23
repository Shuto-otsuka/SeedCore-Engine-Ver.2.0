#include <FoundationEngine/Resource/Sharing/GoogleAuth.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	/**
	* [EN]
	* Takes the transport and the client credentials, then loads whatever
	* refresh token a previous session left on disk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 通信手段とクライアント情報を受け取り、前回の起動がディスクへ残した
	* 更新用トークンがあれば読み込む。
	*/
	GoogleAuth::GoogleAuth(HttpClient& http, const std::filesystem::path& tokenPath, const String& clientId, const String& clientSecret) : http_(http), tokenPath_(tokenPath), clientId_(clientId), clientSecret_(clientSecret)
	{
		/// [EN] Loading here means the Editor already knows at startup whether this member still needs to sign in.
		/// [JP] ここで読み込んでおくことで、Editor は起動時点で「このメンバーはログインが要るか」を把握できる。
		LoadToken();
	}

	/**
	* [EN]
	* Nothing is undone here. The refresh token stays on disk so the next
	* run starts signed in, and the access token simply expires on its own
	* within the hour.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ここで後始末することは無い。更新用トークンは次回の起動がログイン済み
	* で始められるようディスクへ残し、アクセストークンは1時間以内に自然に
	* 期限切れになる。
	*/
	GoogleAuth::~GoogleAuth()
	{
		/// [EN] Signing out is a deliberate action by the member, never something that happens on shutdown.
		/// [JP] ログアウトはメンバーが意図して行う操作で、終了時に自動で起きるものではない。

		/// No Code
	}

	/**
	* [EN]
	* Whether a refresh token is held, meaning this member has signed in
	* at some point and no re-authorization is needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 更新用トークンを保持しているか。保持していれば、このメンバーは
	* 一度ログイン済みで、再認可は要らない。
	*/
	Bool GoogleAuth::SignedIn()const
	{
		/// [EN] The refresh token is the lasting half of a sign-in, so holding one is what "signed in" means here.
		/// [JP] ログインのうち長く残るのが更新用トークンなので、それを持っていることが「ログイン済み」の意味になる。
		return !refreshToken_.str().empty();
	}

	/**
	* [EN]
	* Opens the browser for consent and waits for the redirect on a
	* loopback port. Blocks until the member finishes or the wait times
	* out, and returns whether a refresh token was obtained.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 同意のためにブラウザを開き、ループバックのポートでリダイレクトを
	* 待つ。メンバーの操作が終わるか待ち時間が尽きるまで戻らず、更新用
	* トークンを得られたかを返す。
	*/
	Bool GoogleAuth::SignIn()
	{
		/// [EN] Google never hands the result to this program directly; it redirects the browser to an address we name.
		/// [JP] Google は結果をこのプログラムへ直接渡さず、こちらが指定したアドレスへブラウザをリダイレクトさせる。

		/// [EN] For a desktop application that address has to be a port on this machine, so a temporary listener is opened.
		/// [JP] デスクトップアプリではそのアドレスがこの PC 上のポートである必要があるため、一時的な受け口を開く。

		/// [EN] Winsock has to be started before any socket call, and stopped again when we are done with it.
		/// [JP] Winsock はソケットを使う前に開始し、使い終えたら停止する必要がある。
		WSADATA winsock{};
		if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
		{
			error_ = String("Windows のネットワーク機能を初期化できませんでした。");
			return false;
		}

		/// [EN] A plain TCP socket is enough: the browser will speak ordinary HTTP to it exactly once.
		/// [JP] 普通の TCP ソケットで足りる。ブラウザはここへ普通の HTTP を1回だけ話しに来る。
		SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener == INVALID_SOCKET)
		{
			WSACleanup();
			error_ = String("ログイン受け取り用のソケットを作成できませんでした。");
			return false;
		}

		/// [EN] INADDR_LOOPBACK restricts the listener to this machine, so nothing on the network can reach it.
		/// [JP] INADDR_LOOPBACK にすると、この受け口はこの PC の中だけに開かれ、ネットワークからは届かない。

		/// [EN] Port 0 means "any free port"; which one was actually taken is read back with getsockname.
		/// [JP] ポート0は「空いているものを任せる」指定。実際に割り当てられた番号は getsockname で読み取る。
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		Int addressSize = sizeof(address);

		/// [EN] Bind claims the address, listen starts accepting, and the backlog of 1 is all a single redirect needs.
		/// [JP] bind で番号を確保し、listen で受け付けを始める。待ち行列は1つあれば、1回のリダイレクトには十分。
		if (bind(listener, reinterpret_cast<sockaddr*>(&address), addressSize) == SOCKET_ERROR || listen(listener, 1) == SOCKET_ERROR || getsockname(listener, reinterpret_cast<sockaddr*>(&address), &addressSize) == SOCKET_ERROR)
		{
			closesocket(listener);
			WSACleanup();
			error_ = String("ログインの戻り先となるポートを用意できませんでした。");
			return false;
		}

		/// [EN] ntohs converts the port from network byte order into the order this machine uses.
		/// [JP] ntohs は、ポート番号をネットワーク上の並びから、この PC が使う並びへ直す。
		Uint16 port = ntohs(address.sin_port);

		/// [EN] This same address is sent to Google now and checked again when the tokens are requested; the two must match exactly.
		/// [JP] このアドレスは今 Google へ送り、トークン要求時にも再度照合される。両者は完全に一致する必要がある。
		String redirect = String(std::format("http://127.0.0.1:{}/", port));

		/// [EN] scope lists exactly what the Editor will do: all of Drive for shared files, Docs for the arbiter, and the address that identifies the member.
		/// [JP] scope は Editor が行うことの一覧。共有ファイルのための Drive 全体、判定役のための Docs、メンバー識別のためのアドレス。
		String scope = String("openid email https://www.googleapis.com/auth/drive https://www.googleapis.com/auth/documents");

		/// [EN] access_type=offline is what makes Google return a refresh token; without it the member would sign in every hour.
		/// [JP] 更新用トークンが返るのは access_type=offline を付けているため。これが無いとメンバーは毎時ログインすることになる。

		/// [EN] prompt=consent forces the consent screen, so a re-sign-in always yields a fresh refresh token rather than none.
		/// [JP] prompt=consent で同意画面を必ず出す。こうすると再ログイン時にも更新用トークンが必ず返る。

		/// [EN] Every value is percent-encoded, because a raw ':' or '/' would be read as query structure instead of as content.
		/// [JP] 各値をパーセントエンコードする。生の ':' や '/' は値ではなくクエリの構造として読まれてしまうため。
		String authorizeUrl = String(std::format("https://accounts.google.com/o/oauth2/v2/auth?client_id={}&redirect_uri={}&response_type=code&access_type=offline&prompt=select_account%20consent&scope={}", HttpClient::Escape(clientId_).str(), HttpClient::Escape(redirect).str(), HttpClient::Escape(scope).str()));

		/// [EN] Handing the URL to the shell opens the member's usual browser, where they are often already signed in to Google.
		/// [JP] URLをシェルに渡すと、メンバーが普段使うブラウザで開く。そこでは Google に既にログイン済みのことが多い。
		ShellExecuteW(nullptr, L"open", authorizeUrl.w_str().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

		/// [EN] From here the program simply waits; the member is choosing an account and approving in the browser.
		/// [JP] ここから先はただ待つだけ。メンバーはブラウザでアカウントを選び、許可の操作をしている。
		String code = ReceiveCode(listener);

		/// [EN] The listener has done its one job, so it and Winsock are shut down before the network call below.
		/// [JP] 受け口の役目は終わったので、下の通信を行う前に受け口と Winsock を畳む。
		closesocket(listener);
		WSACleanup();
		if (code.str().empty())
		{
			return false;
		}

		/// [EN] The code is single-use and short-lived; it is traded here for the access and refresh tokens.
		/// [JP] 認可コードは一度きりで短命。ここでアクセストークンと更新用トークンに交換する。

		/// [EN] The redirect address is repeated so Google can confirm the code is being redeemed by whoever it was issued to.
		/// [JP] リダイレクト先を再度送るのは、そのコードが発行先本人によって使われていることを Google が確認するため。
		return RequestToken(String(std::format("code={}&client_id={}&client_secret={}&redirect_uri={}&grant_type=authorization_code", HttpClient::Escape(code).str(), HttpClient::Escape(clientId_).str(), HttpClient::Escape(clientSecret_).str(), HttpClient::Escape(redirect).str())));
	}

	/**
	* [EN]
	* Forgets the stored refresh token and deletes its file, so the next
	* use requires signing in again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 保持している更新用トークンを破棄してファイルも削除する。次に使う
	* ときは再度のログインが必要になる。
	*/
	void GoogleAuth::SignOut()
	{
		/// [EN] Both tokens are dropped from memory, and the deadline with them so nothing looks momentarily valid.
		/// [JP] 2つのトークンをメモリから捨て、期限も戻す。一瞬でも有効に見える状態を残さないため。
		refreshToken_ = String();
		accessToken_ = String();
		expiresAt_ = 0.0;

		/// [EN] The file goes too; a missing file is the normal state for a member who has never signed in.
		/// [JP] ファイルも消す。ファイルが無い状態は、一度もログインしていないメンバーの通常の姿。
		std::error_code errorCode;
		std::filesystem::remove(tokenPath_, errorCode);
	}

	/**
	* [EN]
	* Returns a usable access token, renewing it first when it has
	* expired or is about to. Empty when the member must sign in again.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 使用可能なアクセストークンを返す。期限切れか期限間近なら先に更新
	* する。再ログインが必要な場合は空を返す。
	*/
	String GoogleAuth::AccessToken()
	{
		/// [EN] Without a refresh token nothing can be renewed, so the caller is told to have the member sign in.
		/// [JP] 更新用トークンが無ければ何も更新できないので、呼び出し側にはログインを促してもらう。
		if (!SignedIn())
		{
			error_ = String("Google へのログインが必要です。");
			return String();
		}

		/// [EN] Access tokens last about an hour, so most calls fall through this check and reuse the one in hand.
		/// [JP] アクセストークンの寿命はおよそ1時間なので、ほとんどの呼び出しはこの判定を素通りして手持ちを使い回す。
		Double now = std::chrono::duration<Double>(std::chrono::system_clock::now().time_since_epoch()).count();
		if (accessToken_.str().empty() || now >= expiresAt_)
		{
			/// [EN] Renewing is a network round trip, which is why it is done here rather than before every single call.
			/// [JP] 更新は通信を1往復伴う。毎回の呼び出し前ではなくここで行っているのはそのため。
			if (!Refresh())
			{
				return String();
			}
		}
		return accessToken_;
	}

	/**
	* [EN]
	* The last failure, if any, in a form that can be shown to the user.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の失敗内容。ユーザーへそのまま表示できる形で返す。
	*/
	const String& GoogleAuth::Error()const
	{
		return error_;
	}

	/**
	* [EN]
	* Exchanges the stored refresh token for a new access token.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 保持している更新用トークンを、新しいアクセストークンと交換する。
	*/
	Bool GoogleAuth::Refresh()
	{
		/// [EN] The same endpoint serves both exchanges; grant_type is what says which one is being asked for.
		/// [JP] 2種類の交換はどちらも同じ窓口を使い、どちらを求めているかは grant_type で伝える。
		return RequestToken(String(std::format("client_id={}&client_secret={}&refresh_token={}&grant_type=refresh_token", HttpClient::Escape(clientId_).str(), HttpClient::Escape(clientSecret_).str(), HttpClient::Escape(refreshToken_).str())));
	}

	/**
	* [EN]
	* Posts form to Google's token endpoint and stores whichever of the
	* access and refresh tokens come back.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* form を Google のトークンエンドポイントへ送り、返ってきた
	* アクセストークンと更新用トークンを保存する。
	*/
	Bool GoogleAuth::RequestToken(const String& form)
	{
		/// [EN] This one endpoint takes form-encoded fields rather than JSON, unlike every other Google call in this layer.
		/// [JP] この窓口だけは JSON ではなくフォーム形式の項目を受け取る。この層の他の Google 呼び出しとは異なる。
		DynamicArray<HttpHeader> headers;
		headers.push_back(HttpHeader{ String("Content-Type"), String("application/x-www-form-urlencoded") });

		/// [EN] The form text is copied into a byte buffer because that is the shape the transport sends.
		/// [JP] フォームの文字列をバイト列へ写す。通信側が送れるのはその形のため。
		std::string body = form.str();
		DynamicArray<Byte> payload(body.size());
		std::memcpy(payload.data(), body.data(), body.size());

		HttpResponse response = http_.Send(String("POST"), String("https://oauth2.googleapis.com/token"), headers, payload);
		if (response.status_ == 0)
		{
			/// [EN] Status 0 means the exchange never reached Google, so the transport's own description is kept.
			/// [JP] ステータス0は Google まで届かなかった場合。通信側が書いた説明をそのまま使う。
			error_ = response.error_;
			return false;
		}

		/// [EN] Parsing without exceptions returns a null value on malformed input, which is checked rather than thrown.
		/// [JP] 例外を使わない解析は、壊れた入力に対して null を返す。投げる代わりにその値を確認する。
		nlohmann::json result = nlohmann::json::parse(response.body_.begin(), response.body_.end(), nullptr, false);
		if (!result.is_object())
		{
			error_ = String("Google の応答を解釈できませんでした。");
			return false;
		}
		if (response.status_ != 200)
		{
			/// [EN] invalid_grant means the refresh token itself is dead: revoked, expired, or issued while the app was still in testing.
			/// [JP] invalid_grant は更新用トークン自体が死んでいる状態。取り消し、期限切れ、テスト状態で発行されたもの、など。
			if (result.value("error", "") == "invalid_grant")
			{
				/// [EN] Keeping a dead token would make every later call fail the same way, so it is discarded here.
				/// [JP] 死んだトークンを持ち続けると以降も同じ失敗を繰り返すため、ここで捨てる。
				SignOut();
				error_ = String("Google のログインが期限切れです。もう一度ログインしてください。");
			}
			else
			{
				error_ = String(std::format("Google のログインに失敗しました: {}", result.value("error_description", result.value("error", "unknown"))));
			}
			return false;
		}

		/// [EN] This short-lived token is what every Drive and Docs request will carry from now on.
		/// [JP] この短命なトークンが、以後の Drive と Docs のリクエストすべてに付いていくことになる。
		accessToken_ = String(result.value("access_token", ""));

		/// [EN] 60 seconds are shaved off the stated lifetime as a margin for slow requests and clock drift.
		/// [JP] 通信の遅さや時計のずれに備え、提示された寿命から60秒引いておく。
		Double now = std::chrono::duration<Double>(std::chrono::system_clock::now().time_since_epoch()).count();
		expiresAt_ = now + static_cast<Double>(result.value("expires_in", 3600)) - 60.0;

		/// [EN] A refresh token comes back only on the first exchange; a renewal answers without one and the stored one stays valid.
		/// [JP] 更新用トークンは最初の交換でしか返らない。更新時の応答には含まれず、保存済みのものが引き続き有効。
		if (result.contains("refresh_token"))
		{
			refreshToken_ = String(result.value("refresh_token", ""));
			SaveToken();
		}
		error_ = String();
		return true;
	}

	/**
	* [EN]
	* Waits on a loopback socket for the browser redirect and returns
	* the authorization code carried in its query string.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ループバックのソケットでブラウザからのリダイレクトを待ち、その
	* クエリ文字列に含まれる認可コードを返す。
	*/
	String GoogleAuth::ReceiveCode(SOCKET listener)
	{
		/// [EN] The member has to find the browser window, pick an account and approve, so the wait is deliberately long.
		/// [JP] メンバーはブラウザを探し、アカウントを選び、許可する必要がある。待ち時間は意図的に長く取る。
		DWORD timeout = 300000;
		setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const Char*>(&timeout), sizeof(timeout));

		/// [EN] Accept blocks until the browser connects, which happens the moment Google redirects it here.
		/// [JP] accept はブラウザが接続してくるまで戻らない。接続は Google がここへリダイレクトした瞬間に起きる。
		SOCKET browser = accept(listener, nullptr, nullptr);
		if (browser == INVALID_SOCKET)
		{
			error_ = String("ログインの完了を待っている間に時間切れになりました。");
			return String();
		}

		/// [EN] The accepted connection gets its own timeout, since the limit set above applies only to the listener.
		/// [JP] 受け入れた接続にも別途タイムアウトを設ける。上で設定した上限は受け口にしか効かないため。
		setsockopt(browser, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const Char*>(&timeout), sizeof(timeout));

		/// [EN] Only the request line matters - "GET /?code=... HTTP/1.1" - so one read of the first packet is enough.
		/// [JP] 必要なのは最初の行「GET /?code=... HTTP/1.1」だけなので、最初のパケットを1回読めば足りる。
		std::string request(4096, '\0');
		Int received = recv(browser, request.data(), static_cast<Int>(request.size()) - 1, 0);
		if (received > 0)
		{
			/// [EN] The buffer is cut down to what arrived, so the searches below do not run over unwritten bytes.
			/// [JP] 届いた分までバッファを切り詰める。下の検索が、書かれていない領域まで走らないようにするため。
			request.resize(static_cast<Size>(received));
		}
		else
		{
			request.clear();
		}

		/// [EN] The browser is left on a plain page saying it is done; nothing on that page ever talks to the Editor again.
		/// [JP] ブラウザには完了を伝えるだけの素朴なページを返す。このページが以後 Editor と通信することはない。
		std::string page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head><body>SeedCore へのログインが完了しました。このタブは閉じて構いません。</body></html>";

		/// [EN] The reply is written out by hand because this is the only HTTP message this program ever serves.
		/// [JP] 応答を手書きしているのは、このプログラムが HTTP を返す場面がここだけのため。
		std::string reply = std::format("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}", page.size(), page);
		send(browser, reply.data(), static_cast<Int>(reply.size()), 0);
		closesocket(browser);

		/// [EN] A refusal arrives as error=access_denied in the same query string, so it is reported rather than read as a timeout.
		/// [JP] 拒否された場合は同じクエリ文字列に error=access_denied が入る。時間切れと混同せず、そのまま報告する。
		if (request.find("error=") != std::string::npos)
		{
			error_ = String("Google へのアクセス許可が拒否されました。");
			return String();
		}

		/// [EN] The code sits between "code=" and the next separator, whether that is another field or the end of the line.
		/// [JP] コードは "code=" の後から、次の区切り（別の項目か行末）までの間にある。
		Size start = request.find("code=");
		if (start == std::string::npos)
		{
			error_ = String("ブラウザからの戻りに認可コードが含まれていませんでした。");
			return String();
		}
		start += 5;
		Size end = request.find_first_of("& \r\n", start);

		/// [EN] What the browser sent is percent-encoded, so it is decoded before being handed to the token exchange.
		/// [JP] ブラウザが送ってくる形はパーセントエンコードされているため、トークン交換へ渡す前に復号する。
		return HttpClient::Unescape(String(request.substr(start, end == std::string::npos ? std::string::npos : end - start)));
	}

	/**
	* [EN]
	* Reads the encrypted refresh token from tokenPath_ into memory.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 暗号化された更新用トークンを tokenPath_ から読み込む。
	*/
	void GoogleAuth::LoadToken()
	{
		/// [EN] A missing file simply means this member has not signed in yet, which is not an error worth reporting.
		/// [JP] ファイルが無いのは、このメンバーがまだログインしていないというだけで、報告すべき失敗ではない。
		std::ifstream stream(tokenPath_, std::ios::binary);
		if (!stream)
		{
			return;
		}
		DynamicArray<Byte> content((std::istreambuf_iterator<Char>(stream)), std::istreambuf_iterator<Char>());

		/// [EN] The file is the 16-byte initialization vector followed by the ciphertext, the same layout BinaryArchive uses.
		/// [JP] ファイルの中身は16バイトの初期化ベクタに暗号文が続く形。BinaryArchive と同じ並び。
		if (content.size() <= 16)
		{
			return;
		}
		DynamicArray<Byte> iv(content.begin(), content.begin() + 16);
		DynamicArray<Byte> ciphertext(content.begin() + 16, content.end());

		/// [EN] The key is derived from the engine's seed, so it is the same on every machine that runs this Editor.
		/// [JP] 鍵はエンジンの種から導出するため、この Editor を動かすどの PC でも同じものになる。
		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		/// [EN] An empty result means the file was damaged or written by a different build; the member signs in again.
		/// [JP] 結果が空なら、ファイルが壊れているか別のビルドが書いたもの。その場合はメンバーが再ログインする。
		DynamicArray<Byte> plaintext = Aes256::Decrypt(key, iv, ciphertext);
		if (!plaintext.empty())
		{
			refreshToken_ = String(std::string(plaintext.data(), plaintext.size()));
		}
	}

	/**
	* [EN]
	* Writes the refresh token back to tokenPath_, encrypted with the
	* engine's asset key.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 更新用トークンを、エンジンのアセット用鍵で暗号化して tokenPath_
	* へ書き戻す。
	*/
	void GoogleAuth::SaveToken()const
	{
		/// [EN] For this application the token is as good as the member's Drive password, so it never touches disk in the clear.
		/// [JP] このアプリにとってこのトークンはメンバーの Drive の鍵に等しいため、平文のままディスクへは置かない。
		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		/// [EN] A fresh random initialization vector per write keeps two saves of the same token from producing identical files.
		/// [JP] 書き込みごとに初期化ベクタを乱数で作り直し、同じトークンを2回保存しても同じファイルにならないようにする。
		DynamicArray<Byte> iv(16);
		std::random_device randomDevice;
		for (Byte& ivByte : iv)
		{
			ivByte = static_cast<Byte>(randomDevice());
		}

		/// [EN] The token text is copied into a byte buffer, which is the form the cipher works on.
		/// [JP] トークンの文字列をバイト列へ写す。暗号が扱えるのはその形のため。
		std::string token = refreshToken_.str();
		DynamicArray<Byte> plaintext(token.size());
		std::memcpy(plaintext.data(), token.data(), token.size());
		DynamicArray<Byte> ciphertext = Aes256::Encrypt(key, iv, plaintext);

		/// [EN] The folder may not exist on a first sign-in, so it is created before the file is opened.
		/// [JP] 初回ログインではフォルダがまだ無いことがあるため、ファイルを開く前に作っておく。
		std::error_code errorCode;
		std::filesystem::create_directories(tokenPath_.parent_path(), errorCode);
		std::ofstream stream(tokenPath_, std::ios::binary);
		if (stream)
		{
			/// [EN] The initialization vector is written in front because decrypting needs the same one that encrypted.
			/// [JP] 初期化ベクタを先頭に書くのは、復号に暗号化時と同じものが要るため。
			stream.write(iv.data(), iv.size());
			stream.write(ciphertext.data(), ciphertext.size());
		}
	}
}
