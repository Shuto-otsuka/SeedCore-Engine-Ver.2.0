#pragma once
#include <GraphicsEngine/D3D12/Buffer/Buffer.h>
#include <GraphicsEngine/D3D12/FrameRing.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	/**
	* [EN]
	* Frame-ring structured buffer: one upload resource (and SRV) per frame in
	* flight so the CPU can write frame N while the GPU reads frame N-1.
	* Update writes the current frame's slot; Index() returns the current
	* frame's bindless SRV index and must be re-registered every frame (never
	* cache it across frames).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フレームリング StructuredBuffer: インフライトフレーム数ぶんのアップロード
	* リソース（と SRV）を持ち、GPU がフレーム N-1 を読んでいる間に CPU が
	* フレーム N を書けるようにする。Update は現在フレームのスロットに書く。
	* Index() は現在フレームの bindless SRV インデックスを返すため、毎フレーム
	* 再登録すること（フレームを跨いでキャッシュしない）。
	*/
	template<typename T>
	class ReadOnlyStructuredBuffer :public NonCopyable
	{
	public:
		ReadOnlyStructuredBuffer(ID3D12Device* device, BindlessHeap* heap, Uint elementCount) : heap_(heap), elementCount_(elementCount)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = sizeof(T) * static_cast<Uint64>(elementCount);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			for (Uint frame = 0; frame < FrameRing::frameCount; frame++)
			{
				HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resources_[frame]));
				SC_HR_CHECK(hr, "構造化バッファの生成に失敗しました");

				indices_[frame] = heap->AllocateIndex();
				D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
				shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
				shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDesc.Buffer.FirstElement = 0;
				shaderResourceViewDesc.Buffer.NumElements = elementCount_;
				shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(T);
				shaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				device->CreateShaderResourceView(resources_[frame].Get(), &shaderResourceViewDesc, heap->CPUHandle(indices_[frame]));

				resources_[frame]->Map(0, nullptr, &mappedPtrs_[frame]);
			}
		}

		~ReadOnlyStructuredBuffer()
		{
			for (Uint frame = 0; frame < FrameRing::frameCount; frame++)
			{
				if (resources_[frame])
				{
					resources_[frame]->Unmap(0, nullptr);
				}
				if (heap_)
				{
					heap_->FreeIndex(indices_[frame]);
				}
			}
		}

		void Update(const T* data, Uint count)
		{
			Uint frame = FrameRing::Index();
			memcpy(mappedPtrs_[frame], data, sizeof(T) * count);

			/// [EN] Only clear the range this slot's previous use wrote beyond the
			///      new count — zeroing the whole tail every frame moved tens of
			///      megabytes of write-combined memory per frame.
			/// [JP] このスロットが前回書いた範囲のうち新しい count を超える部分
			///      だけをクリアする — 毎フレーム残り全域をゼロ埋めすると、
			///      フレームごとに数十 MB の write-combined 書き込みになる。
			if (count < lastCounts_[frame])
			{
				memset(static_cast<Byte*>(mappedPtrs_[frame]) + sizeof(T) * count, 0, sizeof(T) * (lastCounts_[frame] - count));
			}
			lastCounts_[frame] = count;
		}

		[[nodiscard]] Uint Index()const
		{
			return indices_[FrameRing::Index()];
		}

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress()const
		{
			return resources_[FrameRing::Index()]->GetGPUVirtualAddress();
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resources_[FrameRing::frameCount];
		void* mappedPtrs_[FrameRing::frameCount] = {};
		Uint indices_[FrameRing::frameCount] = {};
		Uint lastCounts_[FrameRing::frameCount] = {};

		BindlessHeap* heap_;
		Uint elementCount_;
	};

	template<typename T>
	class ReadWriteStructuredBuffer :public Buffer<ReadWriteStructuredBuffer<T>>
	{
	public:
		ReadWriteStructuredBuffer(ID3D12Device* device, Uint elementCount) :Buffer<ReadWriteStructuredBuffer<T>>(device, sizeof(T)* elementCount, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), elementCount_(elementCount)
		{
			/// No Code
		}

		D3D12_UNORDERED_ACCESS_VIEW_DESC ViewImplementation()const
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc{};
			unorderedAccessViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			unorderedAccessViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			unorderedAccessViewDesc.Buffer.FirstElement = 0;
			unorderedAccessViewDesc.Buffer.NumElements = elementCount_;
			unorderedAccessViewDesc.Buffer.StructureByteStride = sizeof(T);
			unorderedAccessViewDesc.Buffer.CounterOffsetInBytes = 0;
			unorderedAccessViewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			return unorderedAccessViewDesc;
		}

	private:
		Uint elementCount_;
	};
}