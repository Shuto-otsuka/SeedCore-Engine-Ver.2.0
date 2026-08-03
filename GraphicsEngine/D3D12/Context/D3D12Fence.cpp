#include <GraphicsEngine/D3D12/Context/D3D12Fence.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	Bool D3D12Fence::Create(ID3D12Device* device)
	{
		HRESULT hr{ S_OK };

		hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
		SC_HR_CHECK(hr, "フェンスの生成に失敗しました");

		return SUCCEEDED(hr);
	}

	Uint64 D3D12Fence::Next()
	{
		return ++fenceValue_;
	}

	ID3D12Fence* D3D12Fence::Get()const
	{
		return fence_.Get();
	}
}