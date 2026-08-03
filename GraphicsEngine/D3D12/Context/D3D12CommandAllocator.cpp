#include <GraphicsEngine/D3D12/Context/D3D12CommandAllocator.h>
#include <FoundationEngine/Log/DxFail.h>
#include <GraphicsEngine/D3D12/D3D12Types.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>

namespace SeedCore
{
	Bool D3D12CommandAllocator::Create(ID3D12Device* device, const D3D12CommandQueue& cmdQueue)
	{
		HRESULT hr{ S_OK };

		D3D12_COMMAND_LIST_TYPE type = ToDxCmdType(cmdQueue.GetType());

		hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&cmdAllocator_));

		return SUCCEEDED(hr);
	}

	void D3D12CommandAllocator::Reset()
	{
		HRESULT hr{ S_OK };

		hr = cmdAllocator_->Reset();
		SC_HR_CHECK(hr, "コマンドアロケーターのResetに失敗しました");
	}

	ID3D12CommandAllocator* D3D12CommandAllocator::Get()const
	{
		return cmdAllocator_.Get();
	}
}