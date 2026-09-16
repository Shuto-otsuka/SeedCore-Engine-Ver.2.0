#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API AudioByteStream
	{
	public:
		static CriError CRIAPI SelectIo(const CriChar8* path, CriFsDeviceId* deviceID, CriFsIoInterfacePtr* ioInterface);

	private:
		static CriFsIoError CRIAPI Exists(const CriChar8* path, CriBool* result);

		static CriFsIoError CRIAPI Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* fileHandle);

		static CriFsIoError CRIAPI Close(CriFsFileHn fileHandle);

		static CriFsIoError CRIAPI GetFileSize(CriFsFileHn fileHandle, CriSint64* fileSize);

		static CriFsIoError CRIAPI Read(CriFsFileHn fileHandle, CriSint64 offset, CriSint64 readSize, void* buffer, CriSint64 bufferSize);

		static CriFsIoError CRIAPI IsReadComplete(CriFsFileHn fileHandle, CriBool* result);

		static CriFsIoError CRIAPI GetReadSize(CriFsFileHn fileHandle, CriSint64* readSize);

	private:
		struct File
		{
			HANDLE handle_ = INVALID_HANDLE_VALUE;

			Bool encrypted_ = false;

			Byte iv_[16]{};

			Uint64 ciphertextOffset_ = 0;

			Uint64 ciphertextSize_ = 0;

			Uint64 plainSize_ = 0;

			Uint64 readSize_ = 0;
		};

		static CriFsIoInterface ioInterface_;
	};
}
