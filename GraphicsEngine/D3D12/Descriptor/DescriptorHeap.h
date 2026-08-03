#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API DescriptorHeap
	{
	public:
		DescriptorHeap() = default;
		~DescriptorHeap() = default;

		Bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint count, Bool shaderVisible = false);

		ID3D12DescriptorHeap* Get()const;

		Uint AllocateIndex();

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(Uint index)const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(Uint index)const;

		Size IncrementSize()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap_;

		Uint currentIndex_ = 0;

		Size incrementSize_ = 0;
	};
}