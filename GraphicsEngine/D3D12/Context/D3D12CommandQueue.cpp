#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	D3D12CommandQueue::D3D12CommandQueue(ID3D12Device* device, D3D12CommandType type) :type_(type)
	{
		HRESULT hr{ S_OK };

		switch (type)
		{
		case D3D12CommandType::Copy:
		{
			D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
			cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			cmdQueueDesc.NodeMask = 0;
			cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			hr = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue_));
			SC_HR_CHECK(hr, "コピー用コマンドキューの生成に失敗しました");
		}
		break;
		case D3D12CommandType::Compute:
		{
			D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
			cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			cmdQueueDesc.NodeMask = 0;
			cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			hr = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue_));
			SC_HR_CHECK(hr, "コンピュート用コマンドキューの生成に失敗しました");
		}
		break;
		case D3D12CommandType::Direct:
			[[fallthrough]];
		default:
		{
			D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
			cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			cmdQueueDesc.NodeMask = 0;
			cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			hr = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue_));
			SC_HR_CHECK(hr, "描画用コマンドキューの生成に失敗しました");
		}
		break;
		}

		fence_ = MakePtr<D3D12Fence>();
		fence_->Create(device);
	}

	void D3D12CommandQueue::Signal()
	{
		lastFenceValue_ = fence_->Next();
		cmdQueue_->Signal(fence_->Get(), lastFenceValue_);
	}

	void D3D12CommandQueue::Wait()
	{
		WaitFor(lastFenceValue_);
	}

	void D3D12CommandQueue::WaitFor(Uint64 fenceValue)
	{
		if (fence_->Get()->GetCompletedValue() < fenceValue)
		{
			HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			fence_->Get()->SetEventOnCompletion(fenceValue, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
	}

	void D3D12CommandQueue::Execute(ID3D12CommandList* cmdList)
	{
		if (!cmdList)
		{
			return;
		}

		ID3D12CommandList* commandLists[] = { cmdList };
		cmdQueue_->ExecuteCommandLists(1, commandLists);
	}

	void D3D12CommandQueue::Execute(DynamicArray<ID3D12CommandList*>& cmdLists)
	{
		if (cmdLists.empty())
		{
			return;
		}

		cmdQueue_->ExecuteCommandLists(static_cast<Uint>(cmdLists.size()), cmdLists.data());
	}

	Uint64 D3D12CommandQueue::LastFenceValue()const
	{
		return lastFenceValue_;
	}

	ID3D12CommandQueue* D3D12CommandQueue::GetCommandQueue()const
	{
		return cmdQueue_.Get();
	}

	ID3D12Fence* D3D12CommandQueue::GetFence()const
	{
		return fence_->Get();
	}

	D3D12CommandType D3D12CommandQueue::GetType()const
	{
		return type_;
	}
}