#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void GeometryBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		HRESULT hr{ S_OK };

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, bufferCount_);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = static_cast<Uint64>(width);
			resourceDesc.Height = static_cast<Uint64>(height);
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = formats_[bufferIndex];
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			if (bufferIndex != 4)
			{
				/// [JP] RT4(VisibilityBuffer id)以外は UAV も付ける — 理由は
				///      GeometryBuffer.h の ColorUnorderedAccessViewIndex() 参照。
				resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			D3D12_CLEAR_VALUE clearValue{};
			clearValue.Format = formats_[bufferIndex];
			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 0.0f;
			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&colorResources_[bufferIndex]));
			SC_HR_CHECK(hr, "カラーバッファの生成に失敗しました");

			colorStates_[bufferIndex] = D3D12_RESOURCE_STATE_COMMON;

			Uint32 renderTargetViewIndex = renderTargetViewHeap_.AllocateIndex();
			D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
			renderTargetViewDesc.Format = formats_[bufferIndex];
			renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			device->CreateRenderTargetView(colorResources_[bufferIndex].Get(), &renderTargetViewDesc, renderTargetViewHeap_.CPUHandle(renderTargetViewIndex));

			colorShaderResourceViewIndices_[bufferIndex] = bindlessHeap->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = formats_[bufferIndex];
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(colorResources_[bufferIndex].Get(), &shaderResourceViewDesc, bindlessHeap->CPUHandle(colorShaderResourceViewIndices_[bufferIndex]));

			if (bufferIndex != 4)
			{
				colorUnorderedAccessViewIndices_[bufferIndex] = bindlessHeap->AllocateIndex();
				D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
				unorderedAccessViewDesc.Format = formats_[bufferIndex];
				unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				device->CreateUnorderedAccessView(colorResources_[bufferIndex].Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(colorUnorderedAccessViewIndices_[bufferIndex]));
			}
		}

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
		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_COMMON, &depthClearValue, IID_PPV_ARGS(&depthResource_));
		SC_HR_CHECK(hr, "深度バッファの生成に失敗しました");

		Uint32 depthStencilViewIndex = depthStencilViewHeap_.AllocateIndex();
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
		depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(depthResource_.Get(), &depthStencilViewDesc, depthStencilViewHeap_.CPUHandle(depthStencilViewIndex));

		depthShaderResourceViewIndex_ = bindlessHeap->AllocateIndex();
		D3D12_SHADER_RESOURCE_VIEW_DESC depthShaderResourceViewDesc{};
		depthShaderResourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		depthShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		depthShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthShaderResourceViewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(depthResource_.Get(), &depthShaderResourceViewDesc, bindlessHeap->CPUHandle(depthShaderResourceViewIndex_));

		viewport_.TopLeftX = 0.0f;
		viewport_.TopLeftY = 0.0f;
		viewport_.Width = static_cast<Float>(width);
		viewport_.Height = static_cast<Float>(height);
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;
	}

	void GeometryBuffer::Destroy(BindlessHeap* bindlessHeap)
	{
		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			bindlessHeap->FreeIndex(colorShaderResourceViewIndices_[bufferIndex]);
			if (bufferIndex != 4)
			{
				bindlessHeap->FreeIndex(colorUnorderedAccessViewIndices_[bufferIndex]);
			}
			colorResources_[bufferIndex].Reset();
		}

		bindlessHeap->FreeIndex(depthShaderResourceViewIndex_);
		depthResource_.Reset();
	}

	void GeometryBuffer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		Create(device, bindlessHeap, width, height);
	}

	void GeometryBuffer::Begin(D3D12CommandList* cmdList)
	{
		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			if (colorStates_[bufferIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET)
			{
				cmdList->Barrier(colorResources_[bufferIndex].Get(), colorStates_[bufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
				colorStates_[bufferIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
		}

		if (depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandles[bufferCount_];
		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			renderTargetViewHandles[bufferIndex] = renderTargetViewHeap_.CPUHandle(static_cast<Uint>(bufferIndex));
		}

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_.CPUHandle(0);
		cmdList->Get()->OMSetRenderTargets(bufferCount_, renderTargetViewHandles, FALSE, &depthStencilViewHandle);

		cmdList->Get()->RSSetViewports(1, &viewport_);

		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
		cmdList->Get()->RSSetScissorRects(1, &scissorRect);
	}

	void GeometryBuffer::BeginDepthOnly(D3D12CommandList* cmdList)
	{
		if (depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_.CPUHandle(0);
		cmdList->Get()->OMSetRenderTargets(0, nullptr, FALSE, &depthStencilViewHandle);

		cmdList->Get()->RSSetViewports(1, &viewport_);

		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
		cmdList->Get()->RSSetScissorRects(1, &scissorRect);
	}

	void GeometryBuffer::BeginVisibility(D3D12CommandList* cmdList)
	{
		if (colorStates_[4] != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			cmdList->Barrier(colorResources_[4].Get(), colorStates_[4], D3D12_RESOURCE_STATE_RENDER_TARGET);
			colorStates_[4] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = renderTargetViewHeap_.CPUHandle(4);
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_.CPUHandle(0);
		cmdList->Get()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);

		cmdList->Get()->RSSetViewports(1, &viewport_);

		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
		cmdList->Get()->RSSetScissorRects(1, &scissorRect);
	}

	void GeometryBuffer::BeginDepth(D3D12CommandList* cmdList)
	{
		if (depthState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
	}

	void GeometryBuffer::Clear(D3D12CommandList* cmdList)
	{
		const Float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			cmdList->Get()->ClearRenderTargetView(renderTargetViewHeap_.CPUHandle(static_cast<Uint>(bufferIndex)), clearColor, 0, nullptr);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_.CPUHandle(0);
		cmdList->Get()->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	}

	void GeometryBuffer::ClearDepth(D3D12CommandList* cmdList)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = depthStencilViewHeap_.CPUHandle(0);
		cmdList->Get()->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	}

	void GeometryBuffer::ClearVisibility(D3D12CommandList* cmdList)
	{
		const Float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		cmdList->Get()->ClearRenderTargetView(renderTargetViewHeap_.CPUHandle(4), clearColor, 0, nullptr);
	}

	void GeometryBuffer::EndColor(D3D12CommandList* cmdList)
	{
		for (Int bufferIndex = 0; bufferIndex < bufferCount_; ++bufferIndex)
		{
			if (colorStates_[bufferIndex] != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
			{
				cmdList->Barrier(colorResources_[bufferIndex].Get(), colorStates_[bufferIndex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
				colorStates_[bufferIndex] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			}
		}
	}

	void GeometryBuffer::EndDepthNonPixel(D3D12CommandList* cmdList)
	{
		if (depthState_ != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		{
			cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			depthState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
	}

	void GeometryBuffer::EndDepth(D3D12CommandList* cmdList)
	{
		if (depthState_ != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		{
			cmdList->Barrier(depthResource_.Get(), depthState_, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			depthState_ = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		}
	}

	void GeometryBuffer::End(D3D12CommandList* cmdList)
	{
		EndColor(cmdList);
		EndDepth(cmdList);
	}

	Uint32 GeometryBuffer::ColorShaderResourceViewIndex(Int index)const
	{
		return colorShaderResourceViewIndices_[index];
	}

	Uint32 GeometryBuffer::DepthShaderResourceViewIndex()const
	{
		return depthShaderResourceViewIndex_;
	}

	ID3D12Resource* GeometryBuffer::ColorResource(Int index)const
	{
		return colorResources_[index].Get();
	}

	ID3D12Resource* GeometryBuffer::DepthResource()const
	{
		return depthResource_.Get();
	}

	Uint32 GeometryBuffer::ColorUnorderedAccessViewIndex(Int index)const
	{
		return colorUnorderedAccessViewIndices_[index];
	}

	Uint32 GeometryBuffer::VelocityUnorderedAccessViewIndex()const
	{
		return colorUnorderedAccessViewIndices_[2];
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GeometryBuffer::DepthStencilViewHandle()const
	{
		return depthStencilViewHeap_.CPUHandle(0);
	}
}
