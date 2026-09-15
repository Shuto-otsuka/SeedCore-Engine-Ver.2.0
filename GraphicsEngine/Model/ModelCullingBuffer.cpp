#include <GraphicsEngine/Model/ModelCullingBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	ModelCullingBuffer::~ModelCullingBuffer()
	{
		if (!bindlessHeap_)
		{
			return;
		}

		bindlessHeap_->FreeIndex(argumentUnorderedAccessViewIndex_);
		bindlessHeap_->DeferRelease(argumentBuffer_);
		bindlessHeap_->DeferRelease(argumentResetBuffer_);

		if (singleSidedBuffer_)
		{
			bindlessHeap_->FreeIndex(singleSidedUnorderedAccessViewIndex_);
			bindlessHeap_->FreeIndex(singleSidedShaderResourceViewIndex_);
			bindlessHeap_->FreeIndex(doubleSidedUnorderedAccessViewIndex_);
			bindlessHeap_->FreeIndex(doubleSidedShaderResourceViewIndex_);
			bindlessHeap_->DeferRelease(singleSidedBuffer_);
			bindlessHeap_->DeferRelease(doubleSidedBuffer_);
		}
	}

	void ModelCullingBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap)
	{
		HRESULT hr = S_OK;

		device_ = device;
		bindlessHeap_ = bindlessHeap;

		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = sizeof(D3D12_DRAW_ARGUMENTS) * 2;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&argumentBuffer_));
			SC_HR_CHECK(hr, "モデルカリングの引数バッファの生成に失敗しました");
#ifdef _DEBUG
			argumentBuffer_->SetName(L"ModelCullingBuffer_Argument");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(argumentBuffer_.Get());
#endif

			argumentUnorderedAccessViewIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = sizeof(D3D12_DRAW_ARGUMENTS) * 2 / 4;
			unorderedAccessViewDesc.Buffer.StructureByteStride = 0;
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			device->CreateUnorderedAccessView(argumentBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(argumentUnorderedAccessViewIndex_));
		}

		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = sizeof(D3D12_DRAW_ARGUMENTS) * 2;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&argumentResetBuffer_));
			SC_HR_CHECK(hr, "モデルカリングの引数リセットバッファの生成に失敗しました");
#ifdef _DEBUG
			argumentResetBuffer_->SetName(L"ModelCullingBuffer_ArgumentReset");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(argumentResetBuffer_.Get());
#endif

			D3D12_DRAW_ARGUMENTS resetArguments[2]{};
			resetArguments[0].VertexCountPerInstance = 124 * 3;
			resetArguments[1].VertexCountPerInstance = 124 * 3;

			void* mapped = nullptr;
			argumentResetBuffer_->Map(0, nullptr, &mapped);
			memcpy(mapped, resetArguments, sizeof(resetArguments));
			argumentResetBuffer_->Unmap(0, nullptr);
		}

		{
			D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
			argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{};
			commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
			commandSignatureDesc.NumArgumentDescs = 1;
			commandSignatureDesc.pArgumentDescs = &argumentDesc;

			hr = device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature_));
			SC_HR_CHECK(hr, "モデルカリングのコマンドシグネチャの生成に失敗しました");
		}

		constantBuffer_ = MakePtr<ConstantBuffer<ModelCullingConstantBuffer>>(device, bindlessHeap);
	}

	void ModelCullingBuffer::Reserve(Uint capacity)
	{
		if (capacity <= capacity_)
		{
			return;
		}

		if (singleSidedBuffer_)
		{
			bindlessHeap_->FreeIndex(singleSidedUnorderedAccessViewIndex_);
			bindlessHeap_->FreeIndex(singleSidedShaderResourceViewIndex_);
			bindlessHeap_->FreeIndex(doubleSidedUnorderedAccessViewIndex_);
			bindlessHeap_->FreeIndex(doubleSidedShaderResourceViewIndex_);
			bindlessHeap_->DeferRelease(singleSidedBuffer_);
			bindlessHeap_->DeferRelease(doubleSidedBuffer_);
		}

		capacity_ = capacity;

		HRESULT hr = S_OK;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(ModelCullingStructuredBuffer) * static_cast<Uint64>(capacity_);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
		unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		unorderedAccessViewDesc.Buffer.FirstElement = 0;
		unorderedAccessViewDesc.Buffer.NumElements = capacity_;
		unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(ModelCullingStructuredBuffer);
		unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
		unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Buffer.FirstElement = 0;
		shaderResourceViewDesc.Buffer.NumElements = capacity_;
		shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(ModelCullingStructuredBuffer);
		shaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&singleSidedBuffer_));
		SC_HR_CHECK(hr, "モデルカリングの片面リストバッファの生成に失敗しました");
#ifdef _DEBUG
		singleSidedBuffer_->SetName(L"ModelCullingBuffer_SingleSided");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(singleSidedBuffer_.Get());
#endif

		singleSidedUnorderedAccessViewIndex_ = bindlessHeap_->AllocateIndex();
		device_->CreateUnorderedAccessView(singleSidedBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap_->CPUHandle(singleSidedUnorderedAccessViewIndex_));

		singleSidedShaderResourceViewIndex_ = bindlessHeap_->AllocateIndex();
		device_->CreateShaderResourceView(singleSidedBuffer_.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(singleSidedShaderResourceViewIndex_));

		hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&doubleSidedBuffer_));
		SC_HR_CHECK(hr, "モデルカリングの両面リストバッファの生成に失敗しました");
#ifdef _DEBUG
		doubleSidedBuffer_->SetName(L"ModelCullingBuffer_DoubleSided");
		GFSDK_Aftermath_DX12_UpdateResourceInfo(doubleSidedBuffer_.Get());
#endif

		doubleSidedUnorderedAccessViewIndex_ = bindlessHeap_->AllocateIndex();
		device_->CreateUnorderedAccessView(doubleSidedBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap_->CPUHandle(doubleSidedUnorderedAccessViewIndex_));

		doubleSidedShaderResourceViewIndex_ = bindlessHeap_->AllocateIndex();
		device_->CreateShaderResourceView(doubleSidedBuffer_.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(doubleSidedShaderResourceViewIndex_));
	}

	void ModelCullingBuffer::Begin(ID3D12GraphicsCommandList* cmdList)
	{
		ModelCullingConstantBuffer constants{};
		constants.singleSidedIndex_ = singleSidedUnorderedAccessViewIndex_;
		constants.doubleSidedIndex_ = doubleSidedUnorderedAccessViewIndex_;
		constants.argumentsIndex_ = argumentUnorderedAccessViewIndex_;
		constantBuffer_->Update(constants);

		D3D12_RESOURCE_BARRIER copyBarrier{};
		copyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		copyBarrier.Transition.pResource = argumentBuffer_.Get();
		copyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		copyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		copyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		cmdList->ResourceBarrier(1, &copyBarrier);

		cmdList->CopyBufferRegion(argumentBuffer_.Get(), 0, argumentResetBuffer_.Get(), 0, sizeof(D3D12_DRAW_ARGUMENTS) * 2);

		D3D12_RESOURCE_BARRIER barriers[3]{};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = argumentBuffer_.Get();
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource = singleSidedBuffer_.Get();
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[2].Transition.pResource = doubleSidedBuffer_.Get();
		barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	void ModelCullingBuffer::Barrier(ID3D12GraphicsCommandList* cmdList)const
	{
		D3D12_RESOURCE_BARRIER barriers[3]{};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = argumentBuffer_.Get();
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource = singleSidedBuffer_.Get();
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[2].Transition.pResource = doubleSidedBuffer_.Get();
		barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	void ModelCullingBuffer::End(ID3D12GraphicsCommandList* cmdList)const
	{
		D3D12_RESOURCE_BARRIER barriers[3]{};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = argumentBuffer_.Get();
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource = singleSidedBuffer_.Get();
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[2].Transition.pResource = doubleSidedBuffer_.Get();
		barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	Uint ModelCullingBuffer::GetConstantBufferIndex()const
	{
		return constantBuffer_->GetIndex();
	}

	Uint ModelCullingBuffer::GetSingleSidedShaderResourceViewIndex()const
	{
		return singleSidedShaderResourceViewIndex_;
	}

	Uint ModelCullingBuffer::GetDoubleSidedShaderResourceViewIndex()const
	{
		return doubleSidedShaderResourceViewIndex_;
	}

	ID3D12CommandSignature* ModelCullingBuffer::GetCommandSignature()const
	{
		return commandSignature_.Get();
	}

	ID3D12Resource* ModelCullingBuffer::GetArgumentBuffer()const
	{
		return argumentBuffer_.Get();
	}
}
