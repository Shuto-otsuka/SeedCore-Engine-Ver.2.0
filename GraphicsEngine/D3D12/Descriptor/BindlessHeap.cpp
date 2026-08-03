#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Bool BindlessHeap::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint maxCount)
	{
		heap_ = MakePtr<DescriptorHeap>();
		if (!heap_->Create(device, type, maxCount, true))
		{
			return false;
		}

		maxCount_ = maxCount;
		freeLists_.reserve(maxCount);

		for (Int index = static_cast<Int>(maxCount_) - 1; index >= 0; --index)
		{
			freeLists_.push_back(static_cast<Uint>(index));
		}

		return true;
	}

	Uint BindlessHeap::AllocateIndex()
	{
		if (freeLists_.empty())
		{
			return 0xFFFFFFFF;
		}

		Uint index = freeLists_.back();
		freeLists_.pop_back();
		return index;
	}

	void BindlessHeap::FreeIndex(Uint index)
	{
		freeLists_.push_back(index);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE BindlessHeap::CPUHandle(Uint index)const
	{
		return heap_->CPUHandle(index);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE BindlessHeap::GPUHandle(Uint index)const
	{
		return heap_->GPUHandle(index);
	}

	ID3D12DescriptorHeap* BindlessHeap::Heap()const
	{
		return heap_->Get();
	}
}