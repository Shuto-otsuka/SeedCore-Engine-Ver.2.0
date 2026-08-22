#include <GraphicsEngine/D3D12/Context/D3D12Context.h>
#include <GraphicsEngine/D3D12/Context/D3D12Factory.h>
#include <GraphicsEngine/D3D12/Context/D3D12Adapter.h>
#include <GraphicsEngine/D3D12/Context/D3D12Device.h>
#include <GraphicsEngine/D3D12/Context/D3D12Fence.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandAllocator.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Context/D3D12DebugLayer.h>
#include <GraphicsEngine/D3D12/D3D12Types.h>
#include <GraphicsEngine/D3D12/FrameRing.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	D3D12Context::D3D12Context()
	{
		/// No Code
	}

	D3D12Context::~D3D12Context()
	{
		/// No Code
	}

	Bool D3D12Context::Initialize()
	{
		factory_ = MakePtr<D3D12Factory>();
		adapter_ = MakePtr<D3D12Adapter>();
		device_ = MakePtr<D3D12Device>();

		if (!CreateDevice())
		{
			return false;
		}

		ID3D12Device* device = device_->Get();
		directQueue_ = MakePtr<D3D12CommandQueue>(device, D3D12CommandType::Direct);
		copyQueue_ = MakePtr<D3D12CommandQueue>(device, D3D12CommandType::Copy);
		computeQueue_ = MakePtr<D3D12CommandQueue>(device, D3D12CommandType::Compute);

		directAllocator_ = MakePtr<D3D12CommandAllocator>();
		copyAllocator_ = MakePtr<D3D12CommandAllocator>();
		computeAllocator_ = MakePtr<D3D12CommandAllocator>();

		if (!CreateCommandAllocator())
		{
			return false;
		}

		directList_ = MakePtr<D3D12CommandList>();
		copyList_ = MakePtr<D3D12CommandList>();
		computeList_ = MakePtr<D3D12CommandList>();

		if (!CreateCommandList())
		{
			return false;
		}

		directList_->Close();
		copyList_->Close();
		computeList_->Close();

		return true;
	}

	void D3D12Context::BeginFrame()
	{
		/// [EN] Frames in flight: only wait for the frame that last used THIS
		///      ring slot (frame N-frameCount), so CPU recording of frame N
		///      overlaps GPU execution of frame N-1.
		/// [JP] フレームインフライト: このリングスロットを最後に使った
		///      フレーム（N-frameCount）の完了だけを待つ。これにより
		///      フレーム N の CPU 記録と N-1 の GPU 実行が重なる。
		/// [EN] Report a device lost during the previous frame here, at the very
		///      top of the frame, before anything else touches D3D12. Present is
		///      too late to be relied on: once the device is hung, an allocation
		///      or acceleration-structure call earlier in the frame can block
		///      indefinitely, so the frame never reaches Present and its
		///      device-removed report (with the DRED breadcrumbs) never runs.
		/// [JP] 前フレーム中に失われたデバイスを、他のどの D3D12 呼び出しよりも
		///      先にフレーム先頭で報告する。Present 頼みでは遅い: デバイスが
		///      ハングすると、フレーム前半の確保や加速構造の呼び出しが
		///      無期限にブロックしうるため、Present とそのデバイス削除報告
		///      (DRED ブレッドクラム付き)まで到達しないことがある。
		SC_DEVICE_REMOVED_CHECK(device_->Get()->GetDeviceRemovedReason(), device_->Get(), "前フレームのGPU処理中にデバイスが失われました");

		Uint frame = FrameRing::Index();
		directQueue_->WaitFor(frameFenceValues_[frame]);

		directAllocators_[frame]->Reset();
		directList_->Reset(directAllocators_[frame]->Get());
	}

	void D3D12Context::EndFrame()
	{
		directList_->Close();
		ExecuteDirect();

		directQueue_->Signal();
		frameFenceValues_[FrameRing::Index()] = directQueue_->LastFenceValue();
		FrameRing::Advance();
	}

	IDXGIFactory7* D3D12Context::GetFactory()const
	{
		return factory_->Get();
	}

	ID3D12Device* D3D12Context::GetDevice()const
	{
		return device_->Get();
	}

	D3D12CommandQueue* D3D12Context::GetDirectQueue()const
	{
		return directQueue_.get();
	}

	D3D12CommandQueue* D3D12Context::GetCopyQueue()const
	{
		return copyQueue_.get();
	}

	D3D12CommandQueue* D3D12Context::GetComputeQueue()const
	{
		return computeQueue_.get();
	}

	D3D12CommandAllocator* D3D12Context::GetDirectAllocator()const
	{
		return directAllocator_.get();
	}

	D3D12CommandAllocator* D3D12Context::GetCopyAllocator()const
	{
		return copyAllocator_.get();
	}

	D3D12CommandAllocator* D3D12Context::GetComputeAllocator()const
	{
		return computeAllocator_.get();
	}

	D3D12CommandList* D3D12Context::GetDirectList()const
	{
		return directList_.get();
	}

	D3D12CommandList* D3D12Context::GetCopyList()const
	{
		return copyList_.get();
	}

	D3D12CommandList* D3D12Context::GetComputeList()const
	{
		return computeList_.get();
	}

	D3D12Adapter* D3D12Context::GetAdapter()const
	{
		return adapter_.get();
	}

	void D3D12Context::ExecuteDirect()
	{
		directQueue_->Execute(directList_->Get());
	}

	Bool D3D12Context::CreateDevice()
	{
		if (!factory_->Create())
		{
			return false;
		}
		if (!adapter_->Create(factory_->Get()))
		{
			return false;
		}
		if (!device_->Create(adapter_->Get()))
		{
			return false;
		}

		return true;
	}

	Bool D3D12Context::CreateCommandAllocator()
	{
		ID3D12Device* device = device_->Get();
		if (!directAllocator_->Create(device, *directQueue_))
		{
			return false;
		}
		for (Uint frame = 0; frame < FrameRing::frameCount; frame++)
		{
			directAllocators_[frame] = MakePtr<D3D12CommandAllocator>();
			if (!directAllocators_[frame]->Create(device, *directQueue_))
			{
				return false;
			}
		}
		if (!copyAllocator_->Create(device, *copyQueue_))
		{
			return false;
		}
		if (!computeAllocator_->Create(device, *computeQueue_))
		{
			return false;
		}

		return true;
	}

	Bool D3D12Context::CreateCommandList()
	{
		ID3D12Device* device = device_->Get();
		if (!directList_->Create(device, *directQueue_, *directAllocator_))
		{
			return false;
		}
		if (!copyList_->Create(device, *copyQueue_, *copyAllocator_))
		{
			return false;
		}
		if (!computeList_->Create(device, *computeQueue_, *computeAllocator_))
		{
			return false;
		}

		return true;
	}
}