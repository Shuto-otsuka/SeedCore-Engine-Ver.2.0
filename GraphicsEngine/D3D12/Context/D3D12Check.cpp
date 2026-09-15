#include <GraphicsEngine/D3D12/Context/D3D12Check.h>

namespace SeedCore
{
	D3D12Level D3D12Check::level_ = D3D12Level::D12_0;

	void D3D12Check::SetLevel(D3D12Level level)
	{
		level_ = level;
	}

	D3D12Level D3D12Check::GetLevel()
	{
		return level_;
	}
}
