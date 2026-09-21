#include <GraphicsEngine/Movie/MovieByteStream.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	Bool MovieByteStream::Open(const std::string& filePath)
	{
		fileStream_.open(filePath, std::ios::binary);
		if (!fileStream_.is_open())
		{
			return false;
		}

		fileStream_.seekg(0, std::ios::end);
		Uint64 fileSize = static_cast<Uint64>(fileStream_.tellg());
		fileStream_.seekg(0, std::ios::beg);

		if (fileSize < 32 || (fileSize - 16) % 16 != 0)
		{
			return false;
		}

		fileStream_.read(reinterpret_cast<Char*>(globalIv_), 16);
		ciphertextLength_ = fileSize - 16;

		key_ = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		DynamicArray<Byte> chainIv(16);
		if (ciphertextLength_ == 16)
		{
			std::memcpy(chainIv.data(), globalIv_, 16);
		}
		else
		{
			fileStream_.seekg(static_cast<std::streamoff>(16 + ciphertextLength_ - 32));
			fileStream_.read(reinterpret_cast<Char*>(chainIv.data()), 16);
		}

		DynamicArray<Byte> lastCiphertextBlock(16);
		fileStream_.seekg(static_cast<std::streamoff>(16 + ciphertextLength_ - 16));
		fileStream_.read(reinterpret_cast<Char*>(lastCiphertextBlock.data()), 16);

		DynamicArray<Byte> lastPlaintextBlock = Aes256::DecryptUnpadded(key_, chainIv, lastCiphertextBlock);
		if (lastPlaintextBlock.empty())
		{
			return false;
		}

		Uint8 padValue = static_cast<Uint8>(lastPlaintextBlock.back());
		if (padValue == 0 || padValue > 16)
		{
			return false;
		}

		logicalLength_ = ciphertextLength_ - padValue;
		currentPosition_ = 0;

		return true;
	}

	HRESULT __stdcall MovieByteStream::QueryInterface(REFIID riid, void** ppvObject)
	{
		if (!ppvObject)
		{
			return E_POINTER;
		}

		if (riid == IID_IUnknown || riid == __uuidof(IMFByteStream))
		{
			*ppvObject = static_cast<IMFByteStream*>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	ULONG __stdcall MovieByteStream::AddRef()
	{
		return ++referenceCount_;
	}

	ULONG __stdcall MovieByteStream::Release()
	{
		ULONG count = --referenceCount_;
		if (count == 0)
		{
			delete this;
		}
		return count;
	}

	HRESULT __stdcall MovieByteStream::GetCapabilities(DWORD* capabilities)
	{
		if (!capabilities)
		{
			return E_POINTER;
		}
		*capabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE;
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::GetLength(QWORD* length)
	{
		if (!length)
		{
			return E_POINTER;
		}
		*length = logicalLength_;
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::SetLength(QWORD length)
	{
		return E_NOTIMPL;
	}

	HRESULT __stdcall MovieByteStream::GetCurrentPosition(QWORD* position)
	{
		if (!position)
		{
			return E_POINTER;
		}
		*position = currentPosition_;
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::SetCurrentPosition(QWORD position)
	{
		currentPosition_ = Min<Uint64>(position, logicalLength_);
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::IsEndOfStream(BOOL* endOfStream)
	{
		if (!endOfStream)
		{
			return E_POINTER;
		}
		*endOfStream = (currentPosition_ >= logicalLength_) ? TRUE : FALSE;
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::Read(BYTE* buffer, ULONG bufferSize, ULONG* bytesRead)
	{
		if (currentPosition_ >= logicalLength_ || bufferSize == 0)
		{
			if (bytesRead)
			{
				*bytesRead = 0;
			}
			return S_OK;
		}

		Uint64 remaining = logicalLength_ - currentPosition_;
		Uint64 toRead = Min<Uint64>(static_cast<Uint64>(bufferSize), remaining);

		Uint64 blockStart = (currentPosition_ / 16) * 16;
		Uint64 blockEnd = Min<Uint64>(((currentPosition_ + toRead + 15) / 16) * 16, ciphertextLength_);

		DynamicArray<Byte> chainIv(16);
		if (blockStart == 0)
		{
			std::memcpy(chainIv.data(), globalIv_, 16);
		}
		else
		{
			fileStream_.clear();
			fileStream_.seekg(static_cast<std::streamoff>(16 + blockStart - 16));
			fileStream_.read(reinterpret_cast<Char*>(chainIv.data()), 16);
		}

		DynamicArray<Byte> ciphertextChunk(static_cast<Size>(blockEnd - blockStart));
		fileStream_.clear();
		fileStream_.seekg(static_cast<std::streamoff>(16 + blockStart));
		fileStream_.read(reinterpret_cast<Char*>(ciphertextChunk.data()), static_cast<std::streamsize>(ciphertextChunk.size()));

		DynamicArray<Byte> decrypted = Aes256::DecryptUnpadded(key_, chainIv, ciphertextChunk);
		if (decrypted.empty())
		{
			if (bytesRead)
			{
				*bytesRead = 0;
			}
			return E_FAIL;
		}

		Uint64 offsetInChunk = currentPosition_ - blockStart;
		std::memcpy(buffer, decrypted.data() + offsetInChunk, static_cast<Size>(toRead));

		currentPosition_ += toRead;
		if (bytesRead)
		{
			*bytesRead = static_cast<ULONG>(toRead);
		}

		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::BeginRead(BYTE* buffer, ULONG bufferSize, IMFAsyncCallback* callback, IUnknown* state)
	{
		if (!callback)
		{
			return E_POINTER;
		}

		HRESULT readResult = Read(buffer, bufferSize, &pendingReadBytes_);

		Microsoft::WRL::ComPtr<IMFAsyncResult> asyncResult;
		HRESULT createResult = MFCreateAsyncResult(nullptr, callback, state, asyncResult.ReleaseAndGetAddressOf());
		if (FAILED(createResult))
		{
			return createResult;
		}

		asyncResult->SetStatus(readResult);
		return MFInvokeCallback(asyncResult.Get());
	}

	HRESULT __stdcall MovieByteStream::EndRead(IMFAsyncResult* result, ULONG* bytesRead)
	{
		if (!result || !bytesRead)
		{
			return E_POINTER;
		}

		*bytesRead = pendingReadBytes_;
		return result->GetStatus();
	}

	HRESULT __stdcall MovieByteStream::Write(const BYTE* buffer, ULONG bufferSize, ULONG* bytesWritten)
	{
		return E_NOTIMPL;
	}

	HRESULT __stdcall MovieByteStream::BeginWrite(const BYTE* buffer, ULONG bufferSize, IMFAsyncCallback* callback, IUnknown* state)
	{
		return E_NOTIMPL;
	}

	HRESULT __stdcall MovieByteStream::EndWrite(IMFAsyncResult* result, ULONG* bytesWritten)
	{
		return E_NOTIMPL;
	}

	HRESULT __stdcall MovieByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN seekOrigin, LONGLONG seekOffset, DWORD seekFlags, QWORD* currentPosition)
	{
		Int64 base = (seekOrigin == msoBegin) ? 0 : static_cast<Int64>(currentPosition_);
		Int64 target = Max<Int64>(base + seekOffset, 0);

		currentPosition_ = Min<Uint64>(static_cast<Uint64>(target), logicalLength_);

		if (currentPosition)
		{
			*currentPosition = currentPosition_;
		}

		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::Flush()
	{
		return S_OK;
	}

	HRESULT __stdcall MovieByteStream::Close()
	{
		fileStream_.close();
		return S_OK;
	}
}
