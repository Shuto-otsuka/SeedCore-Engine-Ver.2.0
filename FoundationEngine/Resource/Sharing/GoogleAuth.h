#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/HttpClient.h>

namespace SeedCore
{
	/**
	* [EN]
	* Google sign-in for the shared asset library. Runs the desktop OAuth
	* flow once per member (browser plus a loopback listener), keeps the
	* refresh token encrypted on disk, and hands out access tokens,
	* renewing them as they expire.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有アセットライブラリ用の Google ログイン。メンバーごとに1回だけ
	* デスクトップ用の OAuth 手順（ブラウザとループバック受信）を行い、更新用
	* トークンを暗号化してディスクに保持し、期限が来たら更新しながら
	* アクセストークンを渡す。
	*/
	class SEEDCORE_API GoogleAuth
	{
	public:
		/**
		* [EN]
		* Takes the transport and the client credentials, then loads
		* whatever refresh token a previous session left on disk.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 通信手段とクライアント情報を受け取り、前回の起動がディスクへ
		* 残した更新用トークンがあれば読み込む。
		*/
		GoogleAuth(HttpClient& http, const std::filesystem::path& tokenPath, const String& clientId, const String& clientSecret);

		/**
		* [EN]
		* Nothing is undone here: the refresh token stays on disk so the
		* next run starts signed in.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ここで後始末することは無い。更新用トークンは、次回の起動が
		* ログイン済みで始められるようディスクへ残す。
		*/
		~GoogleAuth();

		/// [EN] Copying is disallowed because one sign-in belongs to one Editor run.
		/// [JP] 1つのログインは1つの Editor の起動に属するものなので、コピーは禁止する。
		GoogleAuth(const GoogleAuth&) = delete;
		GoogleAuth& operator=(const GoogleAuth&) = delete;

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
		Bool SignedIn()const;

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
		Bool SignIn();

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
		void SignOut();

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
		String AccessToken();

		/**
		* [EN]
		* The last failure, if any, in a form that can be shown to the user.
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
		* Exchanges the stored refresh token for a new access token.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 保持している更新用トークンを、新しいアクセストークンと交換する。
		*/
		Bool Refresh();

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
		Bool RequestToken(const String& form);

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
		String ReceiveCode(SOCKET listener);

		/**
		* [EN]
		* Reads the encrypted refresh token from tokenPath_ into memory.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 暗号化された更新用トークンを tokenPath_ から読み込む。
		*/
		void LoadToken();

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
		void SaveToken()const;

	private:
		/// [EN] Transport used for the token endpoint; owned by the sharing layer.
		/// [JP] トークンエンドポイントとの通信に使う。所有者は共有機能側。
		HttpClient& http_;

		/// [EN] File holding the encrypted refresh token.
		/// [JP] 暗号化した更新用トークンを置くファイル。
		std::filesystem::path tokenPath_;

		/// [EN] OAuth client identifier of the Editor's desktop application.
		/// [JP] Editor のデスクトップアプリとしての OAuth クライアントID。
		String clientId_;

		/// [EN] OAuth client secret; not secret for desktop clients, per Google's own guidance.
		/// [JP] OAuth クライアントシークレット。Google の案内通り、デスクトップ用では秘密扱いされない。
		String clientSecret_;

		/// [EN] Long-lived token used to obtain access tokens.
		/// [JP] アクセストークンを得るために使う長期のトークン。
		String refreshToken_;

		/// [EN] Short-lived token sent with every API call.
		/// [JP] すべてのAPI呼び出しに付ける短期のトークン。
		String accessToken_;

		/// [EN] Local clock reading, in seconds, at which accessToken_ stops being valid.
		/// [JP] accessToken_ が無効になるローカル時刻（秒）。
		Double expiresAt_ = 0.0;

		/// [EN] Last failure description; empty while everything is working.
		/// [JP] 直近の失敗の説明。問題なく動いている間は空。
		String error_;
	};
}
