#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	Bool DescriptorHeap::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint count, Bool shaderVisible)
	{
		HRESULT hr{ S_OK };

		currentIndex_ = 0;

		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
		descHeapDesc.Type = type;
		descHeapDesc.NumDescriptors = count;
		descHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&descHeap_));
		SC_HR_CHECK(hr, "ディスクリプタヒープの生成に失敗しました");

		incrementSize_ = device->GetDescriptorHandleIncrementSize(type);

		return SUCCEEDED(hr);
	}

	ID3D12DescriptorHeap* DescriptorHeap::Get()const
	{
		return descHeap_.Get();
	}

	Uint DescriptorHeap::AllocateIndex()
	{
		return currentIndex_++;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandle(Uint index)const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<Uint64>(index) * incrementSize_;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandle(Uint index)const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = descHeap_->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<Uint64>(index) * incrementSize_;
		return handle;
	}

	Size DescriptorHeap::IncrementSize()const
	{
		return incrementSize_;
	}
}