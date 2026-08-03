#pragma once
#include <GraphicsEngine/D3D12/Buffer/Buffer.h>

namespace SeedCore
{
	class IndexBuffer :public Buffer<IndexBuffer>
	{
	public:
		IndexBuffer(ID3D12Device* device, Uint count, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT);

		~IndexBuffer() = default;

		D3D12_INDEX_BUFFER_VIEW ViewImplementation()const;

	private:
		DXGI_FORMAT format_;
		Uint size_;
	};
}