#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Context/D3D12Fence.h>
#include <GraphicsEngine/D3D12/D3D12Types.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class SEEDCORE_API D3D12CommandQueue :public NonTransferable
	{
	private:
		static constexpr Uint fenceTimeoutMilliseconds = 5000;

	public:
		explicit D3D12CommandQueue(ID3D12Device* device, D3D12CommandType type = D3D12CommandType::Direct);

		void Signal();

		void Wait();

		void WaitFor(Uint64 fenceValue);

		[[nodiscard]] Uint64 LastFenceValue()const;

		void Execute(ID3D12CommandList* cmdList);

		void Execute(DynamicArray<ID3D12CommandList*>& cmdLists);

		ID3D12CommandQueue* GetCommandQueue()const;

		ID3D12Fence* GetFence()const;

		D3D12CommandType GetType()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> cmdQueue_;
		ResourcePtr<D3D12Fence> fence_;

		D3D12CommandType type_;

		Uint64 lastFenceValue_{ 0 };
	};
}