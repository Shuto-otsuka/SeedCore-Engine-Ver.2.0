#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/Shadow/ShadowShader.h>
#include <GraphicsEngine/Raytracing/Shadow/ShadowDenoiseShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] 0 = own temporal (reprojected) accumulation, 1 = DLSS Ray
	///      Reconstruction (RaytracingRenderer drives this field from the
	///      single global RaytracingContext::dlssRayReconstructionEnabled_
	///      toggle before calling PrepareFrame — see
	///      RaytracingRenderer::Build). When DlssRR, ShadowRenderer skips its
	///      own ShadowDenoiseCS.hlsl dispatch entirely and exposes the raw
	///      traced visibility directly, since DLSS-RR denoises the whole
	///      composited frame itself (double-denoising would fight it).
	/// [JP] 0=自前の時間積分(リプロジェクションあり)、1=DLSS Ray
	///      Reconstruction(RaytracingRenderer が単一のグローバルトグル
	///      RaytracingContext::dlssRayReconstructionEnabled_ からこの
	///      フィールドを駆動してから PrepareFrame を呼ぶ —
	///      RaytracingRenderer::Build 参照)。DlssRR の間は
	///      ShadowDenoiseCS.hlsl 自体のディスパッチを丸ごと止め、生のトレース
	///      可視性をそのまま露出する — DLSS-RR が合成フレーム全体を自身で
	///      デノイズするため(二重デノイズは衝突する)。
	enum class ShadowDenoiseMode : Uint32
	{
		Temporal = 0,
		DlssRR = 1,
	};

	/// [EN] Mirrors Raytracing/Shadow/Shadow.hlsli's ShadowRayConstantBuffer —
	///      read by both ShadowRT.hlsl and DeferredLightingPS.hlsl via
	///      structured_indices.shadow_ray_constant_index_. Must stay
	///      byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/Shadow/Shadow.hlsli の ShadowRayConstantBuffer と対応。
	///      ShadowRT.hlsl と DeferredLightingPS.hlsl の両方が
	///      structured_indices.shadow_ray_constant_index_ 経由で読む。HLSL 側と
	///      バイト単位で一致させること。
	struct ShadowRayConstantBuffer
	{
		Float rayTMax_ = 1000.0f;
		Float normalBias_ = 0.01f;

		/// [EN] 0 = ignore traced visibility (always lit), 1 = apply it as-is.
		/// [JP] 0=シャドウレイの結果を無視(常に照射)、1=可視性をそのまま適用。
		Float shadowStrength_ = 1.0f;

		/// [EN] Directional light's disk half-angle (radians) — soft shadow
		///      cone size. 0 = hard shadow.
		/// [JP] ディレクショナルライトの半径角(ラジアン) — ソフトシャドウの
		///      コーンサイズ。0 なら硬い影。
		Float sunAngularRadius_ = 0.02f;

		/// [EN] World-space radius used to soften point/spot shadows.
		/// [JP] Point/Spot の影を柔らかくするワールド空間半径。
		Float punctualLightRadius_ = 0.1f;

		/// [EN] Incremented once per frame by ShadowRenderer (not the UI) —
		///      drives the per-pixel RNG seed so the stochastic ray direction
		///      changes every frame.
		/// [JP] ShadowRenderer が毎フレーム1つずつ加算する(UI からは触らない) —
		///      ピクセルごとの RNG シードを駆動し、確率的なレイ方向を毎フレーム
		///      変える。
		Uint32 frameIndex_ = 0;

		Uint32 denoiseMode_ = static_cast<Uint32>(ShadowDenoiseMode::Temporal);

		Float shadowRayPadding_ = 0.0f;

		template<class Archive>
		void serialize(Archive& archive)
		{
			archive(
				cereal::make_nvp("rayTMax", rayTMax_),
				cereal::make_nvp("normalBias", normalBias_),
				cereal::make_nvp("shadowStrength", shadowStrength_),
				cereal::make_nvp("sunAngularRadius", sunAngularRadius_),
				cereal::make_nvp("punctualLightRadius", punctualLightRadius_),
				cereal::make_nvp("denoiseMode", denoiseMode_));
		}
	};

	/**
	* [EN]
	* Dispatches the ray-traced shadow compute pass (ShadowRT.hlsl) into a raw
	* 2-channel (directional, punctual) noisy visibility texture, then denoises
	* it (ShadowDenoiseCS.hlsl: temporal reprojection + blend against a
	* ping-ponged accumulation buffer) and leaves the result in
	* PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl to sample. If the
	* scene has no TLAS this frame (nothing to trace against) or the DXR PSO is
	* unavailable, both stages are skipped and the accumulated buffer is
	* cleared to 1.0 (fully lit) instead.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレシャドウのコンピュートパス(ShadowRT.hlsl)を生の2チャンネル
	* (ディレクショナル/パンクチュアル)ノイズ可視性テクスチャへディスパッチし、
	* それをデノイズ(ShadowDenoiseCS.hlsl: 時間的リプロジェクション+ピンポン
	* 蓄積バッファとのブレンド)した上で、DeferredLightingPS.hlsl がサンプル
	* できるよう PIXEL_SHADER_RESOURCE 状態にしておく。今フレーム TLAS が無い
	* （追跡対象が無い）、または DXR PSO が無い場合は両パスともスキップし、
	* 蓄積バッファを 1.0（照射）でクリアする。
	*/
	class ShadowRenderer
	{
	public:
		ShadowRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ShadowRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer, decides this frame's
		///      history/write ping-pong slot, and registers every bindless
		///      index (raw uav/srv, history srv, write uav, and the final srv
		///      DeferredLightingPS.hlsl will sample) into IndicesSystem. Must
		///      run before IndicesSystem::UploadEditor/UploadGame bakes this
		///      frame's structured indices — i.e. before the G-Buffer even
		///      exists — unlike Dispatch(), which needs the G-Buffer's
		///      depth/normal/velocity and so runs later. No GPU work.
		/// [JP] チューニング用定数バッファを更新し、今フレームのピンポン
		///      history/write スロットを決め、すべての bindless インデックス
		///      (raw uav/srv、history srv、write uav、DeferredLightingPS.hlsl
		///      がサンプルする最終 srv)を IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームの
		///      structured indices を確定する前 — つまり G-Buffer が存在する
		///      より前 — に呼ぶこと。G-Buffer の深度/法線/速度を要する
		///      Dispatch() とは逆に、これは後で呼ぶ必要はない。GPU 処理は無い。
		void PrepareFrame(const ShadowRayConstantBuffer& settings);

		/// [EN] The actual GPU work: dispatches ShadowRT.hlsl into the raw
		///      texture, then (unless the last PrepareFrame() saw
		///      denoiseMode_ == DlssRR) ShadowDenoiseCS.hlsl into this frame's
		///      write slot (or clears both to 1.0 if tlasValid is false or the
		///      DXR PSO is missing), leaving the write slot in
		///      PIXEL_SHADER_RESOURCE state. When DlssRR, the denoise dispatch
		///      and the accumulation ping-pong are skipped entirely — only the
		///      raw texture is transitioned, since PrepareFrame() already
		///      pointed the composite shader at it directly. Requires the
		///      G-Buffer depth/normal/velocity to already be written.
		/// [JP] 実際の GPU 処理: ShadowRT.hlsl を raw テクスチャへ、続けて
		///      (直近の PrepareFrame() で denoiseMode_ == DlssRR でなければ)
		///      ShadowDenoiseCS.hlsl を今フレームの write スロットへ
		///      ディスパッチする（tlasValid が false か DXR PSO が無ければ
		///      両方とも 1.0 でクリア）。write スロットは
		///      PIXEL_SHADER_RESOURCE 状態で終える。DlssRR の間はデノイズ
		///      ディスパッチと蓄積ピンポンを丸ごとスキップする — 生テクスチャを
		///      遷移させるだけでよい(PrepareFrame() が既に合成シェーダの
		///      参照先をそこへ直接向けているため)。G-Buffer の深度/法線/速度が
		///      書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view);

	private:
		static constexpr Uint32 accumulationSlotCount = 2;
		static constexpr Uint32 viewCount = 2;

		ShadowShader shadowShader_;
		ShadowDenoiseShader denoiseShader_;

		ResourcePtr<ConstantBuffer<ShadowRayConstantBuffer>> tuningBuffer_;

		/// [EN] Raw noisy 2-channel (directional, punctual) output of
		///      ShadowRT.hlsl. Single-buffered — it is fully consumed by
		///      ShadowDenoiseCS.hlsl the same frame it is written.
		/// [JP] ShadowRT.hlsl の生ノイズ2チャンネル(ディレクショナル/
		///      パンクチュアル)出力。単一バッファ — 書かれた同じフレーム内で
		///      ShadowDenoiseCS.hlsl に消費し切られる。
		Microsoft::WRL::ComPtr<ID3D12Resource> rawVisibilityResource_;
		D3D12_RESOURCE_STATES rawVisibilityState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 rawVisibilityUnorderedAccessViewIndex_ = 0;
		Uint32 rawVisibilityShaderResourceViewIndex_ = 0;

		/// [EN] Ping-ponged accumulated (denoised) visibility, one independent
		///      pair per view (see RaytracingView). Each frame one slot is read
		///      as "history" (by ShadowDenoiseCS.hlsl) while the other is
		///      written as this frame's result, then read by
		///      DeferredLightingPS.hlsl; the roles swap next frame. Hand-
		///      rolled (not FrameRing) because these are barrier-transitioned
		///      in place, never reallocated.
		/// [JP] ピンポン方式の蓄積(デノイズ済み)可視性。ビューごと(RaytracingView
		///      参照)に独立した1ペア。毎フレーム片方を "history" として読み
		///      (ShadowDenoiseCS.hlsl)、もう片方を今フレームの結果として
		///      書き込み、それを DeferredLightingPS.hlsl が読む。役割は次
		///      フレームで入れ替わる。リソースは再確保せずバリアで状態遷移
		///      するだけなので、FrameRing ではなく手動で管理する。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedVisibilityResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedVisibilityState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Which slot holds the previous frame's finished result (this
		///      frame's history). The other slot is this frame's write
		///      target. Swapped once per frame at the top of PrepareFrame() —
		///      NOT in Dispatch(), which runs twice per frame (Editor + Game
		///      views) and must see the same slot assignment both times.
		/// [JP] 前フレームの完成結果(今フレームの history)を持つスロット。
		///      もう片方が今フレームの書き込み先。交換は PrepareFrame() の
		///      冒頭で1フレームに1回だけ — Dispatch() では行わない。Dispatch()
		///      は Editor/Game 両ビューで1フレームに2回走り、どちらも同じ
		///      スロット割り当てを見る必要があるため。
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

		/// [EN] Cached from the last PrepareFrame() call's denoiseMode_ so
		///      Dispatch() (which does not receive settings) knows whether to
		///      run its own denoise pass or bypass it for DLSS-RR.
		/// [JP] 直近の PrepareFrame() 呼び出しの denoiseMode_ をキャッシュ。
		///      Dispatch()(settings を受け取らない)が自前デノイズを実行するか
		///      DLSS-RR 用にバイパスするか判断するために使う。
		Bool dlssRayReconstructionActive_ = false;
	};
}
