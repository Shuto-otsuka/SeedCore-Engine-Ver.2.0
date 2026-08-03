#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class BindlessHeap;
	class IndicesSystem;

	/**
	* [EN]
	* Manages GPU resources for Per-Pixel Linked List (PPLL)
	* Order-Independent Transparency.
	*
	* Resources:
	*   - Head Pointer: R32_UINT 2D texture (screen resolution), UAV.
	*     Each pixel stores the index of its first fragment.
	*   - Fragment Buffer: RWStructuredBuffer<OITFragment>, UAV.
	*     Pool of fragments sized to (width * height * maxLayers).
	*   - Counter: RWByteAddressBuffer (4 bytes), UAV.
	*     Atomic counter for fragment allocation.
	*
	* ClearUnorderedAccessViewUint is used each frame to reset the
	* head pointer to 0xFFFFFFFF and the counter to 0.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Per-Pixel Linked List (PPLL) Order-Independent Transparency 用の
	* GPU リソース管理。
	*
	* リソース:
	*   - ヘッドポインタ: R32_UINT 2D テクスチャ（画面解像度）、UAV。
	*     各ピクセルが最初のフラグメントのインデックスを格納。
	*   - フラグメントバッファ: RWStructuredBuffer<OITFragment>、UAV。
	*     フラグメントプール。サイズは (width * height * maxLayers)。
	*   - カウンター: RWByteAddressBuffer (4 バイト)、UAV。
	*     フラグメント割り当て用のアトミックカウンター。
	*
	* 毎フレーム ClearUnorderedAccessViewUint でヘッドポインタを
	* 0xFFFFFFFF に、カウンターを 0 にリセットする。
	*/
	class OITBuffer
	{
	public:
		static constexpr Uint maxLayers_ = 4;

		OITBuffer() = default;
		~OITBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Clear(ID3D12GraphicsCommandList* cmdList);

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> headPointerTexture_;
		Microsoft::WRL::ComPtr<ID3D12Resource> fragmentBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> counterBuffer_;

		DescriptorHeap clearHeap_;

		Uint headPointerUAVIndex_ = 0;
		Uint fragmentBufferUAVIndex_ = 0;
		Uint counterUAVIndex_ = 0;

		Uint clearHeadPointerIndex_ = 0;
		Uint clearCounterIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;
	};
}
