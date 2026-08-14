#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/SubsurfaceScattering/SubsurfaceScatteringShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli's
	///      SubsurfaceScatteringRayConstantBuffer — read by both
	///      SubsurfaceScatteringRT.hlsl and DeferredLightingPS.hlsl via
	///      structured_indices.sss_ray_constant_index_. Must stay byte-for-byte
	///      in sync with the HLSL side.
	/// [JP] Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli の
	///      SubsurfaceScatteringRayConstantBuffer と対応。
	///      SubsurfaceScatteringRT.hlsl と DeferredLightingPS.hlsl の両方が
	///      structured_indices.sss_ray_constant_index_ 経由で読む。HLSL 側と
	///      バイト単位で一致させること。
	struct SubsurfaceScatteringRayConstantBuffer
	{
		/// [EN] Characteristic falloff distance inside the object:
		///      transmittance = exp(-thickness / scatterDistance_). Smaller =
		///      more opaque.
		/// [JP] オブジェクト内部での光の減衰特性距離
		///      (transmittance = exp(-厚み / scatterDistance_))。小さいほど不透明。
		Float scatterDistance_ = 0.5f;

		/// [EN] Offset pushing the thickness ray's origin INTO the surface so
		///      it doesn't immediately hit the face it started on.
		/// [JP] 厚み計測レイの原点を表面の内側へ押し込むオフセット。撃ち出した
		///      面自身に即ヒットしないようにする。
		Float thicknessBias_ = 0.02f;

		/// [EN] Max thickness to search — thicker areas count as opaque.
		/// [JP] 探索する最大厚み。これより厚い箇所は不透明扱い。
		Float rayTMax_ = 5.0f;

		/// [EN] Overall translucency intensity applied in DeferredLightingPS.hlsl.
		/// [JP] DeferredLightingPS.hlsl で適用する透光の全体強度。
		Float strength_ = 1.0f;

		/// [EN] Tint of the transmitted light (reddish for skin, etc.).
		/// [JP] 透けた光の色(肌なら赤寄り、葉なら緑寄り、など)。
		Float subsurfaceColor_[3] = { 1.0f, 0.35f, 0.25f };
		Float subsurfaceScatteringPadding_ = 0.0f;

		template<class Archive>
		void serialize(Archive& archive)
		{
			archive(
				cereal::make_nvp("scatterDistance", scatterDistance_),
				cereal::make_nvp("thicknessBias", thicknessBias_),
				cereal::make_nvp("rayTMax", rayTMax_),
				cereal::make_nvp("strength", strength_),
				cereal::make_nvp("subsurfaceColor", subsurfaceColor_));
		}
	};

	/**
	* [EN]
	* Dispatches the ray-traced subsurface scattering compute pass
	* (SubsurfaceScatteringRT.hlsl) into a single-channel transmittance texture
	* and leaves it in PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl
	* to sample. The pass is deterministic (one fixed thickness ray toward the
	* directional light per pixel), so unlike ShadowRenderer /
	* AmbientOcclusionRenderer there is NO denoiser, NO ping-pong accumulation
	* and NO per-view chain — one texture is written and consumed within each
	* view's flush, sequentially. If the scene has no TLAS this frame or the
	* DXR PSO is unavailable, the texture is cleared to 0.0 (no translucency)
	* instead.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ表面下散乱のコンピュートパス(SubsurfaceScatteringRT.hlsl)を
	* 1チャンネルの透過率テクスチャへディスパッチし、DeferredLightingPS.hlsl
	* がサンプルできるよう PIXEL_SHADER_RESOURCE 状態にしておく。このパスは
	* 決定論的(1ピクセルにつきディレクショナルライトへ固定の厚み計測レイ1本)
	* なので、ShadowRenderer / AmbientOcclusionRenderer と違ってデノイザも
	* ピンポン蓄積もビュー別チェーンも無い — 各ビューの Flush 内で書いて
	* 読むを順番に行う1枚のテクスチャで足りる。今フレーム TLAS が無い、
	* または DXR PSO が無い場合はテクスチャを 0.0(透光なし)でクリアする。
	*/
	class SubsurfaceScatteringRenderer
	{
	public:
		SubsurfaceScatteringRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~SubsurfaceScatteringRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer and registers its bindless
		///      index into IndicesSystem. Must run before IndicesSystem::
		///      UploadEditor/UploadGame bakes this frame's indices. No GPU
		///      work. (Same contract as ShadowRenderer::PrepareFrame.)
		/// [JP] チューニング用定数バッファを更新し、その bindless インデックスを
		///      IndicesSystem へ登録する。IndicesSystem::UploadEditor/UploadGame
		///      が今フレームのインデックスを確定する前に呼ぶこと。GPU 処理は
		///      無い。(ShadowRenderer::PrepareFrame と同じ契約。)
		void PrepareFrame(const SubsurfaceScatteringRayConstantBuffer& settings);

		/// [EN] The actual GPU work: dispatches SubsurfaceScatteringRT.hlsl
		///      into the transmittance texture (or clears it to 0.0 if
		///      tlasValid is false or the DXR PSO is missing), leaving it in
		///      PIXEL_SHADER_RESOURCE state. Requires the G-Buffer
		///      depth/normal to already be written.
		/// [JP] 実際の GPU 処理: SubsurfaceScatteringRT.hlsl を透過率テクスチャへ
		///      ディスパッチする（tlasValid が false か DXR PSO が無ければ 0.0 で
		///      クリア）。テクスチャは PIXEL_SHADER_RESOURCE 状態で終える。
		///      G-Buffer の深度/法線が書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid);

	private:
		SubsurfaceScatteringShader subsurfaceScatteringShader_;

		ResourcePtr<ConstantBuffer<SubsurfaceScatteringRayConstantBuffer>> tuningBuffer_;

		Microsoft::WRL::ComPtr<ID3D12Resource> transmittanceResource_;
		D3D12_RESOURCE_STATES transmittanceState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 transmittanceUnorderedAccessViewIndex_ = 0;
		Uint32 transmittanceShaderResourceViewIndex_ = 0;

		/// [EN] Non-shader-visible UAV descriptor required by
		///      ClearUnorderedAccessViewFloat alongside the shader-visible one.
		/// [JP] ClearUnorderedAccessViewFloat がシェーダ可視の UAV と併せて
		///      要求する、非シェーダ可視の UAV ディスクリプタ。
		DescriptorHeap clearHeap_;
		Uint32 clearIndex_ = 0;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
