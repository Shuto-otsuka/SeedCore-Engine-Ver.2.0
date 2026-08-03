#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12CommandQueue;

	class D3D12CommandAllocator :public NonTransferable
	{
	public:
		D3D12CommandAllocator() = default;
		~D3D12CommandAllocator() = default;

		Bool Create(ID3D12Device* device, const D3D12CommandQueue& cmdQueue);

		void Reset();

		ID3D12CommandAllocator* Get()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator_;
	};
}