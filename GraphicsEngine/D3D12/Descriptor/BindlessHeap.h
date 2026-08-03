#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class SEEDCORE_API BindlessHeap :public NonTransferable
	{
	public:
		BindlessHeap() = default;
		~BindlessHeap() = default;

		Bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint maxCount);

		[[nodiscard]] Uint AllocateIndex();

		void FreeIndex(Uint index);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(Uint index)const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(Uint index)const;

		[[nodiscard]] ID3D12DescriptorHeap* Heap()const;

	private:
		ResourcePtr<DescriptorHeap> heap_;

		DynamicArray<Uint> freeLists_;

		Uint maxCount_ = 0;
	};
}