#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class UpscaleMode : Int32
	{
		MaxPerformance = 0,
		Balanced = 1,
		MaxQuality = 2,
		UltraPerformance = 3,
		Dlaa = 4,
	};

	/// [EN] Standard NVIDIA DLSS per-dimension render/output ratios, shared by
	///      both DLSS Ray Reconstruction and TAAU. Used to derive the native
	///      render resolution from the target output resolution whichever of
	///      the two upscale paths is active.
	/// [JP] NVIDIA DLSS の標準的な次元ごとのレンダー/出力比率。DLSS Ray
	///      Reconstruction・TAAU 双方で共有する。有効なアップスケール経路が
	///      どちらであっても、目標出力解像度からネイティブレンダー解像度を
	///      導出するのに使う。
	inline Float UpscaleRenderScale(UpscaleMode mode)
	{
		switch (mode)
		{
		case UpscaleMode::MaxPerformance:
			return 1.0f / 2.0f;
		case UpscaleMode::Balanced:
			return 1.0f / 1.7f;
		case UpscaleMode::MaxQuality:
			return 1.0f / 1.5f;
		case UpscaleMode::UltraPerformance:
			return 1.0f / 3.0f;
		case UpscaleMode::Dlaa:
			return 1.0f;
		}
		return 1.0f;
	}
}
