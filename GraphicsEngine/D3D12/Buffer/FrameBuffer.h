#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class DescriptorHeap;
	class BindlessHeap;
	class D3D12CommandList;

	class FrameBuffer
	{
	public:
		FrameBuffer(ID3D12Device* device, DescriptorHeap* renderTargetViewHeap, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT, DescriptorHeap* depthStencilViewHeap = nullptr, Float optimizedClearColorR = 0.3f, Float optimizedClearColorG = 0.3f, Float optimizedClearColorB = 0.3f, Float optimizedClearColorA = 1.0f);
		virtual ~FrameBuffer() = default;

		void Begin(D3D12CommandList* cmdList);

		void Clear(D3D12CommandList* cmdList, Color color);

		void Clear(D3D12CommandList* cmdList, Float colorR = 0.3f, Float colorG = 0.3f, Float colorB = 0.3f, Float colorA = 1.0f);

		void End(D3D12CommandList* cmdList);

		void Rebind(D3D12CommandList* cmdList);

		void Destroy(BindlessHeap* shaderResourceViewHeap);

		void Resize(ID3D12Device* device, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height);

		[[nodiscard]] D3D12_VIEWPORT GetViewport()const;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetViewHandle()const;

		[[nodiscard]] Uint32 ColorShaderResourceViewIndex()const;

		[[nodiscard]] Uint32 DepthShaderResourceViewIndex()const;

		[[nodiscard]] Bool HasDepthStencil()const;

		[[nodiscard]] ID3D12Resource* ColorResource()const;

		[[nodiscard]] ID3D12Resource* DepthResource()const;

	private:
		void CreateResources(ID3D12Device* device, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height);

	private:
		DXGI_FORMAT format_;

		Float optimizedClearColorR_;
		Float optimizedClearColorG_;
		Float optimizedClearColorB_;
		Float optimizedClearColorA_;

		D3D12_VIEWPORT viewport_;

		Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource_;

		Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

		D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;

		D3D12_RESOURCE_STATES depthCurrentState_ = D3D12_RESOURCE_STATE_COMMON;

		DescriptorHeap* renderTargetViewHeap_ = nullptr;

		DescriptorHeap* depthStencilViewHeap_ = nullptr;

		Uint32 renderTargetViewIndex_;

		Uint32 shaderResourceViewIndex_;

		Uint32 depthStencilViewIndex_;

		Uint32 depthShaderResourceViewIndex_;
	};
}
