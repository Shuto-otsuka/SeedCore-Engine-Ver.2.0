#pragma once
#include <GraphicsEngine/D3D12/Buffer/Buffer.h>

namespace SeedCore
{
	class ArgumentBuffer :public Buffer<ArgumentBuffer>
	{
	public:
		ArgumentBuffer(ID3D12Device* device, UINT size);

		Bool ViewImplementation()const;
	};
}