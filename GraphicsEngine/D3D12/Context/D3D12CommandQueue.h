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

		/// [EN] Acquires queueMutex_ for a raw ID3D12CommandQueue submission
		///      made outside this class's own Signal()/Execute() (e.g.
		///      DirectX::ResourceUploadBatch::End(GetCommandQueue()) from
		///      the background asset-loading worker) - hold the returned
		///      lock for the duration of that submission.
		/// [JP] このクラス自身の Signal()/Execute() を経由しない、生の
		///      ID3D12CommandQueue への提出(例: バックグラウンドの
		///      アセット読み込みワーカーからの
		///      DirectX::ResourceUploadBatch::End(GetCommandQueue()))の
		///      ために queueMutex_ を取得する - 戻り値のロックはその提出が
		///      終わるまで保持すること。
		[[nodiscard]] std::unique_lock<std::mutex> AcquireLock();

	private:
		/// [EN] Guards every ExecuteCommandLists/Signal call issued against
		///      this physical queue - the main thread (Graphics::Begin/End/
		///      Resize) and the background asset-loading worker
		///      (ResourceCache::StepAsync, via DirectX::ResourceUploadBatch)
		///      both submit to the same ID3D12CommandQueue, and D3D12
		///      requires the caller to serialize concurrent access to one
		///      queue.
		/// [JP] この物理キューへ発行される ExecuteCommandLists/Signal を
		///      全て保護する - メインスレッド(Graphics::Begin/End/Resize)と
		///      バックグラウンドのアセット読み込みワーカー
		///      (ResourceCache::StepAsync、DirectX::ResourceUploadBatch 経由)
		///      が同じ ID3D12CommandQueue へ提出するため、D3D12 は1つの
		///      キューへの同時アクセスを呼び出し側が排他することを要求する。
		std::mutex queueMutex_;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> cmdQueue_;
		ResourcePtr<D3D12Fence> fence_;

		D3D12CommandType type_;

		Uint64 lastFenceValue_ = 0;
	};
}