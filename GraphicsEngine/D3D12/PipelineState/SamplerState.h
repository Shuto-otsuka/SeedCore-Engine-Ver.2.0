#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class SamplerStateType
	{
		POINT_WRAP = 0,
		POINT_CLAMP,
		POINT_MIRROR,
		LINEAR_WRAP,
		LINEAR_CLAMP,
		LINEAR_MIRROR,
		ANISOTROPIC_WRAP,
		ANISOTROPIC_CLAMP,
		ANISOTROPIC_MIRROR,
		BORDER_BLACK,
		BORDER_WHITE,

		MAX_COUNT,
	};

	class SamplerState
	{
	public:
		static DynamicArray<D3D12_STATIC_SAMPLER_DESC> Create();
	};
}