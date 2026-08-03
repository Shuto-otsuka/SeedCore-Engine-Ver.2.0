#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12CommandQueue;
	class D3D12CommandAllocator;

	class SEEDCORE_API D3D12CommandList :public NonTransferable
	{
	public:
		Bool Create(ID3D12Device* device, const D3D12CommandQueue& cmdQueue, const D3D12CommandAllocator& cmdAllocator);

		void Close();

		void Barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

		void Reset(ID3D12CommandAllocator* cmdAllocator);

		ID3D12GraphicsCommandList6* Get()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList_;
	};
}