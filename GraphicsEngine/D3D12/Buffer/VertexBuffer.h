#pragma once
#include <GraphicsEngine/D3D12/Buffer/Buffer.h>

namespace SeedCore
{
	template<typename T>
	class VertexBuffer :public Buffer<VertexBuffer>
	{
	public:
		VertexBuffer(ID3D12Device* device, Uint count) :Buffer<VertexBuffer<T>>(device, sizeof(T)* count, D3D12_HEAP_TYPE_UPLOAD), stride_(sizeof(T)), size_(sizeof(T)* count)
		{
			/// No Code
		}

		~VertexBuffer() = default;

		D3D12_VERTEX_BUFFER_VIEW ViewImplementation()const
		{
			D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
			vertexBufferView.BufferLocation = this->resource_->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes = stride_;
			vertexBufferView.SizeInBytes = size_;
			return vertexBufferView;
		}

		void Update(const T* data, Uint count)
		{
			void* mapped = nullptr;
			this->resource_->Map(0, nullptr, &mapped);
			memcpy(mapped, data, sizeof(T) * count);
			this->resource_->Unmap(0, nullptr);
		}

	private:
		Uint stride_;
		Uint size_;
	};
}