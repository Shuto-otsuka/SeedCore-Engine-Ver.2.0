#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/PostProcess/PostEffect/AutoExposure.h>
#include <GraphicsEngine/PostProcess/PostEffect/ToneMapping.h>
#include <GraphicsEngine/PostProcess/PostEffect/LensFlare.h>
#include <GraphicsEngine/PostProcess/PostEffect/DepthOfField.h>
#include <GraphicsEngine/PostProcess/PostEffect/Bokeh.h>
#include <GraphicsEngine/PostProcess/PostProcess.h>
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

	private:
		struct View
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> outputResource_;
			D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_COMMON;
			Uint32 outputUnorderedAccessViewIndex_ = 0;

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
		LensFlare lensFlareShader_;
		DepthOfField depthOfFieldShader_;
		Bokeh bokehShader_;

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
