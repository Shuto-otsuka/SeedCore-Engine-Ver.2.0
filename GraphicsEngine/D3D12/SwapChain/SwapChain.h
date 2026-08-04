#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class SEEDCORE_API SwapChain
	{
	public:
		SwapChain(Float width, Float height);
		~SwapChain() = default;

		Bool Create(IDXGIFactory7* factory, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, HWND hwnd);

		void Present(ID3D12Device* device);

		Size BufferCount()const;

		D3D12_CPU_DESCRIPTOR_HANDLE Handle()const;

		ID3D12Resource* BackBuffer()const;

		ID3D12DescriptorHeap* GetDescHeap()const;

	private:
		Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

		ResourcePtr<DescriptorHeap> descHeap_;

		DynamicArray<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers_;

		Float width_{ 1920 };
		Float height_{ 1080 };
		Size bufferCount_{ 3 };
	};
}