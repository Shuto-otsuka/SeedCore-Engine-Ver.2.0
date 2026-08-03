#include <GraphicsEngine/Font/FontManager.h>

namespace SeedCore
{
	Bool FontManager::Initialize()
	{
		if (FT_Init_FreeType(&ftLibrary_) != FT_Err_Ok)
		{
			return false;
		}

		return true;
	}

	void FontManager::Finalize()
	{
		if (ftLibrary_)
		{
			FT_Done_FreeType(ftLibrary_);
			ftLibrary_ = nullptr;
		}
	}

	FT_Library FontManager::GetLibrary()const
	{
		return ftLibrary_;
	}
}
