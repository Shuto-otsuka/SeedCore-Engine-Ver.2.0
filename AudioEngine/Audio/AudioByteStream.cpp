#include <AudioEngine/Audio/AudioByteStream.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	CriFsIoInterface AudioByteStream::ioInterface_ =
	{
		&AudioByteStream::Exists,
		nullptr,
		nullptr,
		&AudioByteStream::Open,
		&AudioByteStream::Close,
		&AudioByteStream::GetFileSize,
		&AudioByteStream::Read,
		&AudioByteStream::IsReadComplete,
		nullptr,
		&AudioByteStream::GetReadSize,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};

	CriError CRIAPI AudioByteStream::SelectIo(const CriChar8* path, CriFsDeviceId* deviceID, CriFsIoInterfacePtr* ioInterface)
	{
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

	CriFsIoError CRIAPI AudioByteStream::Exists(const CriChar8* path, CriBool* result)
	{
		if (!result)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*result = CRI_FALSE;

		if (!path)
		{
			return CRIFS_IO_ERROR_OK;
		}

		std::string_view pathView(path);
		if (pathView.starts_with("seedcore_audio://"))
		{
			pathView.remove_prefix(std::strlen("seedcore_audio://"));
		}

		String filePath(pathView);
		*result = std::filesystem::exists(filePath.c_str()) ? CRI_TRUE : CRI_FALSE;

		return CRIFS_IO_ERROR_OK;
	}

	CriFsIoError CRIAPI AudioByteStream::Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* fileHandle)
	{
		if (!fileHandle)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*fileHandle = nullptr;

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

		String filePath(pathView);
		HANDLE handle = CreateFileW(filePath.w_str().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return CRIFS_IO_ERROR_OK;
		}

		File* file = new File();
		file->handle_ = handle;
		file->encrypted_ = encrypted;

		if (!encrypted)
		{
			LARGE_INTEGER fileSize{};
			GetFileSizeEx(handle, &fileSize);
			file->plainSize_ = static_cast<Uint64>(fileSize.QuadPart);

			*fileHandle = file;
			return CRIFS_IO_ERROR_OK;
		}

		Byte header[24]{};
		DWORD readBytes = 0;
		OVERLAPPED headerOverlapped{};
		if (!ReadFile(handle, header, 24, &readBytes, &headerOverlapped) || readBytes != 24 || std::memcmp(header, "SCAUDIO\0", 8) != 0)
		{
			CloseHandle(handle);
			delete file;
			return CRIFS_IO_ERROR_NG_INVALID_DATA;
		}

		Uint32 blobCount = 0;
		std::memcpy(&blobCount, header + 16, 4);

		Uint64 blobOffset = 0;
		Uint64 blobEncryptedSize = 0;
		Uint64 blobPlainSize = 0;
		for (Uint32 blobIndex = 0; blobIndex < blobCount && blobIndex < 2; ++blobIndex)
		{
			Byte entry[32]{};
			OVERLAPPED entryOverlapped{};
			entryOverlapped.Offset = static_cast<DWORD>(24 + blobIndex * 32);
			if (!ReadFile(handle, entry, 32, &readBytes, &entryOverlapped) || readBytes != 32)
			{
				CloseHandle(handle);
				delete file;
				return CRIFS_IO_ERROR_NG_INVALID_DATA;
			}

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

		if (blobEncryptedSize <= 16)
		{
			CloseHandle(handle);
			delete file;
			return CRIFS_IO_ERROR_NG_NO_ENTRY;
		}

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

	CriFsIoError CRIAPI AudioByteStream::Close(CriFsFileHn fileHandle)
	{
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

	CriFsIoError CRIAPI AudioByteStream::GetFileSize(CriFsFileHn fileHandle, CriSint64* fileSize)
	{
		File* file = static_cast<File*>(fileHandle);
		if (!file || !fileSize)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*fileSize = static_cast<CriSint64>(file->plainSize_);
		return CRIFS_IO_ERROR_OK;
	}

	CriFsIoError CRIAPI AudioByteStream::Read(CriFsFileHn fileHandle, CriSint64 offset, CriSint64 readSize, void* buffer, CriSint64 bufferSize)
	{
		File* file = static_cast<File*>(fileHandle);
		if (!file || !buffer)
		{
			return CRIFS_IO_ERROR_NG;
		}

		file->readSize_ = 0;

		if (offset < 0 || readSize <= 0 || static_cast<Uint64>(offset) >= file->plainSize_)
		{
			return CRIFS_IO_ERROR_OK;
		}

		Uint64 toRead = Min<Uint64>(static_cast<Uint64>(readSize), file->plainSize_ - static_cast<Uint64>(offset));
		toRead = Min<Uint64>(toRead, static_cast<Uint64>(bufferSize));

		DWORD readBytes = 0;

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

		Uint64 blockStart = (static_cast<Uint64>(offset) / 16) * 16;
		Uint64 blockEnd = Min<Uint64>(((static_cast<Uint64>(offset) + toRead + 15) / 16) * 16, file->ciphertextSize_);

		DynamicArray<Byte> chainIv(16);
		if (blockStart == 0)
		{
			std::memcpy(chainIv.data(), file->iv_, 16);
		}
		else
		{
			Uint64 chainOffset = file->ciphertextOffset_ + blockStart - 16;
			OVERLAPPED chainOverlapped{};
			chainOverlapped.Offset = static_cast<DWORD>(chainOffset & 0xFFFFFFFF);
			chainOverlapped.OffsetHigh = static_cast<DWORD>(chainOffset >> 32);
			if (!ReadFile(file->handle_, chainIv.data(), 16, &readBytes, &chainOverlapped) || readBytes != 16)
			{
				return CRIFS_IO_ERROR_NG;
			}
		}

		DynamicArray<Byte> ciphertextChunk(static_cast<Size>(blockEnd - blockStart));
		Uint64 chunkOffset = file->ciphertextOffset_ + blockStart;
		OVERLAPPED chunkOverlapped{};
		chunkOverlapped.Offset = static_cast<DWORD>(chunkOffset & 0xFFFFFFFF);
		chunkOverlapped.OffsetHigh = static_cast<DWORD>(chunkOffset >> 32);
		if (!ReadFile(file->handle_, ciphertextChunk.data(), static_cast<DWORD>(ciphertextChunk.size()), &readBytes, &chunkOverlapped) || readBytes != ciphertextChunk.size())
		{
			return CRIFS_IO_ERROR_NG;
		}

		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));
		DynamicArray<Byte> decrypted = Aes256::DecryptUnpadded(key, chainIv, ciphertextChunk);
		if (decrypted.empty())
		{
			return CRIFS_IO_ERROR_NG;
		}

		Uint64 offsetInChunk = static_cast<Uint64>(offset) - blockStart;
		std::memcpy(buffer, decrypted.data() + offsetInChunk, static_cast<Size>(toRead));

		file->readSize_ = toRead;
		return CRIFS_IO_ERROR_OK;
	}

	CriFsIoError CRIAPI AudioByteStream::IsReadComplete(CriFsFileHn fileHandle, CriBool* result)
	{
		if (!result)
		{
			return CRIFS_IO_ERROR_NG;
		}

		*result = CRI_TRUE;
		return CRIFS_IO_ERROR_OK;
	}

	CriFsIoError CRIAPI AudioByteStream::GetReadSize(CriFsFileHn fileHandle, CriSint64* readSize)
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
