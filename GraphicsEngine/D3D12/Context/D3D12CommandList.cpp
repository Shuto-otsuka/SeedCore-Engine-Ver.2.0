#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/Log/DxFail.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandAllocator.h>

namespace SeedCore
{
	Bool D3D12CommandList::Create(ID3D12Device* device, const D3D12CommandQueue& cmdQueue, const D3D12CommandAllocator& cmdAllocator)
	{
		HRESULT hr{ S_OK };

		D3D12_COMMAND_LIST_TYPE type = ToDxCmdType(cmdQueue.GetType());

		hr = device->CreateCommandList(0, type, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&cmdList_));

		return SUCCEEDED(hr);
	}

	void D3D12CommandList::Close()
	{
		cmdList_->Close();
	}

	void D3D12CommandList::Barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		cmdList_->ResourceBarrier(1, &barrier);
	}

	void D3D12CommandList::Reset(ID3D12CommandAllocator* cmdAllocator)
	{
		HRESULT hr{ S_OK };

		hr = cmdList_->Reset(cmdAllocator, nullptr);
		SC_HR_CHECK(hr, "コマンドリストのResetに失敗しました");
	}

	ID3D12GraphicsCommandList6* D3D12CommandList::Get()const
	{
		return cmdList_.Get();
	}
}