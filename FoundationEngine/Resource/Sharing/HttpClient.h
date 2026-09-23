#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* One request header, as a name/value pair.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リクエストヘッダ1つを、名前と値の組として表す。
	*/
	struct HttpHeader
	{
		/// [EN] Header name, without the trailing colon.
		/// [JP] ヘッダ名。末尾のコロンは含まない。
		String name_;

		/// [EN] Header value.
		/// [JP] ヘッダの値。
		String value_;
	};

	/**
	* [EN]
	* The outcome of one HTTP request: status line, body, and the server's
	* own clock. A transport failure leaves status_ at 0 and describes
	* itself in error_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* HTTPリクエスト1回の結果。ステータス、ボディ、サーバー側の時刻を持つ。
	* 通信自体に失敗した場合は status_ が 0 のままとなり、error_ に内容が
	* 入る。
	*/
	struct HttpResponse
	{
		/// [EN] HTTP status code, or 0 when the request never reached the server.
		/// [JP] HTTPステータスコード。サーバーまで届かなかった場合は 0。
		Uint32 status_ = 0;

		/// [EN] Response body, exactly as received.
		/// [JP] 受信したままのレスポンスボディ。
		DynamicArray<Byte> body_;

		/// [EN] The Date header as seconds since the Unix epoch; 0 when absent.
		/// [JP] Date ヘッダをUnixエポックからの秒数にしたもの。無ければ 0。
		Double serverTime_ = 0.0;

		/// [EN] The Location header, which a resumable upload answers with; empty when absent.
		/// [JP] Location ヘッダ。再開可能アップロードの開始時に返ってくる。無ければ空。
		String location_;

		/// [EN] Human-readable failure description; empty on success.
		/// [JP] 人が読める失敗の説明。成功時は空。
		String error_;
	};

	/**
	* [EN]
	* HTTPS client over WinHTTP, used by every Google API call the sharing
	* layer makes. One instance owns one WinHTTP session and may be used
	* from a single thread at a time.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* WinHTTP を使う HTTPS クライアント。共有機能が行う Google API 呼び出し
	* はすべてこれを通る。1インスタンスが1つの WinHTTP セッションを持ち、
	* 同時に使えるスレッドは1つ。
	*/
	class SEEDCORE_API HttpClient
	{
	public:
		/**
		* [EN]
		* Opens the WinHTTP session and applies the timeouts every sharing
		* request uses.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* WinHTTP セッションを開き、共有機能の全リクエストで使うタイム
		* アウトを設定する。
		*/
		HttpClient();

		/**
		* [EN]
		* Closes the WinHTTP session.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* WinHTTP セッションを閉じる。
		*/
		~HttpClient();

		/// [EN] Copying is disallowed because two objects must not close one session handle.
		/// [JP] 1つのセッションハンドルを2つのオブジェクトが閉じることのないよう、コピーは禁止する。
		HttpClient(const HttpClient&) = delete;
		HttpClient& operator=(const HttpClient&) = delete;

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
		HttpResponse Send(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const DynamicArray<Byte>& body);

		/**
		* [EN]
		* Sends a request without a body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディなしでリクエストを送る。
		*/
		HttpResponse Send(const String& method, const String& url, const DynamicArray<HttpHeader>& headers);

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
		HttpResponse Download(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const std::filesystem::path& destination);

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
		Bool Ready()const;

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
		static String Escape(const String& value);

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
		static String Unescape(const String& value);

	private:
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
		HttpResponse Exchange(const String& method, const String& url, const DynamicArray<HttpHeader>& headers, const DynamicArray<Byte>& body, std::ofstream* destination);

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
		static String ReadLocation(HINTERNET request);

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
		static Double ReadServerTime(HINTERNET request);

		/**
		* [EN]
		* Builds the CRLF-separated header block WinHttpSendRequest takes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* WinHttpSendRequest が受け取る、CRLF区切りのヘッダ列を組み立てる。
		*/
		static std::wstring BuildHeaders(const DynamicArray<HttpHeader>& headers);

	private:
		/// [EN] WinHTTP session handle, shared by every request this client sends.
		/// [JP] WinHTTP のセッションハンドル。このクライアントの全リクエストで共有する。
		HINTERNET session_ = nullptr;
	};
}
