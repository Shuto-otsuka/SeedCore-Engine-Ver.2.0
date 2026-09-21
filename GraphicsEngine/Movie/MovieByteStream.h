#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class MovieByteStream :public IMFByteStream
	{
	public:
		MovieByteStream() = default;

		Bool Open(const std::string& filePath);

		HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject)override;

		ULONG __stdcall AddRef()override;

		ULONG __stdcall Release()override;

		HRESULT __stdcall GetCapabilities(DWORD* capabilities)override;

		HRESULT __stdcall GetLength(QWORD* length)override;

		HRESULT __stdcall SetLength(QWORD length)override;

		HRESULT __stdcall GetCurrentPosition(QWORD* position)override;

		HRESULT __stdcall SetCurrentPosition(QWORD position)override;

		HRESULT __stdcall IsEndOfStream(BOOL* endOfStream)override;

		HRESULT __stdcall Read(BYTE* buffer, ULONG bufferSize, ULONG* bytesRead)override;

		HRESULT __stdcall BeginRead(BYTE* buffer, ULONG bufferSize, IMFAsyncCallback* callback, IUnknown* state)override;

		HRESULT __stdcall EndRead(IMFAsyncResult* result, ULONG* bytesRead)override;

		HRESULT __stdcall Write(const BYTE* buffer, ULONG bufferSize, ULONG* bytesWritten)override;

		HRESULT __stdcall BeginWrite(const BYTE* buffer, ULONG bufferSize, IMFAsyncCallback* callback, IUnknown* state)override;

		HRESULT __stdcall EndWrite(IMFAsyncResult* result, ULONG* bytesWritten)override;

		HRESULT __stdcall Seek(MFBYTESTREAM_SEEK_ORIGIN seekOrigin, LONGLONG seekOffset, DWORD seekFlags, QWORD* currentPosition)override;

		HRESULT __stdcall Flush()override;

		HRESULT __stdcall Close()override;

	private:
		std::atomic<ULONG> referenceCount_ = 1;

		std::ifstream fileStream_;

		DynamicArray<Byte> key_;

		Byte globalIv_[16]{};

		Uint64 ciphertextLength_ = 0;

		Uint64 logicalLength_ = 0;

		Uint64 currentPosition_ = 0;

		ULONG pendingReadBytes_ = 0;
	};
}
