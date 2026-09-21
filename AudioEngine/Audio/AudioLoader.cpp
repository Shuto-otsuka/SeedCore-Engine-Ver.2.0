#include <AudioEngine/Audio/AudioLoader.h>
#include <FoundationEngine/File/FileUtility.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Initializes COM and Media Foundation for MP3 decoding.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* MP3 のデコードに使う COM と Media Foundation を初期化する。
	*/
	AudioLoader::AudioLoader()
	{
		/// [EN] Track successful initialization so this instance balances only the calls it owns.
		/// [JP] このインスタンスが所有する初期化呼び出しだけを終了できるよう、成功状態を保持する。
		HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ownsComInitialize_ = (comResult == S_OK || comResult == S_FALSE);

		HRESULT mfResult = MFStartup(MF_VERSION, MFSTARTUP_LITE);
		mfStarted_ = SUCCEEDED(mfResult);

		if (!mfStarted_)
		{
			SC_LOG_WARNING("AudioLoader: MFStartupに失敗しました - mp3の変換は無効です");
		}
	}

	/**
	* [EN]
	* Shuts down the facilities initialized by this loader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このローダーが初期化した機能を終了する。
	*/
	AudioLoader::~AudioLoader()
	{
		if (mfStarted_)
		{
			MFShutdown();
		}

		if (ownsComInitialize_)
		{
			CoUninitialize();
		}
	}

	/**
	* [EN]
	* Loads a Sound from an up-to-date .audio cache, baking one from the
	* source file when needed. Returns a null handle on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 最新の .audio キャッシュから Sound を読み込み、必要なら素材ファイル
	* からキャッシュを作成する。失敗時は null ハンドルを返す。
	*/
	Handle<Sound> AudioLoader::Load(LoaderSystem& loader, String filePath)
	{
		/// [EN] Resolve the source and cache paths, rebuilding stale caches before loading.
		/// [JP] 素材とキャッシュのパスを解決し、古いキャッシュは読み込み前に再作成する。
		std::filesystem::path sourceFsPath(filePath.c_str());
		std::string extension = sourceFsPath.extension().string();
		std::ranges::transform(extension, extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		if (extension == ".awb")
		{
			return Handle<Sound>::null();
		}

		std::filesystem::path cacheFsPath = sourceFsPath;
		if (extension != ".audio")
		{
			cacheFsPath.replace_extension(".audio");

			Bool cacheValid = std::filesystem::exists(cacheFsPath) && std::filesystem::exists(sourceFsPath) && std::filesystem::last_write_time(cacheFsPath) >= std::filesystem::last_write_time(sourceFsPath);
			if (!cacheValid && !Bake(String(sourceFsPath.string()), String(cacheFsPath.string())))
			{
				SC_LOG_ERROR("AudioLoader: .audioの作成に失敗しました ({})", sourceFsPath.string());
				return Handle<Sound>::null();
			}
		}

		/// [EN] Read and validate the fixed header of the encrypted audio container.
		/// [JP] 暗号化音声コンテナの固定ヘッダーを読み込み、妥当性を検証する。
		std::ifstream ifs(cacheFsPath, std::ios::binary);
		if (!ifs)
		{
			SC_LOG_ERROR("AudioLoader: .audioを開けませんでした ({})", cacheFsPath.string());
			return Handle<Sound>::null();
		}

		Byte magic[8]{};
		Uint32 version = 0;
		Uint32 typeValue = 0;
		Uint32 blobCount = 0;
		Uint32 reserved = 0;
		ifs.read(magic, 8);
		ifs.read(reinterpret_cast<Char*>(&version), 4);
		ifs.read(reinterpret_cast<Char*>(&typeValue), 4);
		ifs.read(reinterpret_cast<Char*>(&blobCount), 4);
		ifs.read(reinterpret_cast<Char*>(&reserved), 4);

		if (!ifs || std::memcmp(magic, "SCAUDIO\0", 8) != 0 || version != 1 || blobCount == 0 || blobCount > 2 || typeValue > static_cast<Uint32>(SoundType::Wave))
		{
			SC_LOG_ERROR("AudioLoader: .audioのヘッダーが不正です ({})", cacheFsPath.string());
			return Handle<Sound>::null();
		}

		/// [EN] Locate the in-memory body and record whether a streamed AWB blob is present.
		/// [JP] メモリへ読む本体の位置を取得し、ストリーミング用 AWB ブロブの有無を記録する。
		Uint64 mainOffset = 0;
		Uint64 mainEncryptedSize = 0;
		Bool hasStreamBlob = false;
		for (Uint32 blobIndex = 0; blobIndex < blobCount; ++blobIndex)
		{
			Uint32 blobID = 0;
			Uint32 blobReserved = 0;
			Uint64 offset = 0;
			Uint64 encryptedSize = 0;
			Uint64 plainSize = 0;
			ifs.read(reinterpret_cast<Char*>(&blobID), 4);
			ifs.read(reinterpret_cast<Char*>(&blobReserved), 4);
			ifs.read(reinterpret_cast<Char*>(&offset), 8);
			ifs.read(reinterpret_cast<Char*>(&encryptedSize), 8);
			ifs.read(reinterpret_cast<Char*>(&plainSize), 8);

			if (blobID == 0)
			{
				mainOffset = offset;
				mainEncryptedSize = encryptedSize;
			}
			else if (blobID == 1)
			{
				hasStreamBlob = true;
			}
		}

		if (!ifs || mainEncryptedSize <= 16)
		{
			SC_LOG_ERROR("AudioLoader: .audioの本体データが不正です ({})", cacheFsPath.string());
			return Handle<Sound>::null();
		}

		/// [EN] Read the initialization vector and ciphertext, then decrypt the main body.
		/// [JP] 初期化ベクトルと暗号文を読み込み、本体データを復号する。
		DynamicArray<Byte> iv(16);
		DynamicArray<Byte> ciphertext(static_cast<Size>(mainEncryptedSize - 16));
		ifs.seekg(static_cast<std::streamoff>(mainOffset));
		ifs.read(iv.data(), 16);
		ifs.read(ciphertext.data(), static_cast<std::streamsize>(ciphertext.size()));
		if (!ifs)
		{
			SC_LOG_ERROR("AudioLoader: .audioの本体データを読み込めませんでした ({})", cacheFsPath.string());
			return Handle<Sound>::null();
		}

		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));
		DynamicArray<Byte> plaintext = Aes256::Decrypt(key, iv, ciphertext);
		if (plaintext.empty())
		{
			SC_LOG_ERROR("AudioLoader: .audioの復号に失敗しました ({})", cacheFsPath.string());
			return Handle<Sound>::null();
		}

		/// [EN] Store the decoded body in a stable Sound slot addressed by the returned handle.
		/// [JP] 復号した本体を、返却ハンドルで参照する安定した Sound スロットへ格納する。
		Handle<Sound> handle = pool_.Create();
		Sound* sound = pool_.Get(handle);
		if (!sound)
		{
			return Handle<Sound>::null();
		}

		sound->type_ = static_cast<SoundType>(typeValue);
		sound->data_ = std::move(plaintext);

		if (hasStreamBlob)
		{
			sound->awbPath_ = String("seedcore_audio://" + std::filesystem::absolute(cacheFsPath).generic_string());
		}

		/// [EN] Register cue-sheet data with CRI when the global ACF is available.
		/// [JP] グローバル ACF を利用できる場合は、キューシートデータを CRI へ登録する。
		if (sound->type_ == SoundType::CueSheet)
		{
			CriAtomExAcfInfo acfInfo{};
			if (criAtomExAcf_GetAcfInfo(&acfInfo) != CRI_TRUE)
			{
				SC_LOG_WARNING("AudioLoader: ACFが登録されていないため、キューシートをCRIへ登録できません ({})", cacheFsPath.string());
			}
			else
			{
				sound->acbHandle_ = criAtomExAcb_LoadAcbData(sound->data_.data(), static_cast<CriSint32>(sound->data_.size()), nullptr, sound->awbPath_.str().empty() ? nullptr : sound->awbPath_.c_str(), nullptr, 0);
				if (!sound->acbHandle_)
				{
					SC_LOG_ERROR("AudioLoader: ACBの読み込みに失敗しました ({})", cacheFsPath.string());
					pool_.Destroy(handle);
					return Handle<Sound>::null();
				}
			}
		}

		return handle;
	}

	/**
	* [EN]
	* Resolves a Sound handle to its pooled object.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Sound ハンドルからプール内のオブジェクトを取得する。
	*/
	Sound* AudioLoader::Get(const Handle<Sound>& handle)
	{
		return pool_.Get(handle);
	}

	/**
	* [EN]
	* Releases CRI data associated with a Sound and destroys its handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Sound に関連する CRI データを解放し、ハンドルを破棄する。
	*/
	void AudioLoader::Clear(Handle<Sound>& handle)noexcept
	{
		/// [EN] Release the ACB before returning its backing Sound slot to the pool.
		/// [JP] Sound の格納スロットをプールへ返す前に、ACB を解放する。
		if (Sound* sound = pool_.Get(handle))
		{
			if (sound->acbHandle_)
			{
				criAtomExAcb_Release(sound->acbHandle_);
				sound->acbHandle_ = nullptr;
			}
		}

		pool_.Destroy(handle);
	}

	/**
	* [EN]
	* Converts a supported audio source into an encrypted .audio cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 対応する音声素材を暗号化された .audio キャッシュへ変換する。
	*/
	Bool AudioLoader::Bake(String sourcePath, String cachePath)
	{
		/// [EN] Select the conversion path from the source extension.
		/// [JP] 素材の拡張子に応じて変換経路を選択する。
		std::filesystem::path sourceFsPath(sourcePath.c_str());
		std::string extension = sourceFsPath.extension().string();
		std::ranges::transform(extension, extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		SoundType type = SoundType::Wave;
		DynamicArray<Byte> mainData;
		DynamicArray<Byte> streamData;

		if (extension == ".acb" || extension == ".wav")
		{
			/// [EN] Preserve ACB and wave bytes directly as the main body.
			/// [JP] ACB と wave のバイト列は、そのまま本体データとして保持する。
			type = (extension == ".acb") ? SoundType::CueSheet : SoundType::Wave;

			DynamicArray<Uint8> sourceBytes = FileUtility::LoadFileBinary(String(sourceFsPath.string()));
			if (sourceBytes.empty())
			{
				return false;
			}
			mainData.resize(sourceBytes.size());
			std::memcpy(mainData.data(), sourceBytes.data(), sourceBytes.size());

			if (extension == ".acb")
			{
				/// [EN] Package a sibling AWB as the optional streamed-data blob when present.
				/// [JP] 同階層の AWB が存在する場合は、任意のストリームデータブロブとして格納する。
				std::filesystem::path awbFsPath = sourceFsPath;
				awbFsPath.replace_extension(".awb");
				if (std::filesystem::exists(awbFsPath))
				{
					DynamicArray<Uint8> streamBytes = FileUtility::LoadFileBinary(String(awbFsPath.string()));
					streamData.resize(streamBytes.size());
					std::memcpy(streamData.data(), streamBytes.data(), streamBytes.size());
				}
			}
		}
		else if (extension == ".mp3")
		{
			/// [EN] Decode MP3 input to PCM through Media Foundation.
			/// [JP] Media Foundation を通じて MP3 入力を PCM へデコードする。
			if (!mfStarted_)
			{
				return false;
			}

			std::wstring widePath = sourceFsPath.wstring();
			Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader;
			if (FAILED(MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, sourceReader.ReleaseAndGetAddressOf())))
			{
				SC_LOG_ERROR("AudioLoader: mp3を開けませんでした ({})", sourceFsPath.string());
				return false;
			}

			sourceReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
			sourceReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);

			Microsoft::WRL::ComPtr<IMFMediaType> outputType;
			if (FAILED(MFCreateMediaType(outputType.ReleaseAndGetAddressOf())))
			{
				return false;
			}
			outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
			outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

			if (FAILED(sourceReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, outputType.Get())))
			{
				SC_LOG_ERROR("AudioLoader: mp3のPCM出力を設定できませんでした ({})", sourceFsPath.string());
				return false;
			}

			Microsoft::WRL::ComPtr<IMFMediaType> currentType;
			if (FAILED(sourceReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), currentType.ReleaseAndGetAddressOf())))
			{
				return false;
			}

			WAVEFORMATEX* waveFormat = nullptr;
			Uint32 waveFormatSize = 0;
			if (FAILED(MFCreateWaveFormatExFromMFMediaType(currentType.Get(), &waveFormat, &waveFormatSize)))
			{
				return false;
			}

			Uint16 channels = waveFormat->nChannels;
			Uint32 samplesPerSecond = waveFormat->nSamplesPerSec;
			Uint16 bitsPerSample = waveFormat->wBitsPerSample;
			CoTaskMemFree(waveFormat);

			DynamicArray<Byte> pcmData;
			while (true)
			{
				DWORD streamFlags = 0;
				Microsoft::WRL::ComPtr<IMFSample> sample;
				if (FAILED(sourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &streamFlags, nullptr, sample.ReleaseAndGetAddressOf())))
				{
					break;
				}

				if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
				{
					break;
				}

				if (!sample)
				{
					continue;
				}

				Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
				if (FAILED(sample->ConvertToContiguousBuffer(buffer.ReleaseAndGetAddressOf())))
				{
					break;
				}

				BYTE* data = nullptr;
				DWORD currentLength = 0;
				if (FAILED(buffer->Lock(&data, nullptr, &currentLength)))
				{
					break;
				}

				Size offset = pcmData.size();
				pcmData.resize(offset + currentLength);
				std::memcpy(pcmData.data() + offset, data, currentLength);

				buffer->Unlock();
			}

			if (pcmData.empty())
			{
				SC_LOG_ERROR("AudioLoader: mp3のデコード結果が空でした ({})", sourceFsPath.string());
				return false;
			}

			/// [EN] Wrap decoded PCM samples in a standard wave container.
			/// [JP] デコードした PCM サンプルを標準的な wave コンテナへ格納する。
			Uint16 blockAlign = static_cast<Uint16>(channels * (bitsPerSample / 8));
			Uint32 bytesPerSecond = samplesPerSecond * blockAlign;
			Uint32 dataSize = static_cast<Uint32>(pcmData.size());
			Uint32 riffSize = 36 + dataSize;
			Uint32 formatSize = 16;
			Uint16 formatTag = 1;

			mainData.resize(44 + pcmData.size());
			Byte* writePointer = mainData.data();
			std::memcpy(writePointer, "RIFF", 4); writePointer += 4;
			std::memcpy(writePointer, &riffSize, 4); writePointer += 4;
			std::memcpy(writePointer, "WAVE", 4); writePointer += 4;
			std::memcpy(writePointer, "fmt ", 4); writePointer += 4;
			std::memcpy(writePointer, &formatSize, 4); writePointer += 4;
			std::memcpy(writePointer, &formatTag, 2); writePointer += 2;
			std::memcpy(writePointer, &channels, 2); writePointer += 2;
			std::memcpy(writePointer, &samplesPerSecond, 4); writePointer += 4;
			std::memcpy(writePointer, &bytesPerSecond, 4); writePointer += 4;
			std::memcpy(writePointer, &blockAlign, 2); writePointer += 2;
			std::memcpy(writePointer, &bitsPerSample, 2); writePointer += 2;
			std::memcpy(writePointer, "data", 4); writePointer += 4;
			std::memcpy(writePointer, &dataSize, 4); writePointer += 4;
			std::memcpy(writePointer, pcmData.data(), pcmData.size());
		}
		else
		{
			return false;
		}

		/// [EN] Encrypt each populated blob with an independent initialization vector.
		/// [JP] 格納する各ブロブを、それぞれ独立した初期化ベクトルで暗号化する。
		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));
		std::random_device randomDevice;

		DynamicArray<Byte> mainIv(16);
		for (Byte& ivByte : mainIv)
		{
			ivByte = static_cast<Byte>(randomDevice());
		}
		DynamicArray<Byte> mainCiphertext = Aes256::Encrypt(key, mainIv, mainData);

		DynamicArray<Byte> streamIv(16);
		DynamicArray<Byte> streamCiphertext;
		if (!streamData.empty())
		{
			for (Byte& ivByte : streamIv)
			{
				ivByte = static_cast<Byte>(randomDevice());
			}
			streamCiphertext = Aes256::Encrypt(key, streamIv, streamData);
		}

		/// [EN] Calculate the container header and aligned blob layout.
		/// [JP] コンテナのヘッダーと、境界調整したブロブ配置を算出する。
		Uint32 version = 1;
		Uint32 typeValue = static_cast<Uint32>(type);
		Uint32 blobCount = streamData.empty() ? 1 : 2;
		Uint32 reserved = 0;

		Uint64 mainOffset = 24 + static_cast<Uint64>(blobCount) * 32;
		Uint64 mainEncryptedSize = 16 + mainCiphertext.size();
		Uint64 mainPlainSize = mainData.size();

		Uint64 streamOffset = ((mainOffset + mainEncryptedSize + 4095) / 4096) * 4096;
		Uint64 streamEncryptedSize = streamCiphertext.empty() ? 0 : 16 + streamCiphertext.size();
		Uint64 streamPlainSize = streamData.size();

		/// [EN] Write metadata followed by the encrypted main and optional stream blobs.
		/// [JP] メタデータに続けて、暗号化した本体と任意のストリームブロブを書き込む。
		std::ofstream ofs(cachePath.c_str(), std::ios::binary);
		if (!ofs)
		{
			return false;
		}

		ofs.write("SCAUDIO\0", 8);
		ofs.write(reinterpret_cast<const Char*>(&version), 4);
		ofs.write(reinterpret_cast<const Char*>(&typeValue), 4);
		ofs.write(reinterpret_cast<const Char*>(&blobCount), 4);
		ofs.write(reinterpret_cast<const Char*>(&reserved), 4);

		Uint32 mainBlobID = 0;
		ofs.write(reinterpret_cast<const Char*>(&mainBlobID), 4);
		ofs.write(reinterpret_cast<const Char*>(&reserved), 4);
		ofs.write(reinterpret_cast<const Char*>(&mainOffset), 8);
		ofs.write(reinterpret_cast<const Char*>(&mainEncryptedSize), 8);
		ofs.write(reinterpret_cast<const Char*>(&mainPlainSize), 8);

		if (blobCount == 2)
		{
			Uint32 streamBlobID = 1;
			ofs.write(reinterpret_cast<const Char*>(&streamBlobID), 4);
			ofs.write(reinterpret_cast<const Char*>(&reserved), 4);
			ofs.write(reinterpret_cast<const Char*>(&streamOffset), 8);
			ofs.write(reinterpret_cast<const Char*>(&streamEncryptedSize), 8);
			ofs.write(reinterpret_cast<const Char*>(&streamPlainSize), 8);
		}

		ofs.write(mainIv.data(), static_cast<std::streamsize>(mainIv.size()));
		ofs.write(mainCiphertext.data(), static_cast<std::streamsize>(mainCiphertext.size()));

		if (blobCount == 2)
		{
			DynamicArray<Byte> padding(static_cast<Size>(streamOffset - (mainOffset + mainEncryptedSize)), Byte(0));
			ofs.write(padding.data(), static_cast<std::streamsize>(padding.size()));
			ofs.write(streamIv.data(), static_cast<std::streamsize>(streamIv.size()));
			ofs.write(streamCiphertext.data(), static_cast<std::streamsize>(streamCiphertext.size()));
		}

		return static_cast<Bool>(ofs);
	}
}
