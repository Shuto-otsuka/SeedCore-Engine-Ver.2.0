#pragma once
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates screen-sized, DEFAULT-heap RWStructuredBuffer<T> reservoirs for
	* ReSTIR reservoir chains: UAV + SRV bindless indices, plus a matching
	* pair of RAW (R32_TYPELESS) UAVs - one clear-heap (CPU handle), one
	* shader-visible (GPU handle) - for ClearUnorderedAccessViewUint (zeroing
	* M_/W_ to mark "no history" - see whichever renderer's temporal combine
	* reads this). ClearUnorderedAccessView* cannot target a structured UAV
	* at all (D3D12 debug-layer error CLEARUNORDEREDACCESSVIEW_INCOMPATIBLE_
	* WITH_STRUCTURED_BUFFERS), and per the D3D12 Clear contract the GPU and
	* CPU handles passed to it must describe the very same view - so both
	* the CPU and the GPU copy have to be this RAW alias, not the structured
	* UAV used for actual reservoir reads (see LightSystem.cpp's
	* clusterData clear for the same pattern). Shared by every ReSTIR-based
	* renderer (GlobalIlluminationRenderer today, Reflection/others later)
	* rather than each duplicating the same buffer setup. Holds no state of
	* its own - Create() is the only entry point, matching the caller's own
	* resource/state/index arrays (see GlobalIlluminationRenderer.h).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ReSTIR Reservoir チェーン用の、画面サイズの DEFAULT ヒープ
	* RWStructuredBuffer<T> を作る: UAV/SRV の bindless インデックスに加えて、
	* ClearUnorderedAccessViewUint 用に、RAW(R32_TYPELESS)の UAV を
	* clear-heap 用(CPUハンドル)と shader-visible 用(GPUハンドル)の
	* 組で作る(M_/W_ をゼロにして「履歴無し」を示す - 読む側は各レンダラー
	* の時間的結合を参照)。ClearUnorderedAccessView* はそもそも構造化 UAV
	* を対象にできず(D3D12デバッグレイヤーのエラー
	* CLEARUNORDEREDACCESSVIEW_INCOMPATIBLE_WITH_STRUCTURED_BUFFERS)、かつ
	* D3D12 の Clear の仕様上 GPU/CPU 両ハンドルは同じビューを指す必要が
	* あるため、CPU側だけでなく GPU側も、実際の Reservoir 読み書きに使う
	* 構造化 UAV ではなくこの RAW エイリアスにする必要がある
	* (同じパターンは LightSystem.cpp の clusterData クリアも参照)。
	* ReSTIR を使う全レンダラー(現状 GlobalIlluminationRenderer、将来的には
	* Reflection 等)で共有し、各レンダラーが同じバッファ構築を重複させない。
	* 自身は状態を持たない - Create() のみを提供し、呼び出し側自身の
	* リソース/状態/インデックス配列(GlobalIlluminationRenderer.h 参照)に
	* そのまま書き込む形。
	*/
	class ReservoirBuffer
	{
	public:
		static void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 elementCount, Uint32 elementSizeInBytes, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex, Uint32& outClearIndex, Uint32& outClearGpuUnorderedAccessViewIndex);
	};
}
