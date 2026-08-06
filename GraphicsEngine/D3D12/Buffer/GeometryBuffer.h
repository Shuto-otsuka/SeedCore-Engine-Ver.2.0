#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandList;

	class GeometryBuffer
	{
	private:
		static constexpr Int bufferCount_ = 5;

		const DXGI_FORMAT formats_[bufferCount_] =
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,		// RT0: base_color.rgb + metallic
			DXGI_FORMAT_R16G16B16A16_UNORM,	// RT1: octNormal.rg + roughness (.a unused/reserved - KHR extension scalars are no longer baked per-pixel, DeferredLightingPS.hlsl reads them straight from ModelInstance via VisID)
			DXGI_FORMAT_R16G16_FLOAT,		// RT2: velocity
			DXGI_FORMAT_R11G11B10_FLOAT,		// RT3: emissive.rgb (raw texture*factor - emissive_strength_ is applied at lighting time, not baked in)
			DXGI_FORMAT_R32G32_UINT		// RT4: visibility id (instance_index, pack(meshlet_index, triangle_in_meshlet_index)) - VisibilityBuffer groundwork
		};

	public:
		GeometryBuffer() = default;
		~GeometryBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Begin(D3D12CommandList* cmdList);

		void BeginDepthOnly(D3D12CommandList* cmdList);

		/// [EN] Binds only RT4 (visibility id) + depth - the raster G-Buffer pass
		///      (StaticModelMS/PS, SkeletalModelMS/PS) now writes only that one
		///      color target; RT0/1/2/3 are written afterward by the
		///      VisibilityBuffer material resolve compute pass
		///      (Model/MaterialResolveCS.hlsl) via UAV, not through the OM stage.
		/// [JP] RT4(visibility id) + depth のみをバインドする — ラスタ G-Buffer
		///      パス(StaticModelMS/PS, SkeletalModelMS/PS)はもうそのカラー
		///      ターゲット1枚しか書かない。RT0/1/2/3 はこの後 VisibilityBuffer
		///      マテリアル解決コンピュートパス(Model/MaterialResolveCS.hlsl)が
		///      UAV 経由で書く(OM ステージを通さない)。
		void BeginVisibility(D3D12CommandList* cmdList);

		void BeginDepth(D3D12CommandList* cmdList);

		void Clear(D3D12CommandList* cmdList);

		void ClearDepth(D3D12CommandList* cmdList);

		/// [EN] Clears only RT4 (visibility id) - see BeginVisibility.
		/// [JP] RT4(visibility id)のみをクリアする — BeginVisibility 参照。
		void ClearVisibility(D3D12CommandList* cmdList);

		void EndColor(D3D12CommandList* cmdList);

		void EndDepthNonPixel(D3D12CommandList* cmdList);

		void EndDepth(D3D12CommandList* cmdList);

		void End(D3D12CommandList* cmdList);

		[[nodiscard]] Uint32 ColorShaderResourceViewIndex(Int index)const;

		[[nodiscard]] Uint32 DepthShaderResourceViewIndex()const;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilViewHandle()const;

		/// [EN] Raw resource pointer, for callers that need the native
		///      ID3D12Resource* directly instead of a bindless index - e.g.
		///      DlssBufferTag (DLSS/DlssManager.h), which NVIDIA Streamline
		///      tags by native pointer, not by this engine's bindless heap.
		/// [JP] bindlessインデックスではなく生の ID3D12Resource* を直接必要と
		///      する呼び出し側向け — 例えば DlssBufferTag(DLSS/DlssManager.h)は
		///      NVIDIA Streamline がこのエンジンの bindless ヒープではなく
		///      ネイティブポインタでタグ付けする。
		[[nodiscard]] ID3D12Resource* ColorResource(Int index)const;

		[[nodiscard]] ID3D12Resource* DepthResource()const;

		/// [EN] Bindless UAV index onto RT0/1/2/3 (everything except RT4, the
		///      VisibilityBuffer id, which is written once by the raster
		///      G-Buffer pass and only ever read afterward) - these RTs are
		///      created with D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS in
		///      addition to ALLOW_RENDER_TARGET, so a compute pass can patch
		///      them after the raster G-Buffer pass. RT2 (velocity) is patched
		///      for background (sky/cloud) pixels by DLSS/DlssBackgroundVelocityCS.hlsl;
		///      RT0/1/3 are patched wholesale by the VisibilityBuffer material
		///      resolve pass (Model/MaterialResolveCS.hlsl).
		/// [JP] RT0/1/2/3(RT4=VisibilityBuffer id を除く全て。RT4 はラスタ
		///      G-Bufferパスが一度書くだけで、以降は読み取り専用)への bindless
		///      UAV インデックス — これらの RT は ALLOW_RENDER_TARGET に加えて
		///      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS 付きで生成する。
		///      ラスタG-Bufferパスの後にコンピュートパスでパッチできるように
		///      するため。RT2(velocity)は背景(空/雲)ピクセルを
		///      DLSS/DlssBackgroundVelocityCS.hlsl がパッチし、RT0/1/3 は
		///      VisibilityBuffer マテリアル解決パス(Model/MaterialResolveCS.hlsl)が
		///      丸ごとパッチする。
		[[nodiscard]] Uint32 ColorUnorderedAccessViewIndex(Int index)const;

		/// [EN] Thin alias for ColorUnorderedAccessViewIndex(2), kept so existing
		///      DLSS-RR callers (DlssRayReconstructionRenderer) do not need to
		///      know the RT index.
		/// [JP] ColorUnorderedAccessViewIndex(2) の薄いエイリアス。既存の
		///      DLSS-RR 呼び出し側(DlssRayReconstructionRenderer)が RT の
		///      インデックスを知らなくて済むように残している。
		[[nodiscard]] Uint32 VelocityUnorderedAccessViewIndex()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> colorResources_[bufferCount_];
		D3D12_RESOURCE_STATES colorStates_[bufferCount_] = {};

		Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_;
		D3D12_RESOURCE_STATES depthState_ = D3D12_RESOURCE_STATE_COMMON;

		DescriptorHeap renderTargetViewHeap_;
		DescriptorHeap depthStencilViewHeap_;

		Uint32 colorShaderResourceViewIndices_[bufferCount_] = {};
		Uint32 depthShaderResourceViewIndex_ = 0;
		Uint32 colorUnorderedAccessViewIndices_[bufferCount_] = {};

		D3D12_VIEWPORT viewport_;
	};
}
