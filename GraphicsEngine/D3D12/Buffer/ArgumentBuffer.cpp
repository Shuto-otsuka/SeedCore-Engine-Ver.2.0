#include <GraphicsEngine/D3D12/Buffer/ArgumentBuffer.h>

namespace SeedCore
{
	ArgumentBuffer::ArgumentBuffer(ID3D12Device* device, UINT size) : Buffer(device, size, D3D12_HEAP_TYPE_UPLOAD)
	{
		/// No Code
	}

	Bool ArgumentBuffer::ViewImplementation()const
	{
		return true;
	}
}