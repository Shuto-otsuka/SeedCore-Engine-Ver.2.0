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
		void Serialize(Archive& archive)
		{
			archive.TryField("rayTMax", rayTMax_);
			archive.TryField("normalBias", normalBias_);
			archive.TryField("shadowStrength", shadowStrength_);
			archive.TryField("sunAngularRadius", sunAngularRadius_);
			archive.TryField("punctualLightRadius", punctualLightRadius_);
			archive.TryField("denoiseMode", denoiseMode_);
		}
	};

	/**
	* [EN]
	* Dispatches the ray-traced shadow compute pass (ShadowRT.hlsl) into a raw
	* 2-channel (directional, punctual) noisy visibility texture, then runs the
	* 5-pass SVGF chain over it (ShadowDenoiseCS.hlsl: temporal reprojection with
	* moment accumulation -> spatial variance estimate for short history -> three
	* variance-guided A-Trous wavelet iterations) and leaves the result in
	* PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl to sample. If the
	* scene has no TLAS this frame (nothing to trace against) or the DXR PSO is
	* unavailable, both stages are skipped and the denoised buffer is cleared to
	* 1.0 (fully lit) instead.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレシャドウのコンピュートパス(ShadowRT.hlsl)を生の2チャンネル
	* (ディレクショナル/パンクチュアル)ノイズ可視性テクスチャへディスパッチし、
	* それに対して5パスの SVGF チェーンを回した(ShadowDenoiseCS.hlsl: モーメント
	* 蓄積つき時間的リプロジェクション → 履歴が短いピクセル向けの空間的分散推定
	* → 分散誘導 A-Trous ウェーブレット3反復)上で、DeferredLightingPS.hlsl が
	* サンプルできるよう PIXEL_SHADER_RESOURCE 状態にしておく。今フレーム TLAS が
	* 無い（追跡対象が無い）、または DXR PSO が無い場合は両段ともスキップし、
	* denoised バッファを 1.0（照射）でクリアする。
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
		///      denoiseMode_ == DlssRR) the SVGF chain — reproject into
		///      scratch0, FilterMoments into scratch1, A-Trous step 1 back into
		///      scratch0, A-Trous step 2 into this frame's history write slot
		///      (the feedback tap), A-Trous step 4 into the per-view denoised
		///      output, which is left in PIXEL_SHADER_RESOURCE state. If
		///      tlasValid is false or the DXR PSO is missing, the denoised
		///      output is cleared to 1.0 and the history length to 0 instead.
		///      When DlssRR, the whole chain is skipped — only the raw texture
		///      is transitioned, since PrepareFrame() already pointed the
		///      composite shader at it directly. Requires the G-Buffer
		///      depth/normal/velocity to already be written.
		/// [JP] 実際の GPU 処理: ShadowRT.hlsl を raw テクスチャへ、続けて
		///      (直近の PrepareFrame() で denoiseMode_ == DlssRR でなければ)
		///      SVGF チェーン — リプロジェクションを scratch0 へ、FilterMoments を
		///      scratch1 へ、A-Trous step1 を scratch0 へ戻し、A-Trous step2 を
		///      今フレームの history write スロット(フィードバックタップ)へ、
		///      A-Trous step4 をビューごとの denoised 出力へ書き、それを
		///      PIXEL_SHADER_RESOURCE 状態で終える。tlasValid が false か
		///      DXR PSO が無ければ、代わりに denoised 出力を 1.0、履歴長を 0 で
		///      クリアする。DlssRR の間はチェーンを丸ごとスキップする — 生
		///      テクスチャを遷移させるだけでよい(PrepareFrame() が既に合成
		///      シェーダの参照先をそこへ直接向けているため)。G-Buffer の
		///      深度/法線/速度が書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view);

	private:
		/// [EN] Allocates the raw texture and every per-view buffer of the SVGF
		///      chain. Shared by Create() and Resize() so the two can never
		///      drift apart as the chain gains or loses a buffer.
		/// [JP] raw テクスチャと、ビューごとの SVGF チェーン全バッファを確保する。
		///      Create() と Resize() で共有し、チェーンにバッファが増減しても
		///      両者がずれないようにする。
		void CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

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

		/// [EN] SVGF's temporal history: rg = filtered visibility, ba = its
		///      variance. Ping-ponged, one independent pair per view (see
		///      RaytracingView). This is the FEEDBACK TAP, not the final image —
		///      ATrousPass2 writes it and next frame's reproject reads it, while
		///      the image DeferredLightingPS.hlsl samples comes from
		///      denoisedResource_ below. Hand-rolled (not FrameRing) because
		///      these are barrier-transitioned in place, never reallocated.
		/// [JP] SVGF の時間的履歴。rg = フィルタ済み可視性、ba = その分散。
		///      ピンポン方式で、ビューごと(RaytracingView 参照)に独立した1ペア。
		///      これは【フィードバックタップ】であって最終画ではない —
		///      ATrousPass2 が書き、次フレームのリプロジェクションが読む。
		///      DeferredLightingPS.hlsl がサンプルする画は下の denoisedResource_。
		///      リソースは再確保せずバリアで状態遷移するだけなので、FrameRing
		///      ではなく手動で管理する。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedVisibilityResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedVisibilityState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Per-pixel first/second luminance moments of the two visibility
		///      channels, packed as (1st.x, 2nd.x, 1st.y, 2nd.y). Ping-ponged
		///      alongside the history above — SVGF derives its variance from the
		///      temporally accumulated moments, so they have to survive the frame
		///      exactly like the illumination does.
		/// [JP] 2チャンネル可視性それぞれの1次/2次輝度モーメント。
		///      (1次.x, 2次.x, 1次.y, 2次.y) の順で詰める。上の履歴と同じく
		///      ピンポンする — SVGF は時間蓄積したモーメントから分散を求めるので、
		///      輝度と全く同様にフレームをまたいで保持する必要がある。
		Microsoft::WRL::ComPtr<ID3D12Resource> momentsResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES momentsState_[viewCount][accumulationSlotCount] = {};
		Uint32 momentsUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 momentsShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Per-pixel count of successfully reprojected frames. Drives the
		///      max(alpha, 1/length) blend factor and the switch to the spatial
		///      variance estimate. Ping-ponged.
		/// [JP] ピクセルごとのリプロジェクション成功フレーム数。
		///      max(alpha, 1/履歴長) のブレンド係数と、空間的分散推定への
		///      切り替え判定を駆動する。ピンポンする。
		Microsoft::WRL::ComPtr<ID3D12Resource> historyLengthResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES historyLengthState_[viewCount][accumulationSlotCount] = {};
		Uint32 historyLengthUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 historyLengthShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Packed (view depth, depth derivative, oct normal) copy of this
		///      frame's surface. Ping-ponged because SVGF's temporal consistency
		///      test compares against the PREVIOUS frame's version, and the
		///      engine's G-Buffer is single-buffered so it cannot be read back.
		/// [JP] 今フレームの面を (ビュー深度, 深度勾配, oct法線) で詰めたコピー。
		///      SVGF の時間的整合性テストが【前フレーム】の値と比較するため
		///      ピンポンする — エンジンの G-Buffer は単一バッファで、前フレームを
		///      読み戻せないため。
		Microsoft::WRL::ComPtr<ID3D12Resource> depthNormalResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES depthNormalState_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Fully filtered 2-channel visibility DeferredLightingPS.hlsl
		///      samples — the output of the last A-Trous iteration. Single
		///      buffered per view: it is consumed the same frame it is written
		///      and never feeds back, which is exactly what lets the history
		///      above stop at the earlier, sharper feedback tap.
		/// [JP] DeferredLightingPS.hlsl がサンプルする、完全にフィルタ済みの
		///      2チャンネル可視性 — 最後の A-Trous 反復の出力。ビューごとに単一
		///      バッファ: 書かれた同じフレームで消費されフィードバックしない。
		///      これがあるからこそ、上の履歴を「より早く、よりシャープな」
		///      フィードバックタップで止められる。
		Microsoft::WRL::ComPtr<ID3D12Resource> denoisedResource_[viewCount];
		D3D12_RESOURCE_STATES denoisedState_[viewCount] = {};
		Uint32 denoisedUnorderedAccessViewIndex_[viewCount] = {};
		Uint32 denoisedShaderResourceViewIndex_[viewCount] = {};

		/// [EN] A-Trous ping-pong scratch, one pair per view. Pure scratch -
		///      always fully overwritten by the pass that writes it.
		/// [JP] A-Trous ピンポンスクラッチ、ビューごとに1ペア。純粋なスクラッチ
		///      で、書き込むパスが必ず全画素を上書きする。
		Microsoft::WRL::ComPtr<ID3D12Resource> atrousScratchResource_[viewCount][2];
		D3D12_RESOURCE_STATES atrousScratchState_[viewCount][2] = {};
		Uint32 atrousScratchUnorderedAccessViewIndex_[viewCount][2] = {};
		Uint32 atrousScratchShaderResourceViewIndex_[viewCount][2] = {};

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
		///      ones. Only the surfaces actually cleared on the "nothing to
		///      trace" path need one: the raw texture, the per-view denoised
		///      output, and the history length (cleared to 0 so the filter
		///      re-converges from scratch rather than trusting a stale history).
		/// [JP] ClearUnorderedAccessViewFloat がシェーダ可視の UAV と併せて
		///      要求する、非シェーダ可視の UAV ディスクリプタ。「追跡対象なし」
		///      経路で実際にクリアする面だけが必要 — raw、ビューごとの
		///      denoised 出力、そして履歴長(0 でクリアし、古い履歴を信用せず
		///      ゼロから収束し直させる)。
		DescriptorHeap clearHeap_;
		Uint32 clearRawIndex_ = 0;
		Uint32 clearDenoisedIndex_[viewCount] = {};
		Uint32 clearHistoryLengthIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 clearAccumulatedIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 clearMomentsIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 clearDepthNormalIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Whether the history chain has been zeroed since it was created.
		///      D3D12 does not guarantee a freshly created committed resource
		///      reads as zero, and every buffer here feeds back into itself the
		///      next frame - an uninitialized texel latches in permanently
		///      rather than clearing after a frame.
		/// [JP] 生成以降に履歴チェーンを 0 で埋めたかどうか。D3D12 は生成直後の
		///      committed リソースが 0 で読める保証をせず、ここのバッファは全て
		///      翌フレーム自分自身へ戻る — 未初期化テクセルは 1 フレームで消えず
		///      恒久的に焼き付く。
		Bool historyCleared_ = false;

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
