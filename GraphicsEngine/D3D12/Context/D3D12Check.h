#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	enum class D3D12Level : Uint32
	{
		D12_0 = 0,
		D12_1 = 1,
		D12_2 = 2,
	};

	class SEEDCORE_API D3D12Check
	{
	public:
		static void SetLevel(D3D12Level level);

		[[nodiscard]] static D3D12Level GetLevel();

	private:
		static D3D12Level level_;
	};
}
