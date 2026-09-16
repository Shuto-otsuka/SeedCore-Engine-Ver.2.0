#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	class SEEDCORE_API CriManager
	{
	public:
		CriManager() = default;
		~CriManager() = default;

		Bool Initialize();

		void Execute();

		void Finalize();

	private:
		CriAtomExVoicePoolHn waveVoicePool_ = nullptr;

		CriAtomExVoicePoolHn standardVoicePool_ = nullptr;

		CriAtomDbasId dbasID_ = CRIATOMEXDBAS_ILLEGAL_ID;
	};

	inline void ScCriErrorCallback(const CriChar8* id, CriUint32 p1, CriUint32 p2, CriUint32* parray)
	{
		const CriChar8* message = criErr_ConvertIdToMessage(id, p1, p2);

		if (id && id[0] == 'W')
		{
			SC_LOG_WARNING("CRIWARE: {} ({})", message ? message : "", id);
		}
		else
		{
			SC_LOG_ERROR("CRIWARE: {} ({})", message ? message : "", id);
		}
	}
}