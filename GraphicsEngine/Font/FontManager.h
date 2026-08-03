#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API FontManager
	{
	public:
		FontManager() = default;
		~FontManager() = default;

		Bool Initialize();

		void Finalize();

		FT_Library GetLibrary()const;

	private:
		FT_Library ftLibrary_ = nullptr;
	};
}
