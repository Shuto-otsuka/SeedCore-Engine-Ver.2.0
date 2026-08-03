#include <GraphicsEngine/D3D12/Buffer/IndexBuffer.h>

namespace SeedCore
{
	IndexBuffer::IndexBuffer(ID3D12Device* device, Uint count, DXGI_FORMAT format) :Buffer(device, (format == DXGI_FORMAT_R32_UINT ? 4 : 2 )* count, D3D12_HEAP_TYPE_UPLOAD), format_(format), size_((format == DXGI_FORMAT_R32_UINT ? 4 : 2)* count)
	{
		/// No Code
	}

	D3D12_INDEX_BUFFER_VIEW IndexBuffer::ViewImplementation()const
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		indexBufferView.BufferLocation = this->resource_->GetGPUVirtualAddress();
		indexBufferView.Format = format_;
		indexBufferView.SizeInBytes = size_;
		return indexBufferView;
	}
}