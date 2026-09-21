#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class AtomCraft
	{
	public:
		static String Detect();

		static Bool Open(const String& executablePath, const String& projectPath);
	};
}
