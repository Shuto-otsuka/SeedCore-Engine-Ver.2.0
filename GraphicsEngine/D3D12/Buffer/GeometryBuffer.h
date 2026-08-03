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
			DXGI_FORMAT_R16G16B16A16_UNORM,	// RT1: octNormal.rg + roughness + pack8x8(clearcoat_factor, clearcoat_roughness)
			DXGI_FORMAT_R8G8B8A8_UNORM,		// RT2: anisotropy + specularColor.rgb
			DXGI_FORMAT_R16G16_FLOAT,		// RT3: velocity
			DXGI_FORMAT_R11G11B10_FLOAT		// RT4: emissive (emissive_strength baked in)
		};

	public:
		GeometryBuffer() = default;
		~GeometryBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Begin(D3D12CommandList* cmdList);

		void BeginDepthOnly(D3D12CommandList* cmdList);

		void BeginColor(D3D12CommandList* cmdList);

		void BeginDepth(D3D12CommandList* cmdList);

		void Clear(D3D12CommandList* cmdList);

		void ClearDepth(D3D12CommandList* cmdList);

		void ClearColor(D3D12CommandList* cmdList);

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

		/// [EN] Bindless UAV index onto RT3 (velocity) - only RT3 is created
		///      with D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS in addition to
		///      ALLOW_RENDER_TARGET, so a compute pass can patch background
		///      (sky/cloud) pixels' velocity after the raster G-Buffer pass -
		///      see DLSS/DlssBackgroundVelocityCS.hlsl. Rasterized geometry
		///      never touches this path; only DlssRayReconstructionRenderer
		///      uses it, and only when DLSS-RR is active.
		/// [JP] RT3(velocity)への bindless UAV インデックス — RT3 だけ
		///      ALLOW_RENDER_TARGET に加えて
		///      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS 付きで生成する。
		///      ラスタG-Bufferパスの後に、背景(空/雲)ピクセルの速度を
		///      コンピュートパスでパッチできるようにするため —
		///      DLSS/DlssBackgroundVelocityCS.hlsl 参照。ラスタ描画されるジオメトリ
		///      はこの経路に一切触れない。DlssRayReconstructionRenderer が
		///      DLSS-RR 有効時にのみ使う。
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
		Uint32 velocityUnorderedAccessViewIndex_ = 0;

		D3D12_VIEWPORT viewport_;
	};
}
