#include <AudioEngine/CRI/CriManager.h>
#include <AudioEngine/CRI/CriAllocator.h>
#include <AudioEngine/Audio/AudioByteStream.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	/**
	* [EN]
	* Initializes CRI, loads the saved bindings, registers the ACF and
	* applies the saved volumes, then allocates the voice pools and the
	* streaming bandwidth manager. Returns false if any step fails.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI を初期化し、保存済みの設定を読み、ACF を登録して保存済みの音量を
	* 反映したあと、ボイスプールとストリーミング帯域の管理を確保する。
	* どこかで失敗したら false を返す。
	*/
	Bool CriManager::Initialize()
	{
		/// [EN] Hooks set before initialization: error log, memory from the process heap, and file I/O that can read encrypted .audio caches.
		/// [JP] 初期化の前に登録するフック: エラーログ、プロセスのヒープからのメモリ確保、暗号化された .audio キャッシュを読めるファイル I/O。
		criErr_SetCallback(ScCriErrorCallback);
		criAtomEx_SetUserAllocator(ScCriMalloc, ScCriFree, nullptr);
		criFs_SetUserAllocator(ScCriMalloc, ScCriFree, nullptr);
		criFs_SetSelectIoCallback(&AudioByteStream::SelectIo);

		/// [EN] File system limits: how many files and bindings can be open at once, and the longest path.
		/// [JP] ファイルシステムの上限。同時に開けるファイル数とバインド数、パスの最大長。
		CriFsConfig fileSystemConfig{};
		criFs_SetDefaultConfig(&fileSystemConfig);
		fileSystemConfig.num_binders = 256;
		fileSystemConfig.max_binds = 256;
		fileSystemConfig.num_loaders = 64;
		fileSystemConfig.max_files = 64;
		fileSystemConfig.max_path = 512;

		/// [EN] Atom limits; CRI runs its own threads, and positions use DirectX's left-handed coordinates.
		/// [JP] Atom の上限。CRI は自前のスレッドで動き、位置は DirectX と同じ左手座標系で渡す。
		CriAtomExConfig_WASAPI config{};
		criAtomEx_SetDefaultConfig_WASAPI(&config);
		config.atom_ex.fs_config = &fileSystemConfig;
		config.atom_ex.thread_model = CRIATOMEX_THREAD_MODEL_MULTI;
		config.atom_ex.max_virtual_voices = 512;
		config.atom_ex.max_sequences = 512;
		config.atom_ex.max_tracks = 1024;
		config.atom_ex.max_track_items = 1024;
		config.atom_ex.max_parameter_blocks = 16384;
		config.atom_ex.coordinate_system = CRIATOMEX_COORDINATE_SYSTEM_LEFT_HANDED;

		/// [EN] The server runs at 60 Hz and applies parameter changes every tick; pitch may shift up to 2400 cents (two octaves).
		/// [JP] サーバーは 60 Hz で動き、パラメーターの変更を毎回反映する。ピッチは 2400 セント(2オクターブ)まで変えられる。
		config.atom_ex.server_frequency = 60.0f;
		config.atom_ex.parameter_update_interval = 1;
		config.atom_ex.max_pitch = 2400.0f;

		/// [EN] Mixer output: 48 kHz, 5.1 channels, mapped to whatever speakers the device has.
		/// [JP] ミキサーの出力。48 kHz・5.1ch で、デバイスのスピーカー構成に自動で割り当てる。
		config.asr.server_frequency = config.atom_ex.server_frequency;
		config.asr.output_sampling_rate = 48000;
		config.asr.output_channels = 6;
		config.asr.speaker_mapping = CRIATOM_SPEAKER_MAPPING_AUTO;

		criAtomEx_Initialize_WASAPI(&config, nullptr, 0);

		if (!criAtomEx_IsInitialized())
		{
			return false;
		}

		/// [EN] The saved ACF and volumes; a missing file just leaves them empty and at their defaults.
		/// [JP] 保存済みの ACF と音量。ファイルが無ければ、空と既定値のまま進む。
		Load();

		/// [EN] In the editor, an .acf rebuilt by Atom Craft replaces the saved copy when its bytes differ.
		/// [JP] エディタでは、Atom Craft がビルドし直した .acf の中身が違えば、保存済みのものと置き換える。
		Bool masterDirty = false;
		if (!masterPath_.view().empty())
		{
			std::ifstream masterStream(masterPath_.c_str(), std::ios::binary | std::ios::ate);
			if (masterStream)
			{
				DynamicArray<Byte> masterData(static_cast<Size>(masterStream.tellg()));
				masterStream.seekg(0);
				masterStream.read(masterData.data(), static_cast<std::streamsize>(masterData.size()));

				if (masterStream && !masterData.empty() && (masterData.size() != acfData_.size() || std::memcmp(masterData.data(), acfData_.data(), masterData.size()) != 0))
				{
					acfData_ = std::move(masterData);
					masterDirty = true;
				}
			}
			else
			{
				SC_LOG_WARNING("CriManager: ACFファイルを開けませんでした ({})", masterPath_.str());
			}
		}

		if (!acfData_.empty())
		{
			if (criAtomEx_RegisterAcfData(acfData_.data(), static_cast<CriSint32>(acfData_.size()), nullptr, 0) == CRI_TRUE)
			{
				/// [EN] Records each category's ACF volume as its default and applies the saved volume over it.
				/// [JP] 各カテゴリの ACF の音量を既定値として記録し、保存済みの音量があればそちらを反映する。
				CriSint32 categoryCount = criAtomExAcf_GetNumCategories();
				for (CriSint32 categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex)
				{
					CriAtomExCategoryInfo categoryInfo{};
					if (criAtomExAcf_GetCategoryInfo(static_cast<CriUint16>(categoryIndex), &categoryInfo) != CRI_TRUE || !categoryInfo.name)
					{
						continue;
					}

					String categoryName(categoryInfo.name);
					categoryNames_.push_back(categoryName);
					defaultCategoryVolumes_.insert({ categoryName, categoryInfo.volume });

					Float volume = categoryVolumes_.contains(categoryName) ? categoryVolumes_.at(categoryName) : categoryInfo.volume;
					criAtomExCategory_SetVolumeByName(categoryName.c_str(), volume);
				}

				/// [EN] Bus 0 is the master bus.
				/// [JP] バス 0 がマスターバス。
				criAtomExAsr_SetBusVolume(0, masterVolume_);

				/// [EN] Saves the new ACF so the runtime gets it without the .acf file.
				/// [JP] 新しい ACF を保存し、ランタイムが .acf ファイル無しで使えるようにする。
				if (masterDirty)
				{
					Save();
				}
			}
			else
			{
				SC_LOG_ERROR("CriManager: ACFの登録に失敗しました");
				acfData_.clear();
			}
		}

		/// [EN] 128 voices for Wave data, up to 5.1 channels at 96 kHz, able to stream.
		/// [JP] Wave データ用のボイスを 128 個。最大 5.1ch・96 kHz で、ストリーミングも可能。
		CriAtomExWaveVoicePoolConfig waveConfig{};
		criAtomExVoicePool_SetDefaultConfigForWaveVoicePool(&waveConfig);
		waveConfig.num_voices = 128;
		waveConfig.player_config.max_channels = 6;
		waveConfig.player_config.max_sampling_rate = 96000;
		waveConfig.player_config.streaming_flag = CRI_TRUE;

		waveVoicePool_ = criAtomExVoicePool_AllocateWaveVoicePool(&waveConfig, nullptr, 0);
		if (!waveVoicePool_)
		{
			SC_LOG_ERROR("CriManager: Waveボイスプールの確保に失敗しました");
			return false;
		}

		/// [EN] 128 voices for ADX/HCA with the same limits.
		/// [JP] ADX/HCA 用のボイスを、同じ上限で 128 個。
		CriAtomExStandardVoicePoolConfig standardConfig{};
		criAtomExVoicePool_SetDefaultConfigForStandardVoicePool(&standardConfig);
		standardConfig.num_voices = 128;
		standardConfig.player_config.max_channels = 6;
		standardConfig.player_config.max_sampling_rate = 96000;
		standardConfig.player_config.streaming_flag = CRI_TRUE;

		standardVoicePool_ = criAtomExVoicePool_AllocateStandardVoicePool(&standardConfig, nullptr, 0);
		if (!standardVoicePool_)
		{
			SC_LOG_ERROR("CriManager: Standardボイスプールの確保に失敗しました");
			return false;
		}

		/// [EN] Up to 8 audio streams sharing 12,288,000 bps (8 × 48 kHz 16-bit stereo); no video streams.
		/// [JP] 音声ストリームは最大 8 本で、12,288,000 bps(48 kHz・16bit・ステレオ 8 本分)を分け合う。動画のストリームは無し。
		CriAtomDbasConfig dbasConfig{};
		criAtomDbas_SetDefaultConfig(&dbasConfig);
		dbasConfig.max_streams = 8;
		dbasConfig.max_bps = 12288000;
		dbasConfig.max_mana_streams = 0;
		dbasConfig.max_mana_bps = 0;

		dbasID_ = criAtomExDbas_Create(&dbasConfig, nullptr, 0);
		if (dbasID_ == CRIATOMEXDBAS_ILLEGAL_ID)
		{
			SC_LOG_ERROR("CriManager: D-BASの作成に失敗しました");
			return false;
		}

		return true;
	}

	/**
	* [EN]
	* Runs CRI's per-frame server processing; called once every frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CRI の毎フレームのサーバー処理を行う。毎フレーム1回呼ぶ。
	*/
	void CriManager::Execute()
	{
		criAtomEx_ExecuteMain();
	}

	/**
	* [EN]
	* Sets the path of the .acf file that Atom Craft writes. Used by the
	* editor to pick up a rebuilt ACF; the runtime leaves it empty.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Atom Craft が出力する .acf ファイルのパスを設定する。エディタが
	* ビルドし直した ACF を取り込むのに使い、ランタイムは空のままにする。
	*/
	void CriManager::MasterPath(const String& path)
	{
		masterPath_ = path;
	}

	/**
	* [EN]
	* Sets the master volume and applies it to the master bus at once.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マスター音量を設定し、すぐにマスターバスへ反映する。
	*/
	void CriManager::MasterVolume(Float volume)
	{
		masterVolume_ = volume;

		/// [EN] Bus 0 is the master bus.
		/// [JP] バス 0 がマスターバス。
		criAtomExAsr_SetBusVolume(0, masterVolume_);
	}

	/**
	* [EN]
	* Sets the master volume that ResetVolume returns to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ResetVolume で戻す先のマスター音量を設定する。
	*/
	void CriManager::DefaultMasterVolume(Float volume)
	{
		defaultMasterVolume_ = volume;
	}

	/**
	* [EN]
	* Sets the volume of a category (such as BGM or SE) and applies it
	* at once.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カテゴリ(BGM や SE など)の音量を設定し、すぐに反映する。
	*/
	void CriManager::CategoryVolume(const String& category, Float volume)
	{
		/// [EN] Kept so it is saved and reapplied over the ACF's value on the next start.
		/// [JP] 保存して次回の起動時に ACF の値より優先して反映するため、記録しておく。
		if (categoryVolumes_.contains(category))
		{
			categoryVolumes_.at(category) = volume;
		}
		else
		{
			categoryVolumes_.insert({ category, volume });
		}

		criAtomExCategory_SetVolumeByName(category.c_str(), volume);
	}

	/**
	* [EN]
	* Returns the master volume to its default and every category to the
	* volume defined in the ACF.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マスター音量を既定値に、全カテゴリを ACF で定めた音量に戻す。
	*/
	void CriManager::ResetVolume()
	{
		/// [EN] Dropping the user's category volumes lets the ACF's values apply again.
		/// [JP] ユーザーのカテゴリ音量を消すと、再び ACF の値が使われる。
		categoryVolumes_.clear();
		masterVolume_ = defaultMasterVolume_;

		for (const String& category : categoryNames_)
		{
			Float volume = defaultCategoryVolumes_.contains(category) ? defaultCategoryVolumes_.at(category) : 1.0f;
			criAtomExCategory_SetVolumeByName(category.c_str(), volume);
		}

		criAtomExAsr_SetBusVolume(0, masterVolume_);
	}

	/**
	* [EN]
	* Reads the ACF and the volumes from AudioBindings.scg. Returns false
	* if the file cannot be read.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* AudioBindings.scg から ACF と音量を読み込む。ファイルを読めなければ
	* false を返す。
	*/
	Bool CriManager::Load()
	{
		BinaryInputArchive archive;
		if (!archive.Read(String("../UserProject/Assets/Config/AudioBindings.scg")))
		{
			return false;
		}

		/// [EN] Fields missing from an older file keep their current values.
		/// [JP] 古いファイルに無い項目は、今の値のまま残る。
		archive.TryField("acfData", acfData_);
		archive.TryField("masterVolume", masterVolume_);
		archive.TryField("defaultMasterVolume", defaultMasterVolume_);
		archive.TryField("categoryVolumes", categoryVolumes_);

		return true;
	}

	/**
	* [EN]
	* Writes the ACF and the volumes to AudioBindings.scg. Returns false
	* if the file cannot be written.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ACF と音量を AudioBindings.scg へ書き出す。書き込めなければ
	* false を返す。
	*/
	Bool CriManager::Save()
	{
		/// [EN] Creates the Config folder first if it does not exist yet.
		/// [JP] Config フォルダがまだ無ければ、先に作る。
		std::filesystem::path path("../UserProject/Assets/Config/AudioBindings.scg");
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		BinaryOutputArchive archive;
		archive.Field("acfData", acfData_);
		archive.Field("masterVolume", masterVolume_);
		archive.Field("defaultMasterVolume", defaultMasterVolume_);
		archive.Field("categoryVolumes", categoryVolumes_);

		return archive.Write(String(path.string()));
	}

	/**
	* [EN]
	* Returns the current master volume.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のマスター音量を返す。
	*/
	Float CriManager::MasterVolume()const
	{
		return masterVolume_;
	}

	/**
	* [EN]
	* Returns the master volume that ResetVolume returns to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ResetVolume で戻す先のマスター音量を返す。
	*/
	Float CriManager::DefaultMasterVolume()const
	{
		return defaultMasterVolume_;
	}

	/**
	* [EN]
	* Returns the volume of a category: the value set by the user, else
	* the ACF's value, else 1.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カテゴリの音量を返す。ユーザーが設定した値、無ければ ACF の値、
	* それも無ければ 1。
	*/
	Float CriManager::CategoryVolume(const String& category)const
	{
		if (categoryVolumes_.contains(category))
		{
			return categoryVolumes_.at(category);
		}

		if (defaultCategoryVolumes_.contains(category))
		{
			return defaultCategoryVolumes_.at(category);
		}

		return 1.0f;
	}

	/**
	* [EN]
	* Returns the names of the categories defined in the registered ACF.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録した ACF に定義されたカテゴリ名の一覧を返す。
	*/
	const DynamicArray<String>& CriManager::CategoryNameList()const
	{
		return categoryNames_;
	}

	/**
	* [EN]
	* Releases the voice pools, the bandwidth manager and the ACF, and
	* shuts CRI down. Does nothing if CRI was never initialized.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボイスプール、帯域の管理、ACF を解放し、CRI を終了する。CRI が
	* 初期化されていなければ何もしない。
	*/
	void CriManager::Finalize()
	{
		if (!criAtomEx_IsInitialized())
		{
			return;
		}

		/// [EN] Everything CRI allocated for this manager is released before CRI itself shuts down.
		/// [JP] この管理機構のために CRI が確保したものは、CRI 本体を終了する前に全て解放する。
		criAtomExVoicePool_FreeAll();
		waveVoicePool_ = nullptr;
		standardVoicePool_ = nullptr;

		if (dbasID_ != CRIATOMEXDBAS_ILLEGAL_ID)
		{
			criAtomExDbas_Destroy(dbasID_);
			dbasID_ = CRIATOMEXDBAS_ILLEGAL_ID;
		}

		if (!acfData_.empty())
		{
			criAtomEx_UnregisterAcf();
			acfData_.clear();
		}

		criAtomEx_Finalize_WASAPI();

		/// [EN] Removes the I/O hook so nothing calls into AudioByteStream after shutdown.
		/// [JP] 終了後に AudioByteStream が呼ばれないよう、I/O のフックを外す。
		criFs_SetSelectIoCallback(nullptr);
	}
}
