#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	namespace ScResolution
	{
		struct ResSize
		{
			Float Width;
			Float Height;
		};

		inline const ResSize SC_HHD{ 640,360 };
		inline const ResSize SC_HD{ 1280,720 };
		inline const ResSize SC_FHD{ 1920,1080 };
		inline const ResSize SC_QHD{ 2560,1440 };
		inline const ResSize SC_4KUHD{ 3840,2160 };
		inline const ResSize SC_8KUHD{ 7680,4320 };
	}

	enum class ResolutionPreset : Int32
	{
		HHD = 0,
		HD = 1,
		FHD = 2,
		QHD = 3,
		UHD4K = 4,
		UHD8K = 5,
	};

	inline ScResolution::ResSize ToResSize(ResolutionPreset preset)
	{
		switch (preset)
		{
		case ResolutionPreset::HHD:
			return ScResolution::SC_HHD;
		case ResolutionPreset::HD:
			return ScResolution::SC_HD;
		case ResolutionPreset::FHD:
			return ScResolution::SC_FHD;
		case ResolutionPreset::QHD:
			return ScResolution::SC_QHD;
		case ResolutionPreset::UHD4K:
			return ScResolution::SC_4KUHD;
		case ResolutionPreset::UHD8K:
			return ScResolution::SC_8KUHD;
		}
		return ScResolution::SC_FHD;
	}
}