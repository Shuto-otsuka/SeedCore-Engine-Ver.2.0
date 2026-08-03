#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class RasterizerStateType
	{
		SolidBackLHS,
		SolidBackRHS,
		SolidFrontLHS,
		SolidFrontRHS,
		SolidNoneLHS,
		SolidNoneRHS,
		WireBackLHS,
		WireBackRHS,
		WireFrontLHS,
		WireFrontRHS,
		WireNoneLHS,
		WireNoneRHS,
	};

	class RasterizerState
	{
	public:
		static void Create();

		static D3D12_RASTERIZER_DESC Get(RasterizerStateType type);
	};
}
