#pragma once
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates screen-sized, DEFAULT-heap RWStructuredBuffer<T> reservoirs for
	* ReSTIR reservoir chains: UAV + SRV bindless indices plus a non-shader-
	* visible clear-heap UAV for ClearUnorderedAccessViewUint (zeroing M_/W_ to
	* mark "no history" - see whichever renderer's temporal combine reads
	* this). Shared by every ReSTIR-based renderer (GlobalIlluminationRenderer
	* today, Reflection/others later) rather than each duplicating the same
	* buffer setup. Holds no state of its own - Create() is the only entry
	* point, matching the caller's own resource/state/index arrays (see
	* GlobalIlluminationRenderer.h).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ReSTIR Reservoir チェーン用の、画面サイズの DEFAULT ヒープ
	* RWStructuredBuffer<T> を作る: UAV/SRV の bindless インデックスに加えて、
	* ClearUnorderedAccessViewUint 用の非シェーダ可視 UAV(M_/W_ をゼロにして
	* 「履歴無し」を示す - 読む側は各レンダラーの時間的結合を参照)。
	* ReSTIR を使う全レンダラー(現状 GlobalIlluminationRenderer、将来的には
	* Reflection 等)で共有し、各レンダラーが同じバッファ構築を重複させない。
	* 自身は状態を持たない - Create() のみを提供し、呼び出し側自身の
	* リソース/状態/インデックス配列(GlobalIlluminationRenderer.h 参照)に
	* そのまま書き込む形。
	*/
	class ReservoirBuffer
	{
	public:
		static void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, DescriptorHeap& clearHeap, Uint32 elementCount, Uint32 elementSizeInBytes, Microsoft::WRL::ComPtr<ID3D12Resource>& outResource, Uint32& outUnorderedAccessViewIndex, Uint32& outShaderResourceViewIndex, Uint32& outClearIndex);
	};
}
