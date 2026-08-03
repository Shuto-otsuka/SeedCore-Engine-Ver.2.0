#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/D3D12Common.h>

namespace SeedCore
{
	class D3D12Factory;
	class D3D12Adapter;
	class D3D12Device;
	class D3D12Fence;
	class D3D12CommandQueue;
	class D3D12CommandAllocator;
	class D3D12CommandList;

	class SEEDCORE_API D3D12Context :public NonTransferable
	{
	public:
		D3D12Context();
		~D3D12Context();

		Bool Initialize();

		void BeginFrame();

		void EndFrame();

		IDXGIFactory7* GetFactory()const;

		ID3D12Device* GetDevice()const;

		D3D12CommandQueue* GetDirectQueue()const;

		D3D12CommandQueue* GetCopyQueue()const;

		D3D12CommandQueue* GetComputeQueue()const;

		D3D12CommandAllocator* GetDirectAllocator()const;

		D3D12CommandAllocator* GetCopyAllocator()const;

		D3D12CommandAllocator* GetComputeAllocator()const;

		D3D12CommandList* GetDirectList()const;

		D3D12CommandList* GetCopyList()const;

		D3D12CommandList* GetComputeList()const;

		D3D12Adapter* GetAdapter()const;

	private:
		void ExecuteDirect();

		Bool CreateDevice();
		Bool CreateCommandAllocator();
		Bool CreateCommandList();

	private:
		ResourcePtr<D3D12Adapter> adapter_;
		ResourcePtr<D3D12Factory> factory_;
		ResourcePtr<D3D12Device> device_;

		ResourcePtr<D3D12CommandQueue> directQueue_;
		ResourcePtr<D3D12CommandQueue> copyQueue_;
		ResourcePtr<D3D12CommandQueue> computeQueue_;

		/// [EN] One direct allocator per frame in flight — the GPU may still be
		///      consuming the previous frame's commands while the CPU records.
		/// [JP] インフライトフレーム数ぶんのダイレクトアロケータ — CPU が記録
		///      している間、GPU はまだ前フレームのコマンドを消費している可能性がある。
		ResourcePtr<D3D12CommandAllocator> directAllocators_[2];
		Uint64 frameFenceValues_[2] = { 0, 0 };

		ResourcePtr<D3D12CommandAllocator> directAllocator_;
		ResourcePtr<D3D12CommandAllocator> copyAllocator_;
		ResourcePtr<D3D12CommandAllocator> computeAllocator_;

		ResourcePtr<D3D12CommandList> directList_;
		ResourcePtr<D3D12CommandList> copyList_;
		ResourcePtr<D3D12CommandList> computeList_;
	};
}