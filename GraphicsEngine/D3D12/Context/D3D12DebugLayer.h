#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12DebugLayer :public NonTransferable
	{
	public:
		static void Enable();

		static void Report();
	};
}