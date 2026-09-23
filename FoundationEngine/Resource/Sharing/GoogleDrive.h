#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Sharing/GoogleAuth.h>
#include <FoundationEngine/Resource/Sharing/HttpClient.h>

namespace SeedCore
{
	/**
	* [EN]
	* The shared library's file storage. Asset contents live in a Drive
	* folder as one file per unique content hash: a file is written once
	* and never modified, so a download can never race a change, and two
	* revisions that share content share one file.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 共有ライブラリのファイル置き場。アセットの中身は、内容のハッシュ
	* ごとに1ファイルとして Drive のフォルダに置く。一度書いたファイルは
	* 変更しないので、ダウンロード中に中身が変わることがなく、内容が同じ
	* Revision は1つのファイルを共有する。
	*/
	/**
	* [EN]
	* One child of a Drive folder, as a listing reports it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Drive のフォルダの子1つ。一覧で得られる内容を表す。
	*/
	struct DriveEntry
	{
		/// [EN] Identifier of the file or folder.
		/// [JP] そのファイルまたはフォルダの識別子。
		String id_;

		/// [EN] Name as the member who put it there typed it.
		/// [JP] 置いた人が付けたままの名前。
		String name_;

		/// [EN] Whether this child is itself a folder, which a walk has to descend into.
		/// [JP] その子自身がフォルダかどうか。走査ではここへ降りていく。
		Bool folder_ = false;

		/// [EN] SHA-256 of the contents as Drive computed it, which tells an already-taken-in file from a changed one without downloading.
		/// [JP] Drive 側が計算した中身の SHA-256。ダウンロードせずに、取り込み済みのファイルと差し替えられたファイルを見分けられる。

		/// [EN] Empty for a folder, and for the rare file Drive has not hashed; the size then stands in for it.
		/// [JP] フォルダと、まれに Drive がハッシュを持たないファイルでは空になる。その場合はサイズで代用する。
		String hash_;

		/// [EN] Size in bytes as Drive reports it.
		/// [JP] Drive が示すバイト数。
		Uint64 size_ = 0;
	};

	class SEEDCORE_API GoogleDrive
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
		GoogleDrive(HttpClient& http, GoogleAuth& auth);

		/**
		* [EN]
		* Nothing to undo: every transfer finishes before its call returns.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 後始末は要らない。どの転送も、呼び出しが戻る前に終わっているため。
		*/
		~GoogleDrive();

		/// [EN] Copying is disallowed because the error and the server clock belong to one caller.
		/// [JP] エラーとサーバー時刻は1つの呼び出し元に属するものなので、コピーは禁止する。
		GoogleDrive(const GoogleDrive&) = delete;
		GoogleDrive& operator=(const GoogleDrive&) = delete;

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
		String Find(const String& name, const String& parentId);

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
		Bool List(const String& parentId, DynamicArray<DriveEntry>& entries);

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
		Bool Move(const String& fileId, const String& fromParentId, const String& toParentId);

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
		String CreateFolder(const String& name, const String& parentId);

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
		String Upload(const std::filesystem::path& source, const String& name, const String& parentId);

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
		Bool Fetch(const String& fileId, const std::filesystem::path& destination);

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
		Bool Trash(const String& fileId);

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
		* Sends one authenticated request to the Drive API and parses its
		* JSON answer, recording the server clock and any failure.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 認証付きのリクエストを Drive API へ1回送り、JSON の応答を解釈
		* する。サーバー側の時刻と、失敗した場合はその内容も記録する。
		*/
		Bool Send(const String& method, const String& url, const nlohmann::json& request, nlohmann::json& result);

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
		String BeginUpload(const String& name, const String& parentId, Uint64 size);

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
		String SendChunks(const String& session, const std::filesystem::path& source, Uint64 size);

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
		static String EscapeQuery(const String& value);

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

		/// [EN] Last failure description; empty while everything is working.
		/// [JP] 直近の失敗の説明。問題なく動いている間は空。
		String error_;
	};
}
