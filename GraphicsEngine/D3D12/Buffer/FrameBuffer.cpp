#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	FrameBuffer::FrameBuffer(ID3D12Device* device, DescriptorHeap* renderTargetViewHeap, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height, DXGI_FORMAT format, DescriptorHeap* depthStencilViewHeap, Float optimizedClearColorR, Float optimizedClearColorG, Float optimizedClearColorB, Float optimizedClearColorA) :format_(format), optimizedClearColorR_(optimizedClearColorR), optimizedClearColorG_(optimizedClearColorG), optimizedClearColorB_(optimizedClearColorB), optimizedClearColorA_(optimizedClearColorA), renderTargetViewHeap_(renderTargetViewHeap), depthStencilViewHeap_(depthStencilViewHeap)
	{
		CreateResources(device, shaderResourceViewHeap, width, height);
	}

	void FrameBuffer::CreateResources(ID3D12Device* device, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = static_cast<Uint64>(width);
		resourceDesc.Height = static_cast<Uint64>(height);
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = format_;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format_;
		clearValue.Color[0] = optimizedClearColorR_;
		clearValue.Color[1] = optimizedClearColorG_;
		clearValue.Color[2] = optimizedClearColorB_;
		clearValue.Color[3] = optimizedClearColorA_;
		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&bufferResource_));
		SC_HR_CHECK(hr, "フレームバッファの生成に失敗しました");
#ifdef _DEBUG
		bufferResource_->SetName(L"FrameBuffer");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(bufferResource_.Get());
#endif
		currentState_ = D3D12_RESOURCE_STATE_COMMON;

		renderTargetViewIndex_ = renderTargetViewHeap_->AllocateIndex();

		D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
		renderTargetViewDesc.Format = format_;
		renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(bufferResource_.Get(), &renderTargetViewDesc, renderTargetViewHeap_->CPUHandle(renderTargetViewIndex_));

		shaderResourceViewIndex_ = shaderResourceViewHeap->AllocateIndex();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = format_;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(bufferResource_.Get(), &shaderResourceViewDesc, shaderResourceViewHeap->CPUHandle(shaderResourceViewIndex_));

		if (depthStencilViewHeap_)
		{
			D3D12_RESOURCE_DESC depthDesc{};
			depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			depthDesc.Width = static_cast<Uint64>(width);
			depthDesc.Height = static_cast<Uint64>(height);
			depthDesc.DepthOrArraySize = 1;
			depthDesc.MipLevels = 1;
			depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			depthDesc.SampleDesc.Count = 1;
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE depthClearValue{};
			depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			depthClearValue.DepthStencil.Depth = 0.0f;
			depthClearValue.DepthStencil.Stencil = 0;
			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_COMMON, &depthClearValue, IID_PPV_ARGS(&depthStencilResource_));
			SC_HR_CHECK(hr, "深度ステンシルバッファの生成に失敗しました");
#ifdef _DEBUG
			depthStencilResource_->SetName(L"FrameBuffer_Depth");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(depthStencilResource_.Get());
#endif
			depthCurrentState_ = D3D12_RESOURCE_STATE_COMMON;

			depthStencilViewIndex_ = depthStencilViewHeap_->AllocateIndex();

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
			depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			device->CreateDepthStencilView(depthStencilResource_.Get(), &depthStencilViewDesc, depthStencilViewHeap_->CPUHandle(depthStencilViewIndex_));

			depthShaderResourceViewIndex_ = shaderResourceViewHeap->AllocateIndex();

			D3D12_SHADER_RESOURCE_VIEW_DESC depthShaderResourceViewDesc{};
			depthShaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
			depthShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			depthShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			depthShaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(depthStencilResource_.Get(), &depthShaderResourceViewDesc, shaderResourceViewHeap->CPUHandle(depthShaderResourceViewIndex_));
		}

		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = static_cast<Float>(width);
		viewport_.Height = static_cast<Float>(height);
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;
	}

	void FrameBuffer::Destroy(BindlessHeap* shaderResourceViewHeap)
	{
		shaderResourceViewHeap->FreeIndex(shaderResourceViewIndex_);
		shaderResourceViewHeap->DeferRelease(bufferResource_);
		bufferResource_.Reset();

		if (depthStencilViewHeap_)
		{
			shaderResourceViewHeap->FreeIndex(depthShaderResourceViewIndex_);
			shaderResourceViewHeap->DeferRelease(depthStencilResource_);
			depthStencilResource_.Reset();
		}
	}

	void FrameBuffer::Resize(ID3D12Device* device, BindlessHeap* shaderResourceViewHeap, Uint32 width, Uint32 height)
	{
		Destroy(shaderResourceViewHeap);
		CreateResources(device, shaderResourceViewHeap, width, height);
	}

	void FrameBuffer::Begin(D3D12CommandList* cmdList)
	{
		cmdList->Barrier(bufferResource_.Get(), currentState_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = renderTargetViewHeap_->CPUHandle(renderTargetViewIndex_);

		if (depthStencilViewHeap_)
		{
			cmdList->Barrier(depthStencilResource_.Get(), depthCurrentState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_->CPUHandle(depthStencilViewIndex_);
			cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);
		}
		else
		{
			cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);
		}

		cmdList->Get()->RSSetViewports(1, &viewport_);

		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
		cmdList->Get()->RSSetScissorRects(1, &scissorRect);
	}

	void FrameBuffer::Clear(D3D12CommandList* cmdList, Color color)
	{
		Clear(cmdList, color.r, color.g, color.b, color.a);
	}

	void FrameBuffer::Clear(D3D12CommandList* cmdList, Float colorR, Float colorG, Float colorB, Float colorA)
	{
		const Float color[4] = { colorR, colorG, colorB, colorA };
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = renderTargetViewHeap_->CPUHandle(renderTargetViewIndex_);

		cmdList->Get()->ClearRenderTargetView(renderTargetViewHandle, color, 0, nullptr);

		if (depthStencilViewHeap_)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_->CPUHandle(depthStencilViewIndex_);
			cmdList->Get()->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
		}
	}

	void FrameBuffer::Rebind(D3D12CommandList* cmdList)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = renderTargetViewHeap_->CPUHandle(renderTargetViewIndex_);

		if (depthStencilViewHeap_)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_->CPUHandle(depthStencilViewIndex_);
			cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);
		}
		else
		{
			cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);
		}

		cmdList->Get()->RSSetViewports(1, &viewport_);

		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
		cmdList->Get()->RSSetScissorRects(1, &scissorRect);
	}

	void FrameBuffer::End(D3D12CommandList* cmdList)
	{
		cmdList->Barrier(bufferResource_.Get(), currentState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		currentState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

		if (depthStencilViewHeap_)
		{
			cmdList->Barrier(depthStencilResource_.Get(), depthCurrentState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			depthCurrentState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}
	}

	D3D12_VIEWPORT FrameBuffer::GetViewport()const
	{
		return viewport_;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE FrameBuffer::RenderTargetViewHandle()const
	{
		return renderTargetViewHeap_->CPUHandle(renderTargetViewIndex_);
	}

	Uint32 FrameBuffer::ColorShaderResourceViewIndex()const
	{
		return shaderResourceViewIndex_;
	}

	Uint32 FrameBuffer::DepthShaderResourceViewIndex()const
	{
		return depthShaderResourceViewIndex_;
	}

	Bool FrameBuffer::HasDepthStencil()const
	{
		return depthStencilViewHeap_ != nullptr;
	}

	ID3D12Resource* FrameBuffer::ColorResource()const
	{
		return bufferResource_.Get();
	}

	ID3D12Resource* FrameBuffer::DepthResource()const
	{
		return depthStencilResource_.Get();
	}
}
