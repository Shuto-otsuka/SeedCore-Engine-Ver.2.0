#include <AudioEngine/CRI/CriManager.h>
#include <AudioEngine/CRI/CriAllocator.h>

namespace SeedCore
{
	Bool CriManager::Initialize()
	{
		criErr_SetCallback(ScCriErrorCallback);
		criAtomEx_SetUserAllocator(ScCriMalloc, ScCriFree, nullptr);

		CriAtomExConfig config{};
		criAtomEx_SetDefaultConfig(&config);
		config.thread_model = CRIATOMEX_THREAD_MODEL_USER_MULTI;

		if (!criAtomEx_Initialize(&config, nullptr, 0))
		{
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
		criAtomEx_Finalize();
	}
}