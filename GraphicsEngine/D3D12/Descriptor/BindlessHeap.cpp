#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	Bool BindlessHeap::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint maxCount)
	{
		heap_ = MakePtr<DescriptorHeap>();
		if (!heap_->Create(device, type, maxCount, true))
		{
			return false;
		}

		maxCount_ = maxCount;
		freeLists_.reserve(maxCount);

		for (Int index = static_cast<Int>(maxCount_) - 1; index >= 0; --index)
		{
			freeLists_.push_back(static_cast<Uint>(index));
		}

		return true;
	}

	Uint BindlessHeap::AllocateIndex()
	{
		std::scoped_lock lock(mutex_);

		if (freeLists_.empty())
		{
			/// [EN] Returning 0xFFFFFFFF here would be fatal: no call site checks
			///      the result, and CPUHandle multiplies the index by the
			///      descriptor size, so the caller would write a descriptor
			///      terabytes past the heap and crash instantly. Alias descriptor 0
			///      instead - the frame renders wrong, but it stays alive and says
			///      why.
			/// [JP] ここで 0xFFFFFFFF を返すのは致命的: 戻り値を検査している
			///      呼び出し箇所は 1 つも無く、CPUHandle はインデックスに
			///      ディスクリプタサイズを掛けるため、ヒープの遥か彼方へ
			///      ディスクリプタを書き込んで即クラッシュする。代わりに
			///      ディスクリプタ 0 をエイリアスする — 描画は壊れるが、
			///      落ちずに理由を伝えられる。
			if (!exhaustedLogged_)
			{
				SC_LOG_ERROR("バインドレスディスクリプタヒープが枯渇しました(上限 {})。以降の確保はディスクリプタ 0 を共有するため描画が乱れます。", maxCount_);
				exhaustedLogged_ = true;
			}
			return 0;
		}

		Uint index = freeLists_.back();
		freeLists_.pop_back();
		return index;
	}

	void BindlessHeap::FreeIndex(Uint index)
	{
		if (index == 0xFFFFFFFF)
		{
			return;
		}

		std::scoped_lock lock(mutex_);
		pendingIndices_[deferredSlot_].push_back(index);
	}

	void BindlessHeap::DeferRelease(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
	{
		if (!resource)
		{
			return;
		}

		std::scoped_lock lock(mutex_);
		pendingResources_[deferredSlot_].push_back(std::move(resource));
	}

	void BindlessHeap::Retire()
	{
		std::scoped_lock lock(mutex_);

		deferredSlot_ = (deferredSlot_ + 1) % deferredSlotCount;

		/// [EN] This slot was last written deferredSlotCount frames ago, so every
		///      frame that could still reference its descriptors and resources has
		///      finished on the GPU by now.
		/// [JP] このスロットへ最後に書き込んだのは deferredSlotCount フレーム前
		///      なので、そのディスクリプタとリソースを参照しうるフレームは
		///      すべて GPU 上で完了済み。
		for (Uint index : pendingIndices_[deferredSlot_])
		{
			freeLists_.push_back(index);
		}
		pendingIndices_[deferredSlot_].clear();

		pendingResources_[deferredSlot_].clear();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE BindlessHeap::CPUHandle(Uint index)const
	{
		return heap_->CPUHandle(index);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE BindlessHeap::GPUHandle(Uint index)const
	{
		return heap_->GPUHandle(index);
	}

	ID3D12DescriptorHeap* BindlessHeap::Heap()const
	{
		return heap_->Get();
	}
}