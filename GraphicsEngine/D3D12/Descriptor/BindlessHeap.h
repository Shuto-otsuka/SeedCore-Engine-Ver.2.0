#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/FrameRing.h>

namespace SeedCore
{
	class SEEDCORE_API BindlessHeap :public NonTransferable
	{
	public:
		BindlessHeap() = default;
		~BindlessHeap() = default;

		Bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, Uint maxCount);

		[[nodiscard]] Uint AllocateIndex();

		/// [EN] Queues index for reuse LATER, not immediately. The caller usually
		///      frees a descriptor that the in-flight frames are still reading
		///      (a streamed texture mip being upgraded, an evicted cluster page),
		///      and handing that slot straight back would let the next
		///      AllocateIndex overwrite a descriptor the GPU is about to read -
		///      it would then sample a completely different resource. The slot
		///      only returns to the free list once Retire() has been called
		///      enough times for every frame that could reference it to finish.
		/// [JP] index を「後で」再利用するために積む（即座には戻さない）。
		///      呼び出し側が解放するディスクリプタは、インフライトのフレームが
		///      まだ読んでいることが多い（ストリーミングのミップ昇格、追い出された
		///      クラスタページなど）。そのスロットを即座に返すと、次の
		///      AllocateIndex が GPU のこれから読むディスクリプタを上書きしてしまい、
		///      まったく別のリソースを参照することになる。スロットは、参照しうる
		///      全フレームが完了するだけの回数 Retire() が呼ばれて初めて
		///      フリーリストへ戻る。
		void FreeIndex(Uint index);

		/// [EN] Keeps resource alive until the in-flight frames that may still
		///      reference it have finished. Same hazard as FreeIndex: dropping the
		///      last reference the moment a streamed resource is replaced destroys
		///      it while the GPU is still reading it.
		/// [JP] resource を、まだ参照しうるインフライトのフレームが完了するまで
		///      生かしておく。危険性は FreeIndex と同じで、ストリーミング
		///      リソースを差し替えた瞬間に最後の参照を落とすと、GPU がまだ
		///      読んでいる最中に破棄されてしまう。
		void DeferRelease(Microsoft::WRL::ComPtr<ID3D12Resource> resource);

		/// [EN] Advances one slot of the deferred-reclaim ring and actually
		///      releases whatever was queued frameCount frames ago. Call exactly
		///      once per frame, after that frame's command list is submitted.
		/// [JP] 遅延回収リングを 1 スロット進め、frameCount フレーム前に積まれた
		///      ものを実際に解放する。そのフレームのコマンドリストを提出した後に、
		///      フレームあたり厳密に 1 回だけ呼ぶこと。
		void Retire();

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(Uint index)const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(Uint index)const;

		[[nodiscard]] ID3D12DescriptorHeap* Heap()const;

	private:
		/// [EN] One extra slot beyond the frames in flight, so a descriptor freed
		///      part-way through frame N is never reused before frame N itself has
		///      finished on the GPU.
		/// [JP] インフライトのフレーム数より 1 つ多く持つ。フレーム N の途中で
		///      解放されたディスクリプタが、フレーム N 自身が GPU 上で完了する前に
		///      再利用されないようにするため。
		static constexpr Uint deferredSlotCount = FrameRing::frameCount + 1;

		ResourcePtr<DescriptorHeap> heap_;

		DynamicArray<Uint> freeLists_;

		DynamicArray<Uint> pendingIndices_[deferredSlotCount];

		DynamicArray<Microsoft::WRL::ComPtr<ID3D12Resource>> pendingResources_[deferredSlotCount];

		Uint deferredSlot_ = 0;

		Uint maxCount_ = 0;

		Bool exhaustedLogged_ = false;
	};
}