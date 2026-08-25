#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/PostProcess/PostEffect/AutoExposure.h>
#include <GraphicsEngine/PostProcess/PostEffect/ToneMapping.h>
#include <GraphicsEngine/PostProcess/PostEffect/KawaseBloom.h>
#include <GraphicsEngine/PostProcess/PostEffect/AnamorphicFlare.h>
#include <GraphicsEngine/PostProcess/PostEffect/LensFlare.h>
#include <GraphicsEngine/PostProcess/PostEffect/LensDistortion.h>
#include <GraphicsEngine/PostProcess/PostEffect/ChromaticAberration.h>
#include <GraphicsEngine/PostProcess/PostEffect/Vignette.h>
#include <GraphicsEngine/PostProcess/PostEffect/FilmGrain.h>
#include <GraphicsEngine/PostProcess/PostEffect/ColorGrading.h>
#include <GraphicsEngine/PostProcess/PostEffect/DepthOfField.h>
#include <GraphicsEngine/PostProcess/PostEffect/Bokeh.h>
#include <GraphicsEngine/PostProcess/PostEffect/Sharpness.h>
#include <GraphicsEngine/PostProcess/PostProcess.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;
	class World;

	/**
	* [EN]
	* Owns the per-view GPU resources for the post-process display chain
	* (auto-exposure histogram + smoothed exposure + the tone-mapped display
	* texture) and runs it: Editor and Game each get their own complete set,
	* because the auto-exposure state is genuinely per-camera persistent state
	* (see PostProcessIndices in Shader/Constants.hlsli) - sharing one set
	* between two cameras looking at different scenes would make each view's
	* exposure adaptation fight the other's every frame.
	*
	* This pass is Renderer's hook into ECS the same way every other
	* Renderer-owned subsystem is (ModelRenderer, LightSystem, SkyRenderer,
	* ...): it owns its own Gather(World&), not Renderer performing the query
	* and handing this class a settings value. Gather() collects every
	* PostProcess-carrying entity into volumes_ - this engine is meant to
	* support multiple simultaneous volumes eventually (priority/spatial
	* blending, à la Unreal/Unity PostProcessVolume), so the full list is kept
	* rather than collapsed to one at gather time. ResolveSettings() is the one
	* place that currently collapses it to a single active result (a
	* placeholder today - first entry, or defaults if empty); replacing it
	* with real priority/spatial resolution later only touches that function.
	*
	* Unlike the RT effect renderers (Reflection, GlobalIllumination, ...)
	* this pass has no "disabled -> clear to 0" branch: it is not a sparse
	* additive contribution, it is the unconditional full-screen pass that
	* produces the ONLY thing ever shown in the Editor/Game viewport (see
	* ToneMappingCS.hlsl's doc comment for why the sRGB encode step cannot be
	* skipped even when tone mapping/auto exposure are toggled off).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ポストプロセス表示チェーン(自動露出ヒストグラム+平滑化露出+トーンマップ
	* 済み表示テクスチャ)のビューごとの GPU リソースを保持し、実行する。
	* Editor と Game はそれぞれ完全に独立した一式を持つ — 自動露出の状態は
	* 正真正銘カメラごとの永続状態なので(Shader/Constants.hlsli の
	* PostProcessIndices 参照)、別シーンを見ている2つのカメラで1組を共有すると
	* 毎フレーム互いの露出順応を潰し合う。
	*
	* このパスは、他の Renderer 所有サブシステム(ModelRenderer、LightSystem、
	* SkyRenderer、...)と同じ形で ECS へつながる: Renderer がクエリを行って
	* 設定値を渡すのではなく、このクラス自身が Gather(World&) を持つ。
	* Gather() は PostProcess を持つ全エンティティを volumes_ へ集める —
	* このエンジンは将来複数ボリュームの同時使用(Unreal/Unity の
	* PostProcessVolume のような優先度/空間ブレンド)をサポートする前提なので、
	* Gather の時点で1つに潰さずリスト全体を保持する。ResolveSettings() が
	* 現状このリストを単一の有効な結果へ潰す場所(今は暫定実装 — 先頭要素、
	* 無ければ既定値)で、後で本物の優先度/空間解決に差し替えるときもこの
	* 関数だけを触ればよい。
	*
	* RT エフェクト系レンダラー(Reflection、GlobalIllumination、...)と違い
	* 「無効時は0でクリア」の分岐が無い: これは疎な加算コントリビューションでは
	* なく、Editor/Game ビューポートが実際に表示する【唯一のもの】を作る無条件
	* フルスクリーンパスだから(トーンマップ/自動露出をオフにしても sRGB
	* エンコード段が省略できない理由は ToneMappingCS.hlsl のコメント参照)。
	*/
	class PostProcessRenderer
	{
	public:
		PostProcessRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~PostProcessRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		/// [EN] Collects every PostProcess-carrying entity into volumes_ (see
		///      the class doc comment). Same shape as LightSystem::Gather /
		///      ModelRenderer::Gather - called once per frame from
		///      Renderer::Gather, shared by both Editor and Game flushes.
		/// [JP] PostProcess を持つ全エンティティを volumes_ へ集める(クラス
		///      先頭のコメント参照)。LightSystem::Gather / ModelRenderer::Gather
		///      と同じ形 — Renderer::Gather からフレームに1回呼ばれ、
		///      Editor/Game 両方の Flush が共有する。
		void Gather(World& world);

		/// [EN] Writes this view's PostProcess settings (resolved via
		///      ResolveSettings()) into IndicesSystem (resource indices +
		///      tuning scalars). No GPU work. Must run before IndicesSystem::
		///      UploadEditor/UploadGame bakes that view's constant buffer for
		///      the frame. sourceColorIndex is the HDR FrameBuffer's bindless
		///      SRV index (FrameBuffer::ColorShaderResourceViewIndex()) —
		///      stable for the FrameBuffer's lifetime, so it is fine to bake
		///      here rather than re-deriving it in Dispatch().
		/// [JP] このビューの PostProcess 設定(ResolveSettings() で解決)を
		///      IndicesSystem へ書き込む(リソースインデックス+チューニング用
		///      スカラー)。GPU 処理は無い。IndicesSystem::UploadEditor/
		///      UploadGame がそのビューの今フレーム分の定数バッファを確定する
		///      前に呼ぶこと。sourceColorIndex は HDR FrameBuffer の bindless
		///      SRV インデックス(FrameBuffer::ColorShaderResourceViewIndex())
		///      — FrameBuffer の生存期間中不変なので、Dispatch() 側で改めて
		///      求め直さずここで確定してよい。
		/// [EN] useUpscaledOutput selects which output chain this view's
		///      IndicesSystem registration and Dispatch() target: false = the
		///      native 1280x720 SD chain (default, per-effect custom
		///      denoisers), true = the 3840x2160 UHD chain fed by
		///      DlssRayReconstructionRenderer's output. Histogram/exposure
		///      state is shared between both (resolution-independent).
		/// [JP] useUpscaledOutput はこのビューの IndicesSystem 登録と Dispatch()
		///      が対象にする出力チェーンを選ぶ: false = ネイティブ1280x720の
		///      SDチェーン(既定、エフェクトごとの自前デノイザ経路)、
		///      true = DlssRayReconstructionRenderer の出力を受ける
		///      3840x2160のUHDチェーン。ヒストグラム/露出状態は両方で共有
		///      (解像度非依存のため)。
		void PrepareView(IndicesSystem& indicesSystem, RaytracingView view, Uint32 sourceColorIndex, Bool useUpscaledOutput);

		/// [EN] The GPU work for one view: clears + builds + reduces the
		///      histogram (skipped entirely when ExposureSettings.enabled_ is
		///      off — the tone-map pass then just uses compensation_ alone),
		///      then always runs the tone-map/display pass. sourceColorResource
		///      must be in PIXEL_SHADER_RESOURCE state on entry (this function
		///      barriers it to NON_PIXEL_SHADER_RESOURCE for the compute reads
		///      and barriers it back before returning, so the caller's own
		///      FrameBuffer state bookkeeping stays valid).
		/// [JP] 1ビューぶんの GPU 処理: ヒストグラムのクリア+構築+縮約
		///      (ExposureSettings.enabled_ が無効なら丸ごとスキップ — その場合
		///      トーンマップは compensation_ だけを使う)、その後トーンマップ/
		///      表示パスは常に実行する。sourceColorResource は呼び出し時点で
		///      PIXEL_SHADER_RESOURCE 状態であること(この関数がコンピュート
		///      読み取り用に NON_PIXEL_SHADER_RESOURCE へバリアし、戻る前に
		///      元へ戻すので、呼び出し側の FrameBuffer 側の状態管理は壊れない)。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view, ID3D12Resource* sourceColorResource, Bool useUpscaledOutput);

		/// [EN] The tone-mapped, sRGB-encoded, UNORM display texture for that
		///      view — what Renderer::RegisterImGuiShaderResourceViews should
		///      point the editor/game ImGui SRV at instead of the raw HDR
		///      FrameBuffer. Returns whichever chain (SD/UHD) that view's most
		///      recent PrepareView() selected.
		/// [JP] そのビューのトーンマップ済み・sRGB エンコード済み UNORM 表示
		///      テクスチャ。Renderer::RegisterImGuiShaderResourceViews が
		///      editor/game の ImGui SRV を、生の HDR FrameBuffer ではなく
		///      こちらへ向けるべき対象。そのビューの直近の PrepareView() が
		///      選んだ方のチェーン(SD/UHD)を返す。
		[[nodiscard]] ID3D12Resource* OutputResource(RaytracingView view)const;

		/// [EN] outputWidth_/outputHeight_ (the DLSS-RR-upscaled chain's resolution), unconditionally - not gated by whether that chain is the currently active one. Lets a caller that runs before this frame's PrepareView() (e.g. Graphics::EditorRender building the scene constant buffer) know what resolution the debug overlay will end up drawing at later this same frame if DLSS-RR is active.
		/// [JP] outputWidth_/outputHeight_(DLSS-RRアップスケール後チェーンの解像度)を無条件に返す - そのチェーンが現在アクティブかどうかには関係しない。この関数を、今フレームのPrepareView()より前に実行される呼び出し側(例えばシーン定数バッファを組み立てるGraphics::EditorRender)が、DLSS-RR有効時にデバッグオーバーレイが今フレーム後で実際に描画することになる解像度を知るために使う。
		[[nodiscard]] Vector2 OutputSize()const;

		/// [EN] RTV onto whichever chain (SD/UHD) that view's most recent
		///      PrepareView() selected - same resource OutputResource()
		///      returns. Valid to bind only while that resource is in
		///      RENDER_TARGET state (see BeginDebugOverlay/EndDebugOverlay).
		/// [JP] そのビューの直近の PrepareView() が選んだ方のチェーン(SD/UHD)
		///      へのRTV — OutputResource() が返すのと同じリソース。
		///      そのリソースが RENDER_TARGET 状態の間のみバインド可能
		///      (BeginDebugOverlay/EndDebugOverlay 参照)。
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE OutputRenderTargetViewHandle(RaytracingView view)const;

		/// [EN] Viewport matching whichever chain (SD/UHD) that view's most
		///      recent PrepareView() selected - width_/height_ for the native
		///      chain, outputWidth_/outputHeight_ for the DLSS-RR-upscaled one.
		/// [JP] そのビューの直近の PrepareView() が選んだ方のチェーン(SD/UHD)に
		///      合わせたビューポート - ネイティブチェーンなら width_/height_、
		///      DLSS-RRアップスケール版なら outputWidth_/outputHeight_。
		[[nodiscard]] D3D12_VIEWPORT Viewport(RaytracingView view)const;

		/// [EN] Transitions view's active output chain from Dispatch()'s exit
		///      state (PIXEL_SHADER_RESOURCE) to RENDER_TARGET, so a
		///      post-tonemap debug overlay (collider wireframes, selection
		///      outline, ...) can draw directly onto the final display
		///      texture without being affected by tone mapping/exposure -
		///      call after Dispatch(), pair with EndDebugOverlay().
		/// [JP] viewのアクティブな出力チェーンを、Dispatch()が抜ける時点の
		///      状態(PIXEL_SHADER_RESOURCE)からRENDER_TARGETへ遷移する。
		///      これにより、トーンマップ後のデバッグオーバーレイ
		///      (コライダーワイヤーフレーム、選択アウトラインなど)が
		///      トーンマップ/露出の影響を受けずに最終表示テクスチャへ
		///      直接描画できる - Dispatch()の後に呼び、EndDebugOverlay()と
		///      対にすること。
		void BeginDebugOverlay(D3D12CommandList* cmdList, RaytracingView view);

		/// [EN] Transitions view's active output chain from RENDER_TARGET back
		///      to PIXEL_SHADER_RESOURCE, so RefreshImGui can read it.
		/// [JP] viewのアクティブな出力チェーンをRENDER_TARGETから
		///      PIXEL_SHADER_RESOURCEへ戻す。RefreshImGuiが
		///      読み取れるようにするため。
		void EndDebugOverlay(D3D12CommandList* cmdList, RaytracingView view);

	private:
		struct View
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> outputResource_;
			D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 outputUnorderedAccessViewIndex_ = 0;
			Uint32 outputRenderTargetViewIndex_ = 0;

			Uint32 outputShaderResourceViewIndex_ = 0;

			/// [EN] UHD (3840x2160) counterpart of outputResource_ above, used
			///      only when DLSS Ray Reconstruction is active — fed by
			///      DlssRayReconstructionRenderer's upscaled output instead of
			///      the native-resolution FrameBuffer. Always allocated (same
			///      "allocate unconditionally, branch only at Dispatch" pattern
			///      as every other Renderer here) so toggling DLSS-RR on/off
			///      needs no resource (re)creation.
			/// [JP] 上の outputResource_ の UHD(3840x2160)版。DLSS Ray
			///      Reconstruction が有効な時だけ使う — ネイティブ解像度の
			///      FrameBuffer ではなく DlssRayReconstructionRenderer の
			///      アップスケール出力を受ける。常に確保しておく(他の全
			///      Renderer と同じ「無条件確保、Dispatch側だけで分岐」方式)ので、
			///      DLSS-RRのオン/オフ切り替えにリソース再生成は不要。
			Microsoft::WRL::ComPtr<ID3D12Resource> outputResourceUpscaled_;
			D3D12_RESOURCE_STATES outputStateUpscaled_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 outputUnorderedAccessViewIndexUpscaled_ = 0;
			Uint32 outputRenderTargetViewIndexUpscaled_ = 0;

			Uint32 outputShaderResourceViewIndexUpscaled_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> sharpenResource_;
			D3D12_RESOURCE_STATES sharpenState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 sharpenUnorderedAccessViewIndex_ = 0;
			Uint32 sharpenRenderTargetViewIndex_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> sharpenResourceUpscaled_;
			D3D12_RESOURCE_STATES sharpenStateUpscaled_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 sharpenUnorderedAccessViewIndexUpscaled_ = 0;
			Uint32 sharpenRenderTargetViewIndexUpscaled_ = 0;

			/// [EN] Which chain the most recent PrepareView() selected - what
			///      OutputResource() reports back to Renderer for ImGui.
			/// [JP] 直近の PrepareView() が選んだチェーン — OutputResource() が
			///      Renderer(ImGui用)へ返す先。
			Bool activeIsUpscaled_ = false;

			/// [EN] 256-bin uint histogram. Stays in UNORDERED_ACCESS for its
			///      entire lifetime (created directly in that state) - nothing
			///      outside this class's own compute dispatches ever touches
			///      it, so there is no state-transition barrier to track, only
			///      UAV hazard barriers between dispatches.
			/// [JP] 256 ビンの uint ヒストグラム。生存期間中ずっと
			///      UNORDERED_ACCESS のまま(その状態で直接生成)。このクラス
			///      自身のコンピュートディスパッチ以外は一切触らないので、
			///      状態遷移バリアの追跡は不要 — ディスパッチ間の UAV
			///      ハザードバリアだけで足りる。
			Microsoft::WRL::ComPtr<ID3D12Resource> histogramResource_;
			Uint32 histogramUnorderedAccessViewIndex_ = 0;

			/// [EN] Non-shader-visible RAW(R32_TYPELESS) view of the histogram,
			///      required by ClearUnorderedAccessViewUint alongside the
			///      shader-visible one below (a structured UAV cannot be
			///      cleared directly - see LightSystem's cluster buffer for the
			///      same trick).
			/// [JP] ヒストグラムの非シェーダ可視 RAW(R32_TYPELESS)ビュー。下の
			///      シェーダ可視ビューと併せて ClearUnorderedAccessViewUint が
			///      要求する(構造化 UAV は直接クリアできない — 同じ手法を
			///      LightSystem のクラスタバッファでも使っている)。
			Uint32 histogramClearUnorderedAccessViewIndex_ = 0;

			/// [EN] Shader-visible RAW(R32_TYPELESS) view of the histogram, used
			///      only as ClearUnorderedAccessViewUint's GPU handle.
			///      histogramUnorderedAccessViewIndex_ above is a STRUCTURED
			///      view (what HistogramCS actually binds), so it cannot be
			///      paired with the RAW CPU handle above - the clear API
			///      requires the GPU and CPU descriptors to be identical views,
			///      not just the same underlying resource. This is a second,
			///      dedicated bindless RAW view (LightSystem's
			///      clusterDataClearUnorderedAccessViewIndex_ pattern).
			/// [JP] ヒストグラムのシェーダ可視 RAW(R32_TYPELESS)ビュー。
			///      ClearUnorderedAccessViewUint の GPU ハンドル専用。上の
			///      histogramUnorderedAccessViewIndex_ は構造化ビュー
			///      (HistogramCS が実際にバインドするもの)なので、上の RAW な
			///      CPU ハンドルとは組み合わせられない — clear API は GPU/CPU
			///      記述子が同一リソースというだけでなく同一ビューであることを
			///      要求する。LightSystem の
			///      clusterDataClearUnorderedAccessViewIndex_ と同じ、専用の
			///      bindless RAW ビュー。
			Uint32 histogramClearGpuUnorderedAccessViewIndex_ = 0;

			/// [EN] Single persistent float: the smoothed exposure EV, carried
			///      across frames by AutoExposureAverageCS.hlsl's read-modify-
			///      write. Zeroed once via the same RAW-view clear trick on this
			///      view's first Dispatch(); never cleared again.
			/// [JP] 単一の永続 float: 平滑化露出 EV。
			///      AutoExposureAverageCS.hlsl の read-modify-write でフレームを
			///      跨いで保持される。このビューの最初の Dispatch() で同じ
			///      RAW ビュークリアで一度だけゼロ初期化し、以後は二度と
			///      クリアしない。
			Microsoft::WRL::ComPtr<ID3D12Resource> exposureResource_;
			Uint32 exposureUnorderedAccessViewIndex_ = 0;
			Uint32 exposureClearUnorderedAccessViewIndex_ = 0;
			Uint32 exposureClearGpuUnorderedAccessViewIndex_ = 0;

			Bool exposureInitialized_ = false;

			/// [EN] LensFlareCS.hlsl's additive output, at a quarter of the
			///      native resolution (width_/height_, not outputWidth_/
			///      outputHeight_ - the flare reads the native HDR source, not
			///      the DLSS-RR-upscaled one). ToneMappingCS.hlsl samples it
			///      (bindless SRV) and adds it into the HDR color before
			///      exposure. Always allocated; PostProcess::LensFlareSettings.
			///      enabled_ just gates whether Dispatch() writes into it and
			///      whether ToneMappingCS.hlsl reads it (same "allocate
			///      unconditionally, branch only at Dispatch" pattern as every
			///      other Renderer here).
			/// [JP] LensFlareCS.hlsl の加算出力。ネイティブ解像度(width_/
			///      height_、DLSS-RRアップスケール後のoutputWidth_/
			///      outputHeight_ではない — フレアはネイティブHDRソースを読む)の
			///      1/4。ToneMappingCS.hlsl がこれを(bindless SRV経由で)サンプルし
			///      露出適用前のHDRカラーへ加算する。常に確保しておく;
			///      PostProcess::LensFlareSettings.enabled_ は Dispatch() が
			///      書き込むか、ToneMappingCS.hlsl が読むかを切り替えるだけ
			///      (他の全 Renderer と同じ「無条件確保、Dispatch側だけで分岐」
			///      方式)。
			Microsoft::WRL::ComPtr<ID3D12Resource> lensFlareResource_;
			D3D12_RESOURCE_STATES lensFlareState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 lensFlareUnorderedAccessViewIndex_ = 0;
			Uint32 lensFlareShaderResourceViewIndex_ = 0;

			/// [EN] LensFlareCS.hlsl's per-axis ping-pong buffers
			///      (lensFlareMaxAxisCount axes x
			///      [ping, pong]), same resolution as lensFlareResource_ above.
			///      BlurPass1..4 ping-pong within a given axis's pair; Compose
			///      reads each axis's final (pong) buffer and sums into
			///      lensFlareResource_. Bindless indices are registered once
			///      per frame in PrepareView alongside lensFlareResource_'s -
			///      the ping/pong role for a given pass is fixed at HLSL
			///      compile time (see LensFlareCS.hlsl), not re-registered
			///      mid-frame.
			/// [JP] LensFlareCS.hlsl の軸ごとのピンポンバッファ
			///      (lensFlareMaxAxisCount 軸 x
			///      [ping, pong])、上の lensFlareResource_ と同じ解像度。
			///      BlurPass1..4 が軸ごとのペア内でピンポンし、Compose が各軸の
			///      最終(pong)バッファを読んで lensFlareResource_ へ合算する。
			///      bindless インデックスは lensFlareResource_ と同じく
			///      フレームに1回 PrepareView で登録する - あるパスでの
			///      ping/pong の役割は HLSL コンパイル時に固定なので
			///      (LensFlareCS.hlsl 参照)、フレーム中の再登録はしない。
			Microsoft::WRL::ComPtr<ID3D12Resource> lensFlareStreakResource_[lensFlareMaxAxisCount][2];
			D3D12_RESOURCE_STATES lensFlareStreakState_[lensFlareMaxAxisCount][2] = {};
			Uint32 lensFlareStreakUnorderedAccessViewIndex_[lensFlareMaxAxisCount][2] = {};
			Uint32 lensFlareStreakShaderResourceViewIndex_[lensFlareMaxAxisCount][2] = {};

			/// [EN] LensFlareCS.hlsl's Downsample output: a quarter-res,
			///      bright-passed copy of the scene that both BlurPass1
			///      (streaks) and Ghost (ghost chain + halo) read instead of
			///      the full-res HDR source. Its 4-tap filter covers the whole
			///      4x4 source footprint, so small bright sources survive the
			///      downscale - point-sampling mip 0 at quarter density skips
			///      15 of every 16 pixels and loses the sun disc entirely.
			/// [JP] LensFlareCS.hlsl の Downsample の出力: 1/4解像度の
			///      ブライトパス済みシーンコピーで、BlurPass1(ストリーク)も
			///      Ghost(ゴーストチェーン+ハロー)もフル解像度のHDRソース
			///      ではなくこちらを読む。4タップのフィルタが4x4のソース範囲を
			///      丸ごとカバーするため、小さく明るい光源が縮小で消えない —
			///      1/4密度で mip 0 をポイントサンプルすると16画素中15画素を
			///      読み飛ばし、太陽の円盤が丸ごと失われる。
			Microsoft::WRL::ComPtr<ID3D12Resource> lensFlareBrightResource_;
			D3D12_RESOURCE_STATES lensFlareBrightState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 lensFlareBrightUnorderedAccessViewIndex_ = 0;
			Uint32 lensFlareBrightShaderResourceViewIndex_ = 0;

			/// [EN] KawaseBloomCS.hlsl's 6-level chain, level 0 at half the native
			///      resolution and each subsequent level half of the one
			///      before. Separate committed resources per level rather than
			///      one mipped texture, because UnorderedAccessBarrier and the
			///      cmdList->Barrier helper both transition a whole resource -
			///      the chain needs level N readable while level N+1 is being
			///      written, which would need per-subresource transitions.
			///      Level 0 holds the final result after the upsample chain
			///      finishes; that is what ToneMappingCS.hlsl samples.
			/// [JP] KawaseBloomCS.hlsl の6レベルチェーン。レベル0がネイティブ解像度の
			///      1/2で、以降それぞれ半分ずつ。1枚のミップ付きテクスチャでは
			///      なくレベルごとに独立したコミットリソースにしているのは、
			///      UnorderedAccessBarrier も cmdList->Barrier ヘルパーも
			///      リソース全体を遷移させるため — このチェーンはレベルNを
			///      読みながらレベルN+1へ書く必要があり、それをやるには
			///      サブリソース単位の遷移が要る。アップサンプルチェーンが
			///      終わった時点で最終結果はレベル0に入っており、
			///      ToneMappingCS.hlsl がサンプルするのはそれ。
			Microsoft::WRL::ComPtr<ID3D12Resource> bloomResource_[6];
			D3D12_RESOURCE_STATES bloomState_[6] = {};
			Uint32 bloomUnorderedAccessViewIndex_[6] = {};
			Uint32 bloomShaderResourceViewIndex_[6] = {};

			/// [EN] AnamorphicFlareCS.hlsl's ping-pong working buffers, at
			///      width/8 x height/4 - HALF the width of the other
			///      quarter-res post-process buffers, so they carry a baked
			///      2:1 anamorphic squeeze. Compose samples them back with
			///      normal UVs, which stretches the result 2x horizontally
			///      and is what turns a round flare into a horizontal
			///      streak. The squeeze is baked into the dimensions rather
			///      than exposed as a setting because it is a resource size:
			///      a runtime slider would mean recreating the view. 2:1 is
			///      the historical standard anamorphic ratio.
			/// [JP] AnamorphicFlareCS.hlsl のピンポン作業バッファ、
			///      width/8 x height/4 — 他の1/4解像度ポストプロセス
			///      バッファの【半分の幅】で、2:1 のアナモルフィック圧縮を
			///      焼き込んである。Compose が通常のUVでサンプルして戻すと
			///      横に2倍引き伸ばされ、これが丸いフレアを横長の筋に
			///      変える。圧縮率を設定にせず寸法へ焼き込んでいるのは、
			///      これがリソースの寸法だから — 実行時スライダーにすると
			///      ビューの作り直しが要る。2:1 は歴史的な標準倍率。
			Microsoft::WRL::ComPtr<ID3D12Resource> anamorphicFlareResource_[2];
			D3D12_RESOURCE_STATES anamorphicFlareState_[2] = {};
			Uint32 anamorphicFlareUnorderedAccessViewIndex_[2] = {};
			Uint32 anamorphicFlareShaderResourceViewIndex_[2] = {};

			/// [EN] AnamorphicFlareCS.hlsl's Compose target, at the same
			///      quarter resolution as lensFlareResource_ (NOT squeezed -
			///      Compose is where the de-squeeze happens). This is what
			///      ToneMappingCS.hlsl samples and adds into the HDR color.
			/// [JP] AnamorphicFlareCS.hlsl の Compose の書き込み先。
			///      lensFlareResource_ と同じ1/4解像度(圧縮【されていない】 —
			///      圧縮を戻すのが Compose の役目)。ToneMappingCS.hlsl が
			///      サンプルしてHDRカラーへ加算するのはこれ。
			Microsoft::WRL::ComPtr<ID3D12Resource> anamorphicFlareOutputResource_;
			D3D12_RESOURCE_STATES anamorphicFlareOutputState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 anamorphicFlareOutputUnorderedAccessViewIndex_ = 0;
			Uint32 anamorphicFlareOutputShaderResourceViewIndex_ = 0;

			/// [EN] Ping-pong native-res HDR buffers the lens stage (lens
			///      distortion, then chromatic aberration, then vignette)
			///      passes the scene through. Two rather than one because the
			///      first two RESAMPLE - they read neighbours, so they cannot
			///      read and write the same texture and have to alternate.
			///      Vignette can and does write in place at the end, being a
			///      pure per-pixel multiply with no neighbour taps (the same
			///      reason BokehCS.hlsl may read-modify-write depth of
			///      field's buffer). Which effect reads which slot, and which
			///      slot the scene ends up in, is resolved entirely on the CPU
			///      in PrepareView; ToneMappingCS.hlsl just reads
			///      PostProcessIndices::lensStageShaderResourceViewIndex_
			///      whenever lensStageEnabled_ is set.
			/// [JP] レンズ段(レンズ歪曲 → 色収差 → ビネットの順)がシーンを
			///      通していく、ネイティブ解像度のピンポンHDRバッファ。
			///      1枚でなく2枚なのは、最初の2つが【再サンプル】だから —
			///      近傍を読むので同じテクスチャを読み書きできず、交互に
			///      使う必要がある。ビネットは近傍タップの無い画素ごとの
			///      乗算なので最後にその場で書ける(BokehCS.hlsl が被写界
			///      深度のバッファを read-modify-write できるのと同じ理由)。
			///      どのエフェクトがどちらのスロットを読み、最終的に
			///      シーンがどちらに残るかは、全て PrepareView がCPU側で
			///      解決する。ToneMappingCS.hlsl は lensStageEnabled_ が
			///      立っている間 PostProcessIndices::
			///      lensStageShaderResourceViewIndex_ を読むだけでよい。
			/// [EN] ColorGradingCS.hlsl's output: the scene composited,
			///      light-contribution-added, exposed and graded, still
			///      scene-referred linear because the tone curve has not run
			///      yet. Always native resolution like the lens stage.
			///      ToneMappingCS.hlsl reads this and applies only the curve
			///      whenever colour grading is enabled.
			/// [JP] ColorGradingCS.hlsl の出力。シーンを合成し、光の加算寄与を
			///      足し、露出を掛け、グレーディングまで済ませた状態。
			///      トーンカーブはまだなのでシーン参照リニアのまま。
			///      レンズ段と同じく常にネイティブ解像度。カラーグレーディングが
			///      有効な間、ToneMappingCS.hlsl はこれを読んでカーブだけを
			///      掛ける。
			Microsoft::WRL::ComPtr<ID3D12Resource> colorGradingResource_;
			D3D12_RESOURCE_STATES colorGradingState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 colorGradingUnorderedAccessViewIndex_ = 0;
			Uint32 colorGradingShaderResourceViewIndex_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> lensStageResource_[2];
			D3D12_RESOURCE_STATES lensStageState_[2] = {};
			Uint32 lensStageUnorderedAccessViewIndex_[2] = {};
			Uint32 lensStageShaderResourceViewIndex_[2] = {};

			/// [EN] DepthOfFieldCS.hlsl's output (BokehCS.hlsl read-modify-writes
			///      the same resource, it has no target of its own), ALWAYS at
			///      native resolution (width_/height_), even when DLSS-RR is
			///      active and the rest of this frame's post-process chain is
			///      running at outputWidth_/outputHeight_ - a whole replacement
			///      HDR buffer, not an additive contribution like
			///      lensFlareResource_ above. Because of that fixed native size,
			///      downstream reads MUST go through a UV-based SampleLevel (see
			///      ToneMappingCS.hlsl), never Load(dtid.xy) - a pixel-indexed
			///      Load assumes the source is the same resolution as the
			///      current dispatch, which is only true when DLSS-RR is off;
			///      with DLSS-RR on, dtid.xy ranges over the upscaled output and
			///      Load() would read out of bounds for everything past the
			///      native-sized corner, returning 0 there - this was a real bug
			///      (looked like "the viewport only fills ~1/(DLSS scale)^2 of
			///      the panel, rest black" once DLSS-RR + depth of field were
			///      both on). Always allocated; PostProcess::DepthOfFieldSettings.
			///      enabled_ gates whether Dispatch() writes into it and whether
			///      LensFlareCS.hlsl/ToneMappingCS.hlsl read it instead of the
			///      raw HDR source (same "allocate unconditionally, branch only
			///      at Dispatch/shader-read" pattern as every other Renderer
			///      here).
			/// [JP] DepthOfFieldCS.hlsl の出力(BokehCS.hlsl が同じリソースを
			///      read-modify-write する、自前の書き込み先は持たない)。
			///      DLSS-RR が有効でこのフレームの他のポストプロセス処理が
			///      outputWidth_/outputHeight_ で走っていても、これは常に
			///      ネイティブ解像度(width_/height_)固定 — 上の
			///      lensFlareResource_ と違い加算コントリビューションではなく、
			///      HDRバッファそのものの置き換え。この解像度固定のため、
			///      後続の読み取りは必ずUVベースのSampleLevelを使うこと
			///      (ToneMappingCS.hlsl 参照)、Load(dtid.xy) は使ってはいけない
			///      — ピクセル添字のLoadは「ソースが現在のディスパッチと同じ
			///      解像度」を前提にしており、それが成り立つのはDLSS-RRが
			///      無効な時だけ。DLSS-RR有効時は dtid.xy がアップスケール後の
			///      範囲を走るため、Load()だとネイティブ解像度ぶんの角以外が
			///      境界外アクセスとなり0が返る — 実際に起きたバグ
			///      (DLSS-RR+被写界深度を両方有効にすると「ビューポートが
			///      DLSSスケールの2乗分の1くらいしか埋まらず残りが黒くなる」
			///      症状になっていた)。常に確保しておく; PostProcess::
			///      DepthOfFieldSettings.enabled_ が Dispatch() が書き込むか、
			///      LensFlareCS.hlsl/ToneMappingCS.hlsl が生のHDRソースの代わりに
			///      これを読むかを切り替えるだけ(他の全 Renderer と同じ
			///      「無条件確保、Dispatch/シェーダ読み取り側だけで分岐」方式)。
			Microsoft::WRL::ComPtr<ID3D12Resource> depthOfFieldResource_;
			D3D12_RESOURCE_STATES depthOfFieldState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 depthOfFieldUnorderedAccessViewIndex_ = 0;
			Uint32 depthOfFieldShaderResourceViewIndex_ = 0;
		};

		void CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		void CreateView(ID3D12Device* device, BindlessHeap* bindlessHeap, View& view, Uint32 width, Uint32 height, Uint32 outputWidth, Uint32 outputHeight);

		void DestroyView(BindlessHeap* bindlessHeap, View& view);

		void UnorderedAccessBarrier(D3D12CommandList* cmdList, ID3D12Resource* resource);

		[[nodiscard]] View& ViewFor(RaytracingView view);

		/// [EN] Placeholder resolution: today just the first gathered volume
		///      (or PostProcess{} defaults if none). The one place to replace
		///      when priority/spatial blending across volumes_ is implemented.
		/// [JP] 暫定的な解決: 今は最初に見つかったボリューム(無ければ
		///      PostProcess{} の既定値)をそのまま返すだけ。volumes_ に対する
		///      優先度/空間ブレンドを実装する時に差し替える唯一の場所。
		[[nodiscard]] const PostProcess& ResolveSettings()const;

		AutoExposure autoExposureShader_;
		ToneMapping toneMappingShader_;
		KawaseBloom bloomShader_;
		AnamorphicFlare anamorphicFlareShader_;
		LensFlare lensFlareShader_;
		LensDistortion lensDistortionShader_;
		ChromaticAberration chromaticAberrationShader_;
		Vignette vignetteShader_;
		FilmGrain filmGrainShader_;
		ColorGrading colorGradingShader_;
		DepthOfField depthOfFieldShader_;
		Bokeh bokehShader_;
		Sharpness sharpnessShader_;

		View editorView_;
		View gameView_;

		/// [EN] Every PostProcess-carrying entity found by the last Gather().
		///      See the class doc comment for why this stays a list instead of
		///      collapsing to one settings value at gather time.
		/// [JP] 直近の Gather() で見つかった PostProcess を持つ全エンティティ。
		///      Gather 時点で1つの設定値へ潰さない理由はクラス先頭のコメント
		///      参照。
		DynamicArray<PostProcess> volumes_;

		/// [EN] Two slots per view (histogram + exposure), so 4 total.
		/// [JP] ビューごとに2枠(ヒストグラム+露出)で計4枠。
		DescriptorHeap clearHeap_;

		/// [EN] Four slots per view (SD output + UHD output + SD sharpen + UHD
		///      sharpen), so 8 total. Non-shader-visible RTV heap, distinct
		///      from bindlessHeap_ (SRV/UAV/CBV) and clearHeap_ (also
		///      CBV_SRV_UAV) - RTV descriptors require their own heap type.
		/// [JP] ビューごとに4枠(SD出力+UHD出力+SDシャープネス+UHDシャープネス)
		///      で計8枠。非シェーダ可視のRTVヒープ。bindlessHeap_(SRV/UAV/CBV)
		///      やclearHeap_(同じくCBV_SRV_UAV)とは別 — RTV記述子は専用の
		///      ヒープ種別を要する。
		DescriptorHeap renderTargetViewHeap_;

		BindlessHeap* bindlessHeap_ = nullptr;

		Uint32 width_ = 0;
		Uint32 height_ = 0;

		Uint32 outputWidth_ = 0;
		Uint32 outputHeight_ = 0;

		/// [EN] Logs the PSO-creation-failed warning once instead of every frame.
		/// [JP] PSO 作成失敗の警告を毎フレームでなく 1 度だけログ出力する。
		Bool pipelineStateMissingLogged_ = false;
	};
}
