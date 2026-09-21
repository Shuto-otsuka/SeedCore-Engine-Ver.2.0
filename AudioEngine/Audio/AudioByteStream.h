#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* File I/O that CRI File System uses instead of its own, registered
	* through criFs_SetSelectIoCallback. It serves two kinds of path:
	* plain paths are read straight from disk, and paths starting with
	* "seedcore_audio://" point at a .audio cache whose stream blob (the
	* AWB) is AES-256-CBC encrypted. For those, Read decrypts only the
	* 16-byte blocks covering the requested range, so CRI can stream from
	* the middle of the file without the whole AWB ever being decrypted to
	* disk or memory. Only reading is supported; every read completes
	* synchronously.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI File System が自前の入出力の代わりに使うファイル I/O。
	* criFs_SetSelectIoCallback で登録する。2種類のパスを扱う: 普通のパスは
	* ディスクからそのまま読み、"seedcore_audio://" で始まるパスは、ストリーム
	* 用ブロブ(AWB)が AES-256-CBC で暗号化された .audio キャッシュを指す。
	* 後者では、Read が要求範囲を覆う 16 バイトブロックだけを復号するので、
	* AWB 全体をディスクやメモリに復号することなく、CRI がファイルの途中から
	* ストリーミングできる。読み込みのみ対応し、読み込みは全て同期で完了する。
	*/
	class SEEDCORE_API AudioByteStream
	{
	public:
		/**
		* [EN]
		* CRI's device-selection callback: routes every path to the default
		* device with this class's I/O interface.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CRI のデバイス選択コールバック: 全てのパスを、このクラスの I/O
		* インターフェースを持つ既定デバイスへ振り分ける。
		*/
		static CriError CRIAPI SelectIo(const CriChar8* path, CriFsDeviceId* deviceID, CriFsIoInterfacePtr* ioInterface);

	private:
		/**
		* [EN]
		* Reports whether the file behind path exists on disk (the
		* "seedcore_audio://" prefix is stripped first).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path が指すファイルがディスク上に存在するかを返す
		* ("seedcore_audio://" の接頭辞は先に取り除く)。
		*/
		static CriFsIoError CRIAPI Exists(const CriChar8* path, CriBool* result);

		/**
		* [EN]
		* Opens a file for reading. For an encrypted .audio cache it reads
		* the header and blob table, locates the stream blob (id 1) and its
		* IV, and exposes only that blob's plain contents as the file. A file
		* that cannot be opened, or write access, yields a null handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ファイルを読み込み用に開く。暗号化された .audio キャッシュでは、
		* ヘッダとブロブ表を読んでストリーム用ブロブ(ID 1)と IV の位置を
		* 求め、そのブロブの平文だけをファイルの中身として見せる。開けない
		* ファイルや書き込みアクセスでは、null のハンドルを返す。
		*/
		static CriFsIoError CRIAPI Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* fileHandle);

		/**
		* [EN]
		* Closes the OS handle and frees the File allocated by Open.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* OS のハンドルを閉じ、Open で確保した File を解放する。
		*/
		static CriFsIoError CRIAPI Close(CriFsFileHn fileHandle);

		/**
		* [EN]
		* Returns the file's size as CRI sees it: the plain size of the
		* stream blob for an encrypted cache, the disk size otherwise.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CRI から見たファイルサイズを返す: 暗号化キャッシュならストリーム用
		* ブロブの平文サイズ、そうでなければディスク上のサイズ。
		*/
		static CriFsIoError CRIAPI FileSize(CriFsFileHn fileHandle, CriSint64* fileSize);

		/**
		* [EN]
		* Reads up to readSize bytes of plain data starting at offset into
		* buffer, decrypting on the fly for an encrypted cache. The number
		* of bytes actually read is kept for ReadSize.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* offset から最大 readSize バイトの平文を buffer へ読み込む。暗号化
		* キャッシュでは読みながら復号する。実際に読めたバイト数は ReadSize
		* のために保持しておく。
		*/
		static CriFsIoError CRIAPI Read(CriFsFileHn fileHandle, CriSint64 offset, CriSint64 readSize, void* buffer, CriSint64 bufferSize);

		/**
		* [EN]
		* Reports whether the last Read has finished. Always true, since
		* Read completes synchronously.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直前の Read が完了したかを返す。Read は同期で完了するので常に true。
		*/
		static CriFsIoError CRIAPI Complete(CriFsFileHn fileHandle, CriBool* result);

		/**
		* [EN]
		* Returns how many bytes the last Read actually produced.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直前の Read で実際に読めたバイト数を返す。
		*/
		static CriFsIoError CRIAPI ReadSize(CriFsFileHn fileHandle, CriSint64* readSize);

	private:
		/**
		* [EN]
		* The state behind one CriFsFileHn handed to CRI by Open.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Open が CRI へ渡す CriFsFileHn 1つ分の状態。
		*/
		struct File
		{
			/// [EN] The OS file handle.
			/// [JP] OS のファイルハンドル。
			HANDLE handle_ = INVALID_HANDLE_VALUE;

			/// [EN] Whether this is an encrypted .audio cache rather than a plain file.
			/// [JP] 普通のファイルではなく、暗号化された .audio キャッシュかどうか。
			Bool encrypted_ = false;

			/// [EN] IV of the stream blob; decrypts its first ciphertext block.
			/// [JP] ストリーム用ブロブの IV。最初の暗号文ブロックの復号に使う。
			Byte iv_[16]{};

			/// [EN] Offset in the file where the stream blob's ciphertext starts (after the IV).
			/// [JP] ファイル内で、ストリーム用ブロブの暗号文(IV の後ろ)が始まる位置。
			Uint64 ciphertextOffset_ = 0;

			/// [EN] Size of the stream blob's ciphertext, excluding the IV.
			/// [JP] ストリーム用ブロブの暗号文のサイズ(IV を除く)。
			Uint64 ciphertextSize_ = 0;

			/// [EN] Size of the plain data CRI sees as the whole file.
			/// [JP] CRI がファイル全体として見る、平文のサイズ。
			Uint64 plainSize_ = 0;

			/// [EN] Bytes produced by the last Read, returned by ReadSize.
			/// [JP] 直前の Read で読めたバイト数。ReadSize が返す。
			Uint64 readSize_ = 0;
		};

		/// [EN] The I/O function table SelectIo hands to CRI.
		/// [JP] SelectIo が CRI へ渡す、I/O 関数のテーブル。
		static CriFsIoInterface ioInterface_;
	};
}
