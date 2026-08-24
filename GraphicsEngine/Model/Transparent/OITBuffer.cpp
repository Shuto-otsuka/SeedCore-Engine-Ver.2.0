#include <GraphicsEngine/Model/Transparent/OITBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void OITBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		HRESULT hr{ S_OK };

		bindlessHeap_ = bindlessHeap;
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, false);

		/// [EN] Head Pointer Texture: R32_UINT, one uint per pixel.
		/// [JP] ヘッドポインタテクスチャ: R32_UINT、ピクセルごとに uint 1 つ。
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = width;
			resourceDesc.Height = height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_R32_UINT;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&headPointerTexture_));
			SC_HR_CHECK(hr, "OIT HeadPointerTextureの生成に失敗しました");
#ifdef _DEBUG
			headPointerTexture_->SetName(L"OIT_HeadPointerTexture");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(headPointerTexture_.Get());
#endif

			headPointerUAVIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_UINT;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			unorderedAccessViewDesc.Texture2D.MipSlice = 0;
			unorderedAccessViewDesc.Texture2D.PlaneSlice = 0;
			device->CreateUnorderedAccessView(headPointerTexture_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(headPointerUAVIndex_));

			clearHeadPointerIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(headPointerTexture_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearHeadPointerIndex_));
		}

		/// [EN] Fragment Buffer: RWStructuredBuffer, width * height * poolLayers
		///      elements, capped at poolByteBudget_.
		/// [JP] フラグメントバッファ: RWStructuredBuffer、width * height *
		///      poolLayers 要素（poolByteBudget_ で上限クランプ）。
		{
			static constexpr Uint fragmentStride = 16;
			Uint64 fragmentCount = static_cast<Uint64>(width) * height * poolLayers_;
			Uint64 budgetFragmentCount = poolByteBudget_ / fragmentStride;
			if (fragmentCount > budgetFragmentCount)
			{
				fragmentCount = budgetFragmentCount;
			}
			Uint64 bufferSize = fragmentCount * fragmentStride;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = bufferSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&fragmentBuffer_));
			SC_HR_CHECK(hr, "OIT FragmentBufferの生成に失敗しました");
#ifdef _DEBUG
			fragmentBuffer_->SetName(L"OIT_FragmentBuffer");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(fragmentBuffer_.Get());
#endif

			fragmentBufferUAVIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = static_cast<Uint>(fragmentCount);
			unorderedAccessViewDesc.Buffer.StructureByteStride = fragmentStride;
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			device->CreateUnorderedAccessView(fragmentBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(fragmentBufferUAVIndex_));

			fragmentCapacity_ = static_cast<Uint>(fragmentCount);
		}

		/// [EN] Counter Buffer: RWByteAddressBuffer, 4 bytes.
		/// [JP] カウンターバッファ: RWByteAddressBuffer、4 バイト。
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = 4;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&counterBuffer_));
			SC_HR_CHECK(hr, "OIT CounterBufferの生成に失敗しました");
#ifdef _DEBUG
			counterBuffer_->SetName(L"OIT_CounterBuffer");
			GFSDK_Aftermath_DX12_UpdateResourceInfo(counterBuffer_.Get());
#endif

			counterUAVIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = 1;
			unorderedAccessViewDesc.Buffer.StructureByteStride = 0;
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			device->CreateUnorderedAccessView(counterBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(counterUAVIndex_));

			clearCounterIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(counterBuffer_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearCounterIndex_));
		}

		indicesSystem.SetOITHeadPointerIndex(headPointerUAVIndex_);
		indicesSystem.SetOITFragmentBufferIndex(fragmentBufferUAVIndex_);
		indicesSystem.SetOITCounterIndex(counterUAVIndex_);
		indicesSystem.SetOITFragmentCapacity(fragmentCapacity_);
	}

	void OITBuffer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(headPointerUAVIndex_);
		bindlessHeap->FreeIndex(fragmentBufferUAVIndex_);
		bindlessHeap->FreeIndex(counterUAVIndex_);

		/// [EN] Resize() destroys and immediately recreates these while the
		///      previous frames are still in flight writing their PPLL fragments,
		///      so the old buffers have to outlive this call.
		/// [JP] Resize() はこれらを破棄して即座に作り直すが、その時点で前フレームは
		///      まだインフライトで PPLL のフラグメントを書き込んでいるため、
		///      古いバッファはこの呼び出しより長く生存させる必要がある。
		bindlessHeap->DeferRelease(headPointerTexture_);
		bindlessHeap->DeferRelease(fragmentBuffer_);
		bindlessHeap->DeferRelease(counterBuffer_);

		headPointerTexture_.Reset();
		fragmentBuffer_.Reset();
		counterBuffer_.Reset();
	}

	void OITBuffer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		Create(device, bindlessHeap, indicesSystem, width, height);
	}

	void OITBuffer::Barrier(ID3D12GraphicsCommandList* cmdList)const
	{
		D3D12_RESOURCE_BARRIER barriers[3]{};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[0].UAV.pResource = headPointerTexture_.Get();

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[1].UAV.pResource = fragmentBuffer_.Get();

		barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[2].UAV.pResource = counterBuffer_.Get();

		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	void OITBuffer::Clear(ID3D12GraphicsCommandList* cmdList)
	{
		const UINT headPointerClearValues[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		cmdList->ClearUnorderedAccessViewUint(
			bindlessHeap_->GPUHandle(headPointerUAVIndex_),
			clearHeap_.CPUHandle(clearHeadPointerIndex_),
			headPointerTexture_.Get(),
			headPointerClearValues,
			0, nullptr
		);

		const UINT counterClearValues[4] = { 0, 0, 0, 0 };
		cmdList->ClearUnorderedAccessViewUint(
			bindlessHeap_->GPUHandle(counterUAVIndex_),
			clearHeap_.CPUHandle(clearCounterIndex_),
			counterBuffer_.Get(),
			counterClearValues,
			0, nullptr
		);
	}
}
