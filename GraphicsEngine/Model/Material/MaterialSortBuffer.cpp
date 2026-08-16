#include <GraphicsEngine/Model/Material/MaterialSortBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void MaterialSortBuffer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		HRESULT hr{ S_OK };

		bindlessHeap_ = bindlessHeap;
		width_ = width;
		height_ = height;

		clearHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, false);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

		/// [EN] Bucket: RWStructuredBuffer<uint>[bucketCount_].
		/// [JP] Bucket: RWStructuredBuffer<uint>[bucketCount_]。
		{
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = static_cast<Uint64>(bucketCount_) * sizeof(Uint32);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&bucketBuffer_));
			SC_HR_CHECK(hr, "マテリアルソート Bucket バッファの生成に失敗しました");

			bucketUAVIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = bucketCount_;
			unorderedAccessViewDesc.Buffer.StructureByteStride = 0;
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			device->CreateUnorderedAccessView(bucketBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(bucketUAVIndex_));

			clearBucketIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(bucketBuffer_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearBucketIndex_));
		}

		/// [EN] Sorted Pixel List: RWStructuredBuffer<uint>[width * height].
		/// [JP] Sorted Pixel List: RWStructuredBuffer<uint>[width * height]。
		{
			Uint64 pixelCount = static_cast<Uint64>(width) * height;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = pixelCount * sizeof(Uint32);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&sortedPixelListBuffer_));
			SC_HR_CHECK(hr, "マテリアルソート SortedPixelList バッファの生成に失敗しました");

			sortedPixelListUAVIndex_ = bindlessHeap->AllocateIndex();

			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = static_cast<Uint>(pixelCount);
			unorderedAccessViewDesc.Buffer.StructureByteStride = 0;
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			device->CreateUnorderedAccessView(sortedPixelListBuffer_.Get(), nullptr, &unorderedAccessViewDesc, bindlessHeap->CPUHandle(sortedPixelListUAVIndex_));

			clearSortedPixelListIndex_ = clearHeap_.AllocateIndex();
			device->CreateUnorderedAccessView(sortedPixelListBuffer_.Get(), nullptr, &unorderedAccessViewDesc, clearHeap_.CPUHandle(clearSortedPixelListIndex_));
		}

		indicesSystem.SetMaterialSortBucketIndex(bucketUAVIndex_);
		indicesSystem.SetMaterialSortedPixelListIndex(sortedPixelListUAVIndex_);
	}

	void MaterialSortBuffer::Destroy(BindlessHeap* bindlessHeap)
	{
		bindlessHeap->FreeIndex(bucketUAVIndex_);
		bindlessHeap->FreeIndex(sortedPixelListUAVIndex_);

		/// [EN] Resize() destroys and immediately recreates these while previous
		///      frames may still be in flight reading/writing them, so the old
		///      buffers have to outlive this call - same reasoning as OITBuffer::Destroy.
		/// [JP] Resize() はこれらを破棄して即座に作り直すが、前フレームがまだ
		///      インフライトで読み書きしている可能性があるため、古いバッファは
		///      この呼び出しより長く生存させる必要がある - OITBuffer::Destroy と同じ理由。
		bindlessHeap->DeferRelease(bucketBuffer_);
		bindlessHeap->DeferRelease(sortedPixelListBuffer_);

		bucketBuffer_.Reset();
		sortedPixelListBuffer_.Reset();
	}

	void MaterialSortBuffer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		Destroy(bindlessHeap);
		Create(device, bindlessHeap, indicesSystem, width, height);
	}

	void MaterialSortBuffer::Barrier(ID3D12GraphicsCommandList* cmdList)const
	{
		D3D12_RESOURCE_BARRIER barriers[2]{};

		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[0].UAV.pResource = bucketBuffer_.Get();

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barriers[1].UAV.pResource = sortedPixelListBuffer_.Get();

		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	void MaterialSortBuffer::Clear(ID3D12GraphicsCommandList* cmdList)
	{
		const UINT zeroClearValues[4] = { 0, 0, 0, 0 };
		cmdList->ClearUnorderedAccessViewUint(
			bindlessHeap_->GPUHandle(bucketUAVIndex_),
			clearHeap_.CPUHandle(clearBucketIndex_),
			bucketBuffer_.Get(),
			zeroClearValues,
			0, nullptr
		);

		const UINT invalidPixelClearValues[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		cmdList->ClearUnorderedAccessViewUint(
			bindlessHeap_->GPUHandle(sortedPixelListUAVIndex_),
			clearHeap_.CPUHandle(clearSortedPixelListIndex_),
			sortedPixelListBuffer_.Get(),
			invalidPixelClearValues,
			0, nullptr
		);
	}
}
