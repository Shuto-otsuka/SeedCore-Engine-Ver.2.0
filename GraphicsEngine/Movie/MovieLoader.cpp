#include <GraphicsEngine/Movie/MovieLoader.h>
#include <GraphicsEngine/Movie/MovieByteStream.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/File/FileUtility.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	MovieLoader::~MovieLoader()
	{
		Finalize();
	}

	Bool MovieLoader::Initialize(const std::string& filePath)
	{
		Finalize();

		std::filesystem::path sourceFsPath(filePath);
		std::string extension = sourceFsPath.extension().string();
		std::ranges::transform(extension, extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		Microsoft::WRL::ComPtr<IMFAttributes> attributes;
		HRESULT hr = MFCreateAttributes(attributes.ReleaseAndGetAddressOf(), 1);
		if (FAILED(hr))
		{
			SC_LOG_ERROR("MovieLoader: MFCreateAttributesに失敗しました ({})", filePath);
			return false;
		}

		attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

		if (extension == ".movie")
		{
			MovieByteStream* rawByteStream = new MovieByteStream();
			if (!rawByteStream->Open(filePath))
			{
				delete rawByteStream;
				SC_LOG_ERROR("MovieLoader: 動画キャッシュを開けませんでした ({})", filePath);
				return false;
			}

			byteStream_.Attach(rawByteStream);
			hr = MFCreateSourceReaderFromByteStream(byteStream_.Get(), attributes.Get(), sourceReader_.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				SC_LOG_ERROR("MovieLoader: 動画キャッシュからSourceReaderを生成できませんでした ({})", filePath);
				return false;
			}
		}
		else
		{
			/// [EN] Source-preferred, mirroring ModelLoader's ".crister" cache
			///      handling: only trust the sibling ".movie" cache when it's
			///      newer than the source video. Otherwise (re)open from source
			///      and (re)bake the encrypted cache. Unlike the BinaryArchive-
			///      backed caches, ".movie" has no field framing (just a 16-byte
			///      IV followed by raw AES-256-CBC ciphertext) so MovieByteStream
			///      can decrypt an arbitrary requested range without first
			///      parsing the whole file.
			/// [JP] ソース優先、ModelLoaderの".crister"キャッシュ扱いと同じ構図:
			///      隣の".movie"キャッシュは、ソース動画より新しい時だけ信用
			///      する。それ以外はソースから(再)オープンし、暗号化キャッシュを
			///      (再)ベイクする。他のBinaryArchiveベースのキャッシュと違い、
			///      ".movie"はフィールド形式を持たない(16バイトIV+生のAES-256-CBC
			///      暗号文のみ)ため、MovieByteStreamはファイル全体を解析せずに
			///      任意範囲を復号できる。
			std::filesystem::path cacheFsPath = sourceFsPath;
			cacheFsPath.replace_extension(".movie");

			Bool openedFromCache = false;
			if (std::filesystem::exists(cacheFsPath) && std::filesystem::exists(sourceFsPath) && std::filesystem::last_write_time(cacheFsPath) >= std::filesystem::last_write_time(sourceFsPath))
			{
				MovieByteStream* rawByteStream = new MovieByteStream();
				if (rawByteStream->Open(cacheFsPath.string()))
				{
					byteStream_.Attach(rawByteStream);
					hr = MFCreateSourceReaderFromByteStream(byteStream_.Get(), attributes.Get(), sourceReader_.ReleaseAndGetAddressOf());
					openedFromCache = SUCCEEDED(hr) && sourceReader_;
				}
				else
				{
					delete rawByteStream;
				}

				if (!openedFromCache)
				{
					byteStream_.Reset();
					sourceReader_.Reset();
				}
			}

			if (!openedFromCache)
			{
				std::wstring widePath(filePath.begin(), filePath.end());
				hr = MFCreateSourceReaderFromURL(widePath.c_str(), attributes.Get(), sourceReader_.ReleaseAndGetAddressOf());
				if (FAILED(hr))
				{
					SC_LOG_ERROR("MovieLoader: 動画ファイルを開けませんでした ({})", filePath);
					return false;
				}

				DynamicArray<Uint8> sourceBytes = FileUtility::LoadFileBinary(String(sourceFsPath.string()));
				if (!sourceBytes.empty())
				{
					DynamicArray<Byte> plaintext(sourceBytes.size());
					std::memcpy(plaintext.data(), sourceBytes.data(), sourceBytes.size());

					DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

					DynamicArray<Byte> iv(16);
					std::random_device randomDevice;
					for (Byte& ivByte : iv)
					{
						ivByte = static_cast<Byte>(randomDevice());
					}

					DynamicArray<Byte> ciphertext = Aes256::Encrypt(key, iv, plaintext);

					std::ofstream ofs(cacheFsPath, std::ios::binary);
					if (ofs)
					{
						ofs.write(iv.data(), iv.size());
						ofs.write(ciphertext.data(), ciphertext.size());
					}
				}
			}
		}

		sourceReader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
		sourceReader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

		Microsoft::WRL::ComPtr<IMFMediaType> outputType;
		hr = MFCreateMediaType(outputType.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			SC_LOG_ERROR("MovieLoader: MFCreateMediaTypeに失敗しました ({})", filePath);
			return false;
		}

		outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

		hr = sourceReader_->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, outputType.Get());
		if (FAILED(hr))
		{
			SC_LOG_ERROR("MovieLoader: RGB32出力フォーマットの設定に失敗しました ({})", filePath);
			return false;
		}

		Microsoft::WRL::ComPtr<IMFMediaType> currentType;
		hr = sourceReader_->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), currentType.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			SC_LOG_ERROR("MovieLoader: 出力フォーマットの取得に失敗しました ({})", filePath);
			return false;
		}

		UINT32 width = 0;
		UINT32 height = 0;
		MFGetAttributeSize(currentType.Get(), MF_MT_FRAME_SIZE, &width, &height);
		width_ = static_cast<Int>(width);
		height_ = static_cast<Int>(height);

		LONG stride = 0;
		if (FAILED(currentType->GetUINT32(MF_MT_DEFAULT_STRIDE, reinterpret_cast<UINT32*>(&stride))) || stride == 0)
		{
			stride = width_ * 4;
		}
		rowPitch_ = static_cast<Longlong>(std::abs(stride));

		PROPVARIANT durationVariant;
		PropVariantInit(&durationVariant);
		if (SUCCEEDED(sourceReader_->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &durationVariant)))
		{
			duration_ = static_cast<Double>(durationVariant.uhVal.QuadPart) / 10000000.0;
		}
		PropVariantClear(&durationVariant);

		pixelBuffer_.assign(static_cast<Size>(rowPitch_ * height_), Byte(0));

		endOfStream_ = false;
		initialized_ = (width_ > 0 && height_ > 0);

		if (!initialized_)
		{
			SC_LOG_ERROR("MovieLoader: 動画の解像度を取得できませんでした ({})", filePath);
		}

		return initialized_;
	}

	void MovieLoader::Finalize()
	{
		sourceReader_.Reset();
		byteStream_.Reset();
		pixelBuffer_.clear();
		width_ = 0;
		height_ = 0;
		rowPitch_ = 0;
		duration_ = 0.0;
		endOfStream_ = false;
		initialized_ = false;
	}

	Bool MovieLoader::ReadNextSample(Double& outTimestampSeconds)
	{
		if (!initialized_ || endOfStream_)
		{
			return false;
		}

		DWORD streamFlags = 0;
		LONGLONG timestamp = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;

		HRESULT hr = sourceReader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &streamFlags, &timestamp, sample.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			SC_LOG_WARNING("MovieLoader: ReadSampleに失敗しました");
			return false;
		}

		if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			endOfStream_ = true;
			return false;
		}

		if (!sample)
		{
			return false;
		}

		Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
		hr = sample->ConvertToContiguousBuffer(buffer.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			SC_LOG_WARNING("MovieLoader: ConvertToContiguousBufferに失敗しました");
			return false;
		}

		BYTE* data = nullptr;
		DWORD currentLength = 0;
		hr = buffer->Lock(&data, nullptr, &currentLength);
		if (FAILED(hr))
		{
			SC_LOG_WARNING("MovieLoader: MediaBuffer::Lockに失敗しました");
			return false;
		}

		Size copySize = Min<Size>(pixelBuffer_.size(), static_cast<Size>(currentLength));
		std::memcpy(pixelBuffer_.data(), data, copySize);

		buffer->Unlock();

		outTimestampSeconds = static_cast<Double>(timestamp) / 10000000.0;
		return true;
	}

	Bool MovieLoader::Seek(Double timeSeconds)
	{
		if (!initialized_)
		{
			return false;
		}

		PROPVARIANT position;
		PropVariantInit(&position);
		position.vt = VT_I8;
		position.hVal.QuadPart = static_cast<LONGLONG>(timeSeconds * 10000000.0);

		HRESULT hr = sourceReader_->SetCurrentPosition(GUID_NULL, position);
		PropVariantClear(&position);

		if (FAILED(hr))
		{
			SC_LOG_WARNING("MovieLoader: シークに失敗しました");
			return false;
		}

		endOfStream_ = false;
		return true;
	}

	const Byte* MovieLoader::GetPixelData()const
	{
		return pixelBuffer_.data();
	}

	Int MovieLoader::GetWidth()const
	{
		return width_;
	}

	Int MovieLoader::GetHeight()const
	{
		return height_;
	}

	Longlong MovieLoader::GetRowPitch()const
	{
		return rowPitch_;
	}

	Double MovieLoader::GetDuration()const
	{
		return duration_;
	}

	Bool MovieLoader::IsEndOfStream()const
	{
		return endOfStream_;
	}
}
