#include <FoundationEngine/Log/DxFail.h>
#include <GraphicsEngine/D3D12/SwapChain/SwapChain.h>

namespace SeedCore
{
	SwapChain::SwapChain(Float width, Float height) :width_(width), height_(height)
	{
		/// No Code
	}

	Bool SwapChain::Create(IDXGIFactory7* factory, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, HWND hwnd)
	{
		backBuffers_.reserve(bufferCount_);

		HRESULT hr{ S_OK };

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = static_cast<Uint>(width_);
		swapChainDesc.Height = static_cast<Uint>(height_);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = static_cast<Uint>(bufferCount_);
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		IDXGISwapChain1* swapChain = nullptr;
		hr = factory->CreateSwapChainForHwnd(cmdQueue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);
		SC_HR_CHECK(hr, "スワップチェーンの生成に失敗しました");

		hr = swapChain->QueryInterface(IID_IDXGISwapChain4, (void**)&swapChain_);
		SC_HR_CHECK(hr, "スワップチェーンのQueryInterfaceに失敗しました");

		swapChain->Release();

		descHeap_ = MakePtr<DescriptorHeap>();
		if (!descHeap_->Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, static_cast<Uint>(bufferCount_)))
		{
			return false;
		}

		for (Size index = 0;index < bufferCount_;++index)
		{
			Uint temporaryIndex = static_cast<Uint>(index);
			Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
			swapChain_->GetBuffer(temporaryIndex, IID_PPV_ARGS(&buffer));
			backBuffers_.push_back(buffer);

			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = descHeap_->CPUHandle(temporaryIndex);

			device->CreateRenderTargetView(backBuffers_[index].Get(), nullptr, renderTargetViewHandle);
		}

		return SUCCEEDED(hr);
	}

	void SwapChain::Present()
	{
		HRESULT hr{ S_OK };

		Uint syncInterval = 0;
		Uint presentFlags = DXGI_PRESENT_ALLOW_TEARING;
		hr = swapChain_->Present(syncInterval, presentFlags);
		SC_HR_CHECK(hr, "スワップチェーンのPresentに失敗しました");
	}

	Size SwapChain::BufferCount()const
	{
		return bufferCount_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::Handle()const
	{
		Uint currentIndex = swapChain_->GetCurrentBackBufferIndex();
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = descHeap_->CPUHandle(currentIndex);
		return renderTargetViewHandle;
	}

	ID3D12Resource* SwapChain::BackBuffer()const
	{
		Uint index = swapChain_->GetCurrentBackBufferIndex();
		return backBuffers_[index].Get();
	}

	ID3D12DescriptorHeap* SwapChain::GetDescHeap()const
	{
		return descHeap_->Get();
	}
}