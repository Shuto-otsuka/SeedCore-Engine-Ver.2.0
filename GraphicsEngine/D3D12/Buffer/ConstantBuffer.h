#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/DxFail.h>
#include <GraphicsEngine/D3D12/FrameRing.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Frame-ring constant buffer: one upload resource (and CBV) per frame in
	* flight. Update writes the current frame's slot, Address / GetIndex return
	* the current slot, so the CPU never touches memory the GPU is still reading.
	* Consequence: Update must be called every frame the buffer is used, and
	* GetIndex must be re-queried per frame (never cache it across frames).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フレームリング定数バッファ: インフライトフレーム数ぶんのアップロード
	* リソース（と CBV）を持つ。Update は現在フレームのスロットに書き、
	* Address / GetIndex も現在スロットを返すため、GPU が読んでいる最中の
	* メモリに CPU が触れることはない。その代わり、使用するフレームでは毎回
	* Update を呼ぶこと、GetIndex はフレームを跨いでキャッシュしないこと。
	*/
	template<typename T>
	class ConstantBuffer :public NonCopyable
	{
	public:
		ConstantBuffer(ID3D12Device* device, BindlessHeap* heap) : heap_(heap)
		{
			static_assert(sizeof(T) % 16 == 0, "ConstantBuffer type must be 16-byte aligned");

			constexpr Uint64 alignedSize = (sizeof(T) + 255) & ~255ull;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = alignedSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			for (Uint frame = 0; frame < FrameRing::frameCount; frame++)
			{
				HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resources_[frame]));
				SC_HR_CHECK(hr, "定数バッファの生成に失敗しました");

				indices_[frame] = heap->AllocateIndex();
				D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->CPUHandle(indices_[frame]);
				if (handle.ptr != 0)
				{
					D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc{};
					constantBufferViewDesc.BufferLocation = resources_[frame]->GetGPUVirtualAddress();
					constantBufferViewDesc.SizeInBytes = static_cast<Uint>(alignedSize);
					device->CreateConstantBufferView(&constantBufferViewDesc, handle);
				}

				resources_[frame]->Map(0, nullptr, reinterpret_cast<void**>(&mappedPtrs_[frame]));
			}
		}

		~ConstantBuffer()
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

		void Update(const T& data)
		{
			memcpy(mappedPtrs_[FrameRing::Index()], &data, sizeof(T));
		}

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS Address()const
		{
			return resources_[FrameRing::Index()]->GetGPUVirtualAddress();
		}

		[[nodiscard]] Uint GetIndex()const
		{
			return indices_[FrameRing::Index()];
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resources_[FrameRing::frameCount];
		void* mappedPtrs_[FrameRing::frameCount] = {};
		Uint indices_[FrameRing::frameCount] = {};

		BindlessHeap* heap_;
	};
}
