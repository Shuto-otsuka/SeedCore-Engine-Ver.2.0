#include <GraphicsEngine/D3D12/Buffer/HudlessBuffer.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void HudlessBuffer::Create(ID3D12Device* device, Uint32 width, Uint32 height)
	{
		width_ = width;
		height_ = height;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc.Count = 1;

		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&colorResource_));
		SC_HR_CHECK(hr, "Hudlessバッファの生成に失敗しました");
#ifdef _DEBUG
		colorResource_->SetName(L"HudlessBuffer");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(colorResource_.Get());
#endif
		state_ = D3D12_RESOURCE_STATE_COMMON;
	}

	void HudlessBuffer::Resize(ID3D12Device* device, Uint32 width, Uint32 height)
	{
		colorResource_.Reset();
		Create(device, width, height);
	}

	void HudlessBuffer::Capture(D3D12CommandList* cmdList, ID3D12Resource* source)
	{
		cmdList->Barrier(colorResource_.Get(), state_, D3D12_RESOURCE_STATE_COPY_DEST);
		state_ = D3D12_RESOURCE_STATE_COPY_DEST;

		cmdList->Get()->CopyResource(colorResource_.Get(), source);

		cmdList->Barrier(colorResource_.Get(), state_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		state_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}

	ID3D12Resource* HudlessBuffer::ColorResource()const
	{
		return colorResource_.Get();
	}
}
