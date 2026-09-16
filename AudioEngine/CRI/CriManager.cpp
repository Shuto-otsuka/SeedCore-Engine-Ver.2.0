#include <AudioEngine/CRI/CriManager.h>
#include <AudioEngine/CRI/CriAllocator.h>
#include <AudioEngine/Audio/AudioByteStream.h>
#include <FoundationEngine/Log/Notice.h>

namespace SeedCore
{
	Bool CriManager::Initialize()
	{
		criErr_SetCallback(ScCriErrorCallback);
		criAtomEx_SetUserAllocator(ScCriMalloc, ScCriFree, nullptr);
		criFs_SetUserAllocator(ScCriMalloc, ScCriFree, nullptr);
		criFs_SetSelectIoCallback(&AudioByteStream::SelectIo);

		CriFsConfig fileSystemConfig{};
		criFs_SetDefaultConfig(&fileSystemConfig);
		fileSystemConfig.num_binders = 256;
		fileSystemConfig.max_binds = 256;
		fileSystemConfig.num_loaders = 64;
		fileSystemConfig.max_files = 64;
		fileSystemConfig.max_path = 512;

		CriAtomExConfig_WASAPI config{};
		criAtomEx_SetDefaultConfig_WASAPI(&config);
		config.atom_ex.fs_config = &fileSystemConfig;
		config.atom_ex.thread_model = CRIATOMEX_THREAD_MODEL_USER_MULTI;
		config.atom_ex.max_virtual_voices = 512;
		config.atom_ex.max_sequences = 512;
		config.atom_ex.max_tracks = 1024;
		config.atom_ex.max_track_items = 1024;
		config.atom_ex.max_parameter_blocks = 16384;
		config.atom_ex.coordinate_system = CRIATOMEX_COORDINATE_SYSTEM_LEFT_HANDED;
		config.atom_ex.server_frequency = 60.0f;
		config.atom_ex.parameter_update_interval = 1;
		config.atom_ex.max_pitch = 2400.0f;

		config.asr.server_frequency = config.atom_ex.server_frequency;
		config.asr.output_sampling_rate = 48000;
		config.asr.output_channels = 6;
		config.asr.speaker_mapping = CRIATOM_SPEAKER_MAPPING_AUTO;

		criAtomEx_Initialize_WASAPI(&config, nullptr, 0);

		if (!criAtomEx_IsInitialized())
		{
			return false;
		}

		CriAtomExWaveVoicePoolConfig waveConfig{};
		criAtomExVoicePool_SetDefaultConfigForWaveVoicePool(&waveConfig);
		waveConfig.num_voices = 128;
		waveConfig.player_config.max_channels = 6;
		waveConfig.player_config.max_sampling_rate = 48000;
		waveConfig.player_config.streaming_flag = CRI_TRUE;

		waveVoicePool_ = criAtomExVoicePool_AllocateWaveVoicePool(&waveConfig, nullptr, 0);
		if (!waveVoicePool_)
		{
			SC_LOG_ERROR("CriManager: Waveボイスプールの確保に失敗しました");
			return false;
		}

		CriAtomExStandardVoicePoolConfig standardConfig{};
		criAtomExVoicePool_SetDefaultConfigForStandardVoicePool(&standardConfig);
		standardConfig.num_voices = 128;
		standardConfig.player_config.max_channels = 6;
		standardConfig.player_config.max_sampling_rate = 48000;
		standardConfig.player_config.streaming_flag = CRI_TRUE;

		standardVoicePool_ = criAtomExVoicePool_AllocateStandardVoicePool(&standardConfig, nullptr, 0);
		if (!standardVoicePool_)
		{
			SC_LOG_ERROR("CriManager: Standardボイスプールの確保に失敗しました");
			return false;
		}

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

	void CriManager::Execute()
	{
		criAtomEx_ExecuteMain();
		criAtomEx_ExecuteAudioProcess();
	}

	void CriManager::Finalize()
	{
		if (!criAtomEx_IsInitialized())
		{
			return;
		}

		criAtomExVoicePool_FreeAll();
		waveVoicePool_ = nullptr;
		standardVoicePool_ = nullptr;

		if (dbasID_ != CRIATOMEXDBAS_ILLEGAL_ID)
		{
			criAtomExDbas_Destroy(dbasID_);
			dbasID_ = CRIATOMEXDBAS_ILLEGAL_ID;
		}

		criAtomEx_Finalize_WASAPI();

		criFs_SetSelectIoCallback(nullptr);
	}
}