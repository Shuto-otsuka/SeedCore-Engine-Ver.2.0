#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	template<typename T>
	class Buffer
	{
	public:
		auto View()const
		{
			return static_cast<const T*>(this)->ViewImplementation();
		}

		ID3D12Resource* Resource()const
		{
			return resource_.Get();
		}

		D3D12_GPU_VIRTUAL_ADDRESS Address()const
		{
			return resource_->GetGPUVirtualAddress();
		}

	protected:
		Buffer(ID3D12Device* device, Uint64 size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
		{
			HRESULT hr{ S_OK };

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = type;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;
		
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = size;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = flags;

			/// [EN] Buffers are always effectively created in COMMON regardless of
			///      the InitialState argument (the runtime ignores anything else
			///      for buffers, with a debug-layer warning); state promotion
			///      handles the rest.
			/// [JP] バッファは InitialState に何を渡しても実質 COMMON で作られる
			///      （ランタイムがそれ以外を無視し、デバッグレイヤー警告が出る）。
			///      以降はコモンステートプロモーションに任せる。
			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource_));
			SC_HR_CHECK(hr, "バッファの生成に失敗しました");
#ifdef _DEBUG
			resource_->SetName(L"Buffer");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(resource_.Get());
#endif
		}


	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	};
}