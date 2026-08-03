#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class DepthStencilStateType
	{
		DepthOff,
		DepthOnWriteOn,
		DepthOnWriteOff,
		DepthOnWriteOnReverseZ,
		DepthOnWriteOffReverseZ,
		DepthOffWriteOnReverseZ,
		DepthOnWriteOnStencil,
		DepthOnWriteOnStencilReverseZ,
	};

	class DepthStencilState
	{
	public:
		static void Create();

		static D3D12_DEPTH_STENCIL_DESC Get(DepthStencilStateType type);
	};
}
