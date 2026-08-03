#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class DlssMode : Int32
	{
		MaxPerformance = 0,
		Balanced = 1,
		MaxQuality = 2,
		UltraPerformance = 3,
		Dlaa = 4,
	};

	/// [EN] Standard NVIDIA DLSS per-dimension render/output ratios. Used to
	///      derive the native render resolution from the target output
	///      resolution when DLSS Ray Reconstruction is enabled.
	/// [JP] NVIDIA DLSS の標準的な次元ごとのレンダー/出力比率。DLSS Ray
	///      Reconstruction 有効時、目標出力解像度からネイティブレンダー解像度を
	///      導出するのに使う。
	inline Float DlssRenderScale(DlssMode mode)
	{
		switch (mode)
		{
		case DlssMode::MaxPerformance:
			return 1.0f / 2.0f;
		case DlssMode::Balanced:
			return 1.0f / 1.7f;
		case DlssMode::MaxQuality:
			return 1.0f / 1.5f;
		case DlssMode::UltraPerformance:
			return 1.0f / 3.0f;
		case DlssMode::Dlaa:
			return 1.0f;
		}
		return 1.0f;
	}
}
