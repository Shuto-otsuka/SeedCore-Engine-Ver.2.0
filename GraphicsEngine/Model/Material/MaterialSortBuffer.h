#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class BindlessHeap;
	class IndicesSystem;

	/**
	* [EN]
	* Manages GPU resources for the VisibilityBuffer material sort (see
	* Model.hlsli's MATERIAL_SORT_BUCKET_COUNT and Model/MaterialClassifyCS.hlsl /
	* MaterialPrefixSumCS.hlsl / MaterialScatterCS.hlsl). Instead of
	* Model/MaterialResolveCS.hlsl resolving pixels in raw screen order (where
	* neighbouring threads in a wave can land on completely different
	* instances/materials, causing texture-index divergence), pixels are first
	* bucketed by instance index so the resolve dispatch walks them in an order
	* where nearby threads share the same/nearby material.
	*
	* Resources:
	*   - Bucket: RWStructuredBuffer<uint>[MATERIAL_SORT_BUCKET_COUNT], UAV.
	*     Per-bucket pixel count, then turned in place into exclusive-scan
	*     offsets, then further turned into atomic write cursors.
	*   - Sorted Pixel List: RWStructuredBuffer<uint>[width * height], UAV.
	*     Each foreground pixel's linear coordinate, grouped by bucket.
	*     Cleared to MATERIAL_SORT_INVALID_PIXEL (0xFFFFFFFF) every frame -
	*     unwritten (background) slots must not resolve stale pixels from the
	*     previous frame/view.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* VisibilityBuffer マテリアルソート用の GPU リソース管理(Model.hlsli の
	* MATERIAL_SORT_BUCKET_COUNT と Model/MaterialClassifyCS.hlsl /
	* MaterialPrefixSumCS.hlsl / MaterialScatterCS.hlsl 参照)。
	* Model/MaterialResolveCS.hlsl が生のスクリーン順でピクセルを解決すると
	* (同じウェーブの隣接スレッドが全く違うインスタンス/マテリアルに
	* 着地しうり、テクスチャインデックスの分岐が起きる)、先にピクセルを
	* instance_index でバケット分けし、解決ディスパッチが近いスレッドほど
	* 同じ/近いマテリアルを共有する順序で辿れるようにする。
	*
	* リソース:
	*   - Bucket: RWStructuredBuffer<uint>[MATERIAL_SORT_BUCKET_COUNT]、UAV。
	*     バケットごとのピクセル数 → その場で排他的スキャンのオフセットへ →
	*     さらに atomic 書き込みカーソルへ変化する。
	*   - Sorted Pixel List: RWStructuredBuffer<uint>[width * height]、UAV。
	*     各前景ピクセルの線形座標を、バケットごとにまとめたもの。毎フレーム
	*     MATERIAL_SORT_INVALID_PIXEL(0xFFFFFFFF)でクリアする -
	*     未書き込み(背景)のスロットが前フレーム/前ビューの古いピクセルを
	*     解決してしまわないようにするため。
	*/
	class MaterialSortBuffer
	{
	public:
		static constexpr Uint bucketCount_ = 1024;

		MaterialSortBuffer() = default;
		~MaterialSortBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Clear(ID3D12GraphicsCommandList* cmdList);

		void Barrier(ID3D12GraphicsCommandList* cmdList)const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> bucketBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> sortedPixelListBuffer_;

		DescriptorHeap clearHeap_;

		Uint bucketUAVIndex_ = 0;
		Uint sortedPixelListUAVIndex_ = 0;

		Uint clearBucketIndex_ = 0;
		Uint clearSortedPixelListIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;
	};
}
