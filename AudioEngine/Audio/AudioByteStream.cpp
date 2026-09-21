#include <AudioEngine/Audio/AudioByteStream.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	/// [EN] Slots follow CriFsIoInterface's member order; a null slot is an operation this device does not support.
	/// [JP] 並びは CriFsIoInterface のメンバ順。null の枠は、このデバイスが対応しない操作。
	CriFsIoInterface AudioByteStream::ioInterface_ =
	{
		&AudioByteStream::Exists,

		/// [EN] Remove / Rename: this device is read-only.
		/// [JP] Remove / Rename: このデバイスは読み込み専用。
		nullptr,
		nullptr,

		&AudioByteStream::Open,
		&AudioByteStream::Close,
		&AudioByteStream::FileSize,
		&AudioByteStream::Read,
		&AudioByteStream::Complete,

		/// [EN] CancelRead: Read is synchronous, so there is nothing in flight to cancel.
		/// [JP] CancelRead: Read は同期なので、取り消す処理中の読み込みが無い。
		nullptr,

		&AudioByteStream::ReadSize,

		/// [EN] Write / WriteComplete / CancelWrite / WriteSize / Flush / Resize: read-only.
		/// [JP] Write / WriteComplete / CancelWrite / WriteSize / Flush / Resize: 読み込み専用のため。
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,

		/// [EN] NativeFileHandle / ReadProgressCallback / ParallelRead: not provided.
		/// [JP] NativeFileHandle / ReadProgressCallback / ParallelRead: 提供しない。
		nullptr,
		nullptr,
		nullptr,
	};

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
	CriError CRIAPI AudioByteStream::SelectIo(const CriChar8* path, CriFsDeviceId* deviceID, CriFsIoInterfacePtr* ioInterface)
	{
		/// [EN] Plain and encrypted paths share one device; Open tells them apart by prefix.
		/// [JP] 普通のパスと暗号化パスは同じデバイスを使う。区別は Open が接頭辞で行う。
		if (deviceID)
		{
			*deviceID = CRIFS_DEFAULT_DEVICE;
		}

		if (ioInterface)
		{
			*ioInterface = &ioInterface_;
		}

		return CRIERR_OK;
	}

	/**
	* [EN]
	* Reports whether the file behind path exists on disk.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path が指すファイルがディスク上に存在するかを返す。
	*/
	CriFsIoError CRIAPI AudioByteStream::Exists(const CriChar8* path, CriBool* result)
	{
		if (!result)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*result = CRI_FALSE;

		/// [EN] A missing path is "does not exist", not an I/O failure.
		/// [JP] パスが無いのは「存在しない」であって、I/O の失敗ではない。
		if (!path)
		{
			return CRIFS_IO_ERROR_OK;
		}

		/// [EN] The prefix only marks encryption; the rest is an ordinary disk path.
		/// [JP] 接頭辞は暗号化の目印にすぎず、残りは普通のディスク上のパス。
		std::string_view pathView(path);
		if (pathView.starts_with("seedcore_audio://"))
		{
			pathView.remove_prefix(std::strlen("seedcore_audio://"));
		}

		String filePath(pathView);
		*result = std::filesystem::exists(filePath.c_str()) ? CRI_TRUE : CRI_FALSE;

		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Opens a file for reading, locating the encrypted stream blob for a
	* .audio cache. A file that cannot be opened, or write access, yields
	* a null handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ファイルを読み込み用に開く。.audio キャッシュでは、暗号化された
	* ストリーム用ブロブの位置を求める。開けないファイルや書き込み
	* アクセスでは、null のハンドルを返す。
	*/
	CriFsIoError CRIAPI AudioByteStream::Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* fileHandle)
	{
		if (!fileHandle)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*fileHandle = nullptr;

		/// [EN] CRI's contract: a recoverable failure returns OK with a null handle, not NG.
		/// [JP] CRI の取り決め: 続行可能な失敗は NG ではなく、null ハンドルと OK を返す。
		if (!path || access == CRIFS_FILE_ACCESS_WRITE || access == CRIFS_FILE_ACCESS_READ_WRITE)
		{
			return CRIFS_IO_ERROR_OK;
		}

		std::string_view pathView(path);
		Bool encrypted = pathView.starts_with("seedcore_audio://");
		if (encrypted)
		{
			pathView.remove_prefix(std::strlen("seedcore_audio://"));
		}

		/// [EN] SEQUENTIAL_SCAN: CRI streams mostly forward, so let the OS read ahead.
		/// [JP] SEQUENTIAL_SCAN: CRI はほぼ前方向にストリーミングするので、OS に先読みさせる。
		String filePath(pathView);
		HANDLE handle = CreateFileW(filePath.w_str().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return CRIFS_IO_ERROR_OK;
		}

		/// [EN] Heap-allocated because CRI keeps the handle until Close, which deletes it.
		/// [JP] CRI は Close までハンドルを保持するのでヒープに確保する。解放は Close で行う。
		File* file = new File();
		file->handle_ = handle;
		file->encrypted_ = encrypted;

		/// [EN] A plain file is exposed as-is: its disk size is the size CRI sees.
		/// [JP] 普通のファイルはそのまま見せる: ディスク上のサイズが CRI から見たサイズ。
		if (!encrypted)
		{
			LARGE_INTEGER fileSize{};
			GetFileSizeEx(handle, &fileSize);
			file->plainSize_ = static_cast<Uint64>(fileSize.QuadPart);

			*fileHandle = file;
			return CRIFS_IO_ERROR_OK;
		}

		/// [EN] 24-byte header: magic "SCAUDIO\0", version, sound type, blob count, reserved.
		/// [JP] 24 バイトのヘッダ: マジック "SCAUDIO\0"、バージョン、サウンド種別、ブロブ数、予約。
		Byte header[24]{};
		DWORD readBytes = 0;
		OVERLAPPED headerOverlapped{};
		if (!ReadFile(handle, header, 24, &readBytes, &headerOverlapped) || readBytes != 24 || std::memcmp(header, "SCAUDIO\0", 8) != 0)
		{
			CloseHandle(handle);
			delete file;
			return CRIFS_IO_ERROR_NG_INVALID_DATA;
		}

		/// [EN] The blob count sits at byte 16 of the header.
		/// [JP] ブロブ数はヘッダの 16 バイト目にある。
		Uint32 blobCount = 0;
		std::memcpy(&blobCount, header + 16, 4);

		/// [EN] Scan the blob table (at most 2 entries) for the stream blob, id 1.
		/// [JP] ブロブ表(最大 2 件)から、ストリーム用ブロブ(ID 1)を探す。
		Uint64 blobOffset = 0;
		Uint64 blobEncryptedSize = 0;
		Uint64 blobPlainSize = 0;
		for (Uint32 blobIndex = 0; blobIndex < blobCount && blobIndex < 2; ++blobIndex)
		{
			/// [EN] 32-byte entry: id, reserved, offset, encrypted size (with IV), plain size.
			/// [JP] 32 バイトの項目: ID、予約、位置、暗号化サイズ(IV 込み)、平文サイズ。
			Byte entry[32]{};
			OVERLAPPED entryOverlapped{};
			entryOverlapped.Offset = static_cast<DWORD>(24 + blobIndex * 32);
			if (!ReadFile(handle, entry, 32, &readBytes, &entryOverlapped) || readBytes != 32)
			{
				CloseHandle(handle);
				delete file;
				return CRIFS_IO_ERROR_NG_INVALID_DATA;
			}

			/// [EN] Blob 0 is the in-memory body (ACB / wave) that AudioLoader reads itself.
			/// [JP] ブロブ 0 は AudioLoader が自分で読むメモリ上の本体(ACB / wave)。
			Uint32 blobID = 0;
			std::memcpy(&blobID, entry, 4);
			if (blobID != 1)
			{
				continue;
			}

			std::memcpy(&blobOffset, entry + 8, 8);
			std::memcpy(&blobEncryptedSize, entry + 16, 8);
			std::memcpy(&blobPlainSize, entry + 24, 8);
		}

		/// [EN] 16 bytes or less means no stream blob, or one holding only an IV.
		/// [JP] 16 バイト以下は、ストリーム用ブロブが無いか、IV しか入っていない状態。
		if (blobEncryptedSize <= 16)
		{
			CloseHandle(handle);
			delete file;
			return CRIFS_IO_ERROR_NG_NO_ENTRY;
		}

		/// [EN] The blob starts with its 16-byte IV, followed by the ciphertext.
		/// [JP] ブロブは先頭 16 バイトが IV で、その後ろに暗号文が続く。
		OVERLAPPED ivOverlapped{};
		ivOverlapped.Offset = static_cast<DWORD>(blobOffset & 0xFFFFFFFF);
		ivOverlapped.OffsetHigh = static_cast<DWORD>(blobOffset >> 32);
		if (!ReadFile(handle, file->iv_, 16, &readBytes, &ivOverlapped) || readBytes != 16)
		{
			CloseHandle(handle);
			delete file;
			return CRIFS_IO_ERROR_NG_INVALID_DATA;
		}

		file->ciphertextOffset_ = blobOffset + 16;
		file->ciphertextSize_ = blobEncryptedSize - 16;
		file->plainSize_ = blobPlainSize;

		*fileHandle = file;
		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Closes the OS handle and frees the File allocated by Open.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* OS のハンドルを閉じ、Open で確保した File を解放する。
	*/
	CriFsIoError CRIAPI AudioByteStream::Close(CriFsFileHn fileHandle)
	{
		/// [EN] A null handle is one Open failed to create; closing it is a no-op.
		/// [JP] null のハンドルは Open が作れなかったもの。閉じても何もしない。
		File* file = static_cast<File*>(fileHandle);
		if (!file)
		{
			return CRIFS_IO_ERROR_OK;
		}

		if (file->handle_ != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file->handle_);
		}

		delete file;
		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Returns the file's size as CRI sees it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI から見たファイルサイズを返す。
	*/
	CriFsIoError CRIAPI AudioByteStream::FileSize(CriFsFileHn fileHandle, CriSint64* fileSize)
	{
		File* file = static_cast<File*>(fileHandle);
		if (!file || !fileSize)
		{
			return CRIFS_IO_ERROR_NG;
		}

		/// [EN] For an encrypted cache this is the stream blob's plain size, not the .audio size.
		/// [JP] 暗号化キャッシュでは、.audio のサイズではなくストリーム用ブロブの平文サイズ。
		*fileSize = static_cast<CriSint64>(file->plainSize_);
		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Reads up to readSize bytes of plain data starting at offset into
	* buffer, decrypting on the fly for an encrypted cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* offset から最大 readSize バイトの平文を buffer へ読み込む。暗号化
	* キャッシュでは読みながら復号する。
	*/
	CriFsIoError CRIAPI AudioByteStream::Read(CriFsFileHn fileHandle, CriSint64 offset, CriSint64 readSize, void* buffer, CriSint64 bufferSize)
	{
		File* file = static_cast<File*>(fileHandle);
		if (!file || !buffer)
		{
			return CRIFS_IO_ERROR_NG;
		}

		file->readSize_ = 0;

		/// [EN] Reading at or past the end is not an error; it just produces 0 bytes.
		/// [JP] 末尾以降を読むのはエラーではなく、0 バイト読めたことになるだけ。
		if (offset < 0 || readSize <= 0 || static_cast<Uint64>(offset) >= file->plainSize_)
		{
			return CRIFS_IO_ERROR_OK;
		}

		/// [EN] Clamp to both the end of the plain data and the caller's buffer.
		/// [JP] 平文の末尾と、呼び出し側のバッファの大きさの両方で切り詰める。
		Uint64 toRead = Min<Uint64>(static_cast<Uint64>(readSize), file->plainSize_ - static_cast<Uint64>(offset));
		toRead = Min<Uint64>(toRead, static_cast<Uint64>(bufferSize));

		DWORD readBytes = 0;

		/// [EN] Plain file: one positioned read straight into the caller's buffer.
		/// [JP] 普通のファイル: 位置を指定した1回の読み込みで、呼び出し側のバッファへ直接読む。
		if (!file->encrypted_)
		{
			OVERLAPPED overlapped{};
			overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
			overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
			if (!ReadFile(file->handle_, buffer, static_cast<DWORD>(toRead), &readBytes, &overlapped))
			{
				return CRIFS_IO_ERROR_NG;
			}

			file->readSize_ = readBytes;
			return CRIFS_IO_ERROR_OK;
		}

		/// [EN] Widen the range to whole 16-byte AES blocks, but never past the ciphertext.
		/// [JP] 範囲を 16 バイトの AES ブロック単位まで広げる。ただし暗号文の末尾は越えない。
		Uint64 blockStart = (static_cast<Uint64>(offset) / 16) * 16;
		Uint64 blockEnd = Min<Uint64>(((static_cast<Uint64>(offset) + toRead + 15) / 16) * 16, file->ciphertextSize_);

		/// [EN] In CBC a block is decrypted with the previous ciphertext block as its IV.
		/// [JP] CBC では、各ブロックは1つ前の暗号文ブロックを IV として復号する。
		DynamicArray<Byte> chainIv(16);
		if (blockStart == 0)
		{
			/// [EN] The very first block has no predecessor, so it uses the blob's own IV.
			/// [JP] 最初のブロックには前が無いので、ブロブ自身の IV を使う。
			std::memcpy(chainIv.data(), file->iv_, 16);
		}
		else
		{
			/// [EN] Otherwise read the 16 ciphertext bytes just before the range.
			/// [JP] それ以外は、範囲の直前にある暗号文 16 バイトを読む。
			Uint64 chainOffset = file->ciphertextOffset_ + blockStart - 16;
			OVERLAPPED chainOverlapped{};
			chainOverlapped.Offset = static_cast<DWORD>(chainOffset & 0xFFFFFFFF);
			chainOverlapped.OffsetHigh = static_cast<DWORD>(chainOffset >> 32);
			if (!ReadFile(file->handle_, chainIv.data(), 16, &readBytes, &chainOverlapped) || readBytes != 16)
			{
				return CRIFS_IO_ERROR_NG;
			}
		}

		/// [EN] Read only the ciphertext blocks covering the requested range.
		/// [JP] 要求範囲を覆う暗号文ブロックだけを読む。
		DynamicArray<Byte> ciphertextChunk(static_cast<Size>(blockEnd - blockStart));
		Uint64 chunkOffset = file->ciphertextOffset_ + blockStart;
		OVERLAPPED chunkOverlapped{};
		chunkOverlapped.Offset = static_cast<DWORD>(chunkOffset & 0xFFFFFFFF);
		chunkOverlapped.OffsetHigh = static_cast<DWORD>(chunkOffset >> 32);
		if (!ReadFile(file->handle_, ciphertextChunk.data(), static_cast<DWORD>(ciphertextChunk.size()), &readBytes, &chunkOverlapped) || readBytes != ciphertextChunk.size())
		{
			return CRIFS_IO_ERROR_NG;
		}

		/// [EN] Same key as every other encrypted asset: SHA-256 of SC_ENCRYPTION_KEY_SEED.
		/// [JP] 他の暗号化アセットと同じ鍵: SC_ENCRYPTION_KEY_SEED の SHA-256。
		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		/// [EN] Unpadded: a mid-stream chunk has no PKCS7 padding to strip.
		/// [JP] パディング無しで復号: ストリーム途中の塊には、取り除く PKCS7 パディングが無い。
		DynamicArray<Byte> decrypted = Aes256::DecryptUnpadded(key, chainIv, ciphertextChunk);
		if (decrypted.empty())
		{
			return CRIFS_IO_ERROR_NG;
		}

		/// [EN] Drop the bytes before offset that were only decrypted to complete a block.
		/// [JP] ブロックを揃えるためだけに復号した、offset より前のバイトは捨てる。
		Uint64 offsetInChunk = static_cast<Uint64>(offset) - blockStart;
		std::memcpy(buffer, decrypted.data() + offsetInChunk, static_cast<Size>(toRead));

		file->readSize_ = toRead;
		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Reports whether the last Read has finished. Always true.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直前の Read が完了したかを返す。常に true。
	*/
	CriFsIoError CRIAPI AudioByteStream::Complete(CriFsFileHn fileHandle, CriBool* result)
	{
		if (!result)
		{
			return CRIFS_IO_ERROR_NG;
		}

		/// [EN] Read finishes before returning, so by now it is always complete.
		/// [JP] Read は戻る前に終わっているので、この時点で必ず完了している。
		*result = CRI_TRUE;
		return CRIFS_IO_ERROR_OK;
	}

	/**
	* [EN]
	* Returns how many bytes the last Read actually produced.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直前の Read で実際に読めたバイト数を返す。
	*/
	CriFsIoError CRIAPI AudioByteStream::ReadSize(CriFsFileHn fileHandle, CriSint64* readSize)
	{
		File* file = static_cast<File*>(fileHandle);
		if (!file || !readSize)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*readSize = static_cast<CriSint64>(file->readSize_);
		return CRIFS_IO_ERROR_OK;
	}
}
