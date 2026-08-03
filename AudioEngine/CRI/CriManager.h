#pragma once
#include <FoundationEngine/Prelude.h>

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
	};

	inline void ScCriErrorCallback(const CriChar8* id, CriUint32 p1, CriUint32 p2, CriUint32* parray)
	{
		Char buffer[256];
		wsprintfA(buffer, "CRIWARE Error: %s\n", id);
		OutputDebugStringA(buffer);
	}
}