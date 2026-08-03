#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionShader.h>
#include <GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionDenoiseShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli's
	///      AmbientOcclusionRayConstantBuffer — read by both
	///      AmbientOcclusionRT.hlsl and DeferredCompositePS.hlsl via
	///      structured_indices.ao_ray_constant_index_. Must stay byte-for-byte
	///      in sync with the HLSL side.
	/// [JP] Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli の
	///      AmbientOcclusionRayConstantBuffer と対応。AmbientOcclusionRT.hlsl と
	///      DeferredCompositePS.hlsl の両方が
	///      structured_indices.ao_ray_constant_index_ 経由で読む。HLSL 側と
	///      バイト単位で一致させること。
	struct AmbientOcclusionRayConstantBuffer
	{
		/// [EN] World-space AO radius: occluders farther than this don't darken.
		/// [JP] ワールド空間のAO半径。これより遠い遮蔽物は暗くしない。
		Float rayLength_ = 1.0f;

		Float normalBias_ = 0.01f;

		/// [EN] Contrast exponent (pow(ao, power_)) applied in
		///      DeferredCompositePS.hlsl. 1 = as-is, >1 = darker/stronger.
		/// [JP] DeferredCompositePS.hlsl で適用するコントラスト指数
		///      (pow(ao, power_))。1=そのまま、>1=より暗く/強く。
		Float power_ = 1.0f;

		/// [EN] Incremented once per frame by AmbientOcclusionRenderer (not the
		///      UI) — drives the per-pixel RNG seed so the stochastic ray
		///      direction changes every frame.
		/// [JP] AmbientOcclusionRenderer が毎フレーム1つずつ加算する(UI からは
		///      触らない) — ピクセルごとの RNG シードを駆動し、確率的なレイ方向を
		///      毎フレーム変える。
		Uint32 frameIndex_ = 0;

		template<class Archive>
		void serialize(Archive& archive)
		{
			archive(
				cereal::make_nvp("rayLength", rayLength_),
				cereal::make_nvp("normalBias", normalBias_),
				cereal::make_nvp("power", power_));
		}
	};

	/**
	* [EN]
	* Dispatches the ray-traced ambient occlusion compute pass
	* (AmbientOcclusionRT.hlsl) into a raw single-channel noisy openness
	* texture, then denoises it (AmbientOcclusionDenoiseCS.hlsl: temporal
	* reprojection + blend against a ping-ponged accumulation buffer) and
	* leaves the result in PIXEL_SHADER_RESOURCE state for
	* DeferredCompositePS.hlsl to sample. If the scene has no TLAS this frame
	* (nothing to trace against) or the DXR PSO is unavailable, both stages are
	* skipped and the accumulated buffer is cleared to 1.0 (fully open)
	* instead. Same structure as ShadowRenderer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレAOのコンピュートパス(AmbientOcclusionRT.hlsl)を生の1チャンネル
	* ノイズ開放度テクスチャへディスパッチし、それをデノイズ
	* (AmbientOcclusionDenoiseCS.hlsl: 時間的リプロジェクション+ピンポン
	* 蓄積バッファとのブレンド)した上で、DeferredCompositePS.hlsl がサンプル
	* できるよう PIXEL_SHADER_RESOURCE 状態にしておく。今フレーム TLAS が無い
	* （追跡対象が無い）、または DXR PSO が無い場合は両パスともスキップし、
	* 蓄積バッファを 1.0（開放）でクリアする。ShadowRenderer と同じ構成。
	*/
	class AmbientOcclusionRenderer
	{
	public:
		AmbientOcclusionRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AmbientOcclusionRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer, decides this frame's
		///      history/write ping-pong slot, and registers every bindless
		///      index into IndicesSystem. Must run before IndicesSystem::
		///      UploadEditor/UploadGame bakes this frame's indices — i.e.
		///      before the G-Buffer even exists. No GPU work. (Same contract
		///      as ShadowRenderer::PrepareFrame.)
		/// [JP] チューニング用定数バッファを更新し、今フレームのピンポン
		///      history/write スロットを決め、bindless インデックスを
		///      IndicesSystem へ登録する。IndicesSystem::UploadEditor/UploadGame
		///      が今フレームのインデックスを確定する前 — つまり G-Buffer が
		///      存在するより前 — に呼ぶこと。GPU 処理は無い。
		///      (ShadowRenderer::PrepareFrame と同じ契約。)
		/// [EN] useDlssRayReconstruction: when true, registers the RAW
		///      single-buffered openness SRV as this view's "final" AO read
		///      index instead of the ping-ponged denoised one — DLSS Ray
		///      Reconstruction denoises the whole composited frame itself, so
		///      this renderer's own temporal accumulation is skipped entirely
		///      to avoid double-denoising (same scheme as
		///      GlobalIlluminationRenderer::PrepareFrame).
		/// [JP] useDlssRayReconstruction が true の間は、このビューの「最終的な」
		///      AO読み取りインデックスにピンポンデノイズ済みSRVではなく生の
		///      単一バッファ開放度SRVを登録する — DLSS Ray Reconstruction が
		///      合成フレーム全体を自身でデノイズするため、この Renderer 自身の
		///      時間的蓄積は二重デノイズを避けるため丸ごとスキップする
		///      (GlobalIlluminationRenderer::PrepareFrame と同じ方式)。
		void PrepareFrame(const AmbientOcclusionRayConstantBuffer& settings, Bool useDlssRayReconstruction);

		/// [EN] The actual GPU work: dispatches AmbientOcclusionRT.hlsl into
		///      the raw texture, then AmbientOcclusionDenoiseCS.hlsl into this
		///      frame's write slot (or clears it to 1.0 if tlasValid is false
		///      or the DXR PSO is missing), leaving the write slot in
		///      PIXEL_SHADER_RESOURCE state. Requires the G-Buffer
		///      depth/normal/velocity to already be written.
		/// [JP] 実際の GPU 処理: AmbientOcclusionRT.hlsl を raw テクスチャへ、
		///      続けて AmbientOcclusionDenoiseCS.hlsl を今フレームの write
		///      スロットへディスパッチする（tlasValid が false か DXR PSO が
		///      無ければ 1.0 でクリア）。write スロットは
		///      PIXEL_SHADER_RESOURCE 状態で終える。G-Buffer の
		///      深度/法線/速度が書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction);

	private:
		static constexpr Uint32 accumulationSlotCount = 2;
		static constexpr Uint32 viewCount = 2;

		AmbientOcclusionShader ambientOcclusionShader_;
		AmbientOcclusionDenoiseShader denoiseShader_;

		ResourcePtr<ConstantBuffer<AmbientOcclusionRayConstantBuffer>> tuningBuffer_;

		/// [EN] Raw noisy single-channel openness output of
		///      AmbientOcclusionRT.hlsl. Single-buffered — fully consumed by
		///      AmbientOcclusionDenoiseCS.hlsl the same frame it is written.
		/// [JP] AmbientOcclusionRT.hlsl の生ノイズ1チャンネル開放度出力。
		///      単一バッファ — 書かれた同じフレーム内で
		///      AmbientOcclusionDenoiseCS.hlsl に消費し切られる。
		Microsoft::WRL::ComPtr<ID3D12Resource> rawOpennessResource_;
		D3D12_RESOURCE_STATES rawOpennessState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 rawOpennessUnorderedAccessViewIndex_ = 0;
		Uint32 rawOpennessShaderResourceViewIndex_ = 0;

		/// [EN] Ping-ponged accumulated (denoised) openness, one independent
		///      pair per view (see RaytracingView) — same scheme as
		///      ShadowRenderer's accumulated visibility.
		/// [JP] ピンポン方式の蓄積(デノイズ済み)開放度。ビューごと
		///      (RaytracingView 参照)に独立した1ペア — ShadowRenderer の蓄積
		///      可視性と同じ方式。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedOpennessResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedOpennessState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Which slot holds the previous frame's finished result (this
		///      frame's history). Swapped once per frame at the top of
		///      PrepareFrame() — NOT in Dispatch(), which runs twice per frame
		///      (Editor + Game views). Same rule as ShadowRenderer.
		/// [JP] 前フレームの完成結果(今フレームの history)を持つスロット。
		///      交換は PrepareFrame() の冒頭で1フレームに1回だけ — Dispatch()
		///      では行わない(Editor/Game 両ビューで1フレームに2回走るため)。
		///      ShadowRenderer と同じルール。
		Uint32 historySlot_ = 0;

		/// [EN] Non-shader-visible UAV descriptors required by
		///      ClearUnorderedAccessViewFloat alongside the shader-visible
		///      ones (one for raw, one per accumulated view/slot).
		/// [JP] ClearUnorderedAccessViewFloat がシェーダ可視の UAV と併せて
		///      要求する、非シェーダ可視の UAV ディスクリプタ(raw に1つ、
		///      accumulated はビュー×スロットごとに1つ)。
		DescriptorHeap clearHeap_;
		Uint32 clearRawIndex_ = 0;
		Uint32 clearAccumulatedIndex_[viewCount][accumulationSlotCount] = {};

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		Uint32 frameIndex_ = 0;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
