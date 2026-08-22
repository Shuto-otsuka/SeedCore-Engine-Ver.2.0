#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationShader.h>
#include <GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationDenoiseShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/GlobalIllumination/GlobalIllumination.hlsli's
	///      GlobalIlluminationRayConstantBuffer — read by
	///      GlobalIlluminationRT.hlsl and DeferredLightingPS.hlsl via
	///      structured_indices.global_illumination_ray_constant_index_. Must
	///      stay byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/GlobalIllumination/GlobalIllumination.hlsli の
	///      GlobalIlluminationRayConstantBuffer と対応。GlobalIlluminationRT.hlsl
	///      と DeferredLightingPS.hlsl の両方が
	///      structured_indices.global_illumination_ray_constant_index_ 経由で
	///      読む。HLSL 側とバイト単位で一致させること。
	struct GlobalIlluminationRayConstantBuffer
	{
		/// [EN] How far an indirect ray travels before it is treated as reaching
		///      the sky. Too short and interiors lose their bounce; too long and
		///      every ray pays for a full-scene traversal.
		/// [JP] 間接レイが空に届いたと見なすまでの距離。短すぎると室内のバウンス
		///      が消え、長すぎると全レイがシーン全体の走査コストを払う。
		Float rayTMax_ = 2000.0f;

		/// [EN] Offset along the normal to keep the ray off its own surface.
		/// [JP] レイが自分の面を叩かないよう法線方向へ押し出す量。
		Float normalBias_ = 0.05f;

		/// [EN] Overall indirect intensity, applied in DeferredLightingPS.hlsl.
		/// [JP] 間接光全体の強さ。DeferredLightingPS.hlsl 側で掛かる。
		Float intensity_ = 1.0f;

		/// [EN] Incremented once per frame by GlobalIlluminationRenderer (not the
		///      UI) — rotates the hemisphere sample so the 1spp noise averages
		///      out over time instead of being a fixed pattern.
		/// [JP] GlobalIlluminationRenderer が毎フレーム1つずつ加算する(UI からは
		///      触らない) — 半球サンプルを回して、1spp のノイズが固定パターンに
		///      ならず時間的に平均化されるようにする。
		Uint32 frameIndex_ = 0;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.TryField("rayTMax", rayTMax_);
			archive.TryField("normalBias", normalBias_);
			archive.TryField("intensity", intensity_);
		}
	};

	/// [EN] The HLSL mirror is one 16-byte cbuffer row of 4 scalars.
	/// [JP] HLSL 側は 4 スカラー × 1 行(16バイト)。
	static_assert(sizeof(GlobalIlluminationRayConstantBuffer) == 4 * sizeof(Float),
		"GlobalIlluminationRayConstantBuffer が GlobalIllumination.hlsli とバイト単位で一致していません");

	/**
	* [EN]
	* Dispatches the ray-traced one-bounce diffuse GI RTPSO pass
	* (GlobalIlluminationRT.hlsl — raygen / miss / closesthit via DispatchRays)
	* into a raw single-buffered RGBA16F radiance texture (rgb = incoming
	* indirect radiance, a = 1 valid / 0 invalid), then denoises it
	* (GlobalIlluminationDenoiseCS.hlsl: spatial bilateral filter + temporal
	* reprojection with variance clipping against a ping-ponged accumulation
	* buffer, one independent chain per view) and leaves the result in
	* PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl, which
	* multiplies it by the receiving surface's albedo. Same structure as
	* AmbientOcclusionRenderer, extended from a scalar signal to RGB. The
	* bounce surface's geometry and material come from reflection's
	* already-uploaded instance table rather than a second copy.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ1バウンス拡散GIの RTPSO パス(GlobalIlluminationRT.hlsl —
	* raygen / miss / closesthit を DispatchRays)を生の単一バッファ RGBA16F
	* 放射輝度テクスチャ(rgb=入射間接放射輝度、a=1 有効 / 0 無効)へ
	* ディスパッチし、それをデノイズ(GlobalIlluminationDenoiseCS.hlsl: 空間
	* バイラテラルフィルタ+ビューごとに独立したピンポン蓄積バッファに対する
	* 分散クリッピング付き時間的リプロジェクション)した上で、
	* DeferredLightingPS.hlsl が受け側の面のアルベドを掛けられるよう
	* PIXEL_SHADER_RESOURCE 状態にしておく。AmbientOcclusionRenderer と同じ
	* 構成をスカラーからRGBへ拡張したもの。バウンス面のジオメトリ/マテリアルは、
	* 反射パスがアップロード済みのインスタンステーブルを再利用する(2つ目の
	* コピーは作らない)。
	*/
	class GlobalIlluminationRenderer
	{
	public:
		GlobalIlluminationRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject, PipelineStateObject& pipelineStateObject);
		~GlobalIlluminationRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Updates the tuning constant buffer (stamping the frame counter),
		///      decides this frame's history/write ping-pong slot, and
		///      registers every bindless index into IndicesSystem. Must run
		///      before IndicesSystem::UploadEditor/UploadGame bakes this frame's
		///      indices. No GPU work. When useDlssRayReconstruction is true,
		///      registers the RAW single-buffered radiance SRV
		///      (radianceShaderResourceViewIndex_) as this view's "final" GI
		///      read index instead of the ping-ponged denoised one — DLSS Ray
		///      Reconstruction denoises the whole composited frame itself, so
		///      this renderer's own temporal accumulation is skipped entirely
		///      to avoid double-denoising.
		/// [JP] チューニング用定数バッファを更新し(フレーム番号を焼き込む)、
		///      今フレームのピンポン history/write スロットを決め、bindless
		///      インデックスを IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームのインデックスを
		///      確定する前に呼ぶこと。GPU 処理は無い。useDlssRayReconstruction が
		///      true の間は、このビューの「最終的な」GI読み取りインデックスに
		///      ピンポンデノイズ済みSRVではなく生の単一バッファ放射輝度SRV
		///      (radianceShaderResourceViewIndex_)を登録する — DLSS Ray
		///      Reconstruction が合成フレーム全体を自身でデノイズするため、
		///      この Renderer 自身の時間的蓄積は二重デノイズを避けるため丸ごと
		///      スキップする。
		void PrepareFrame(const GlobalIlluminationRayConstantBuffer& settings, Bool useDlssRayReconstruction);

		/// [EN] The actual GPU work: DispatchRays into the raw texture, then
		///      (unless useDlssRayReconstruction) GlobalIlluminationDenoiseCS.hlsl
		///      into this frame's write slot (or clears it to 0 when there is
		///      no TLAS / the feature is off / the RTPSO/PSO is missing),
		///      leaving the write slot in PIXEL_SHADER_RESOURCE state. When
		///      useDlssRayReconstruction is true, the denoise dispatch and the
		///      accumulation ping-pong are skipped entirely — only the raw
		///      texture is transitioned to PIXEL_SHADER_RESOURCE, since
		///      PrepareFrame() already pointed the composite shader at it
		///      directly. Requires the G-Buffer depth/normal/velocity to
		///      already be written.
		/// [JP] 実際の GPU 処理: DispatchRays を生テクスチャへ、続けて
		///      (useDlssRayReconstruction でなければ)
		///      GlobalIlluminationDenoiseCS.hlsl を今フレームの write スロットへ
		///      ディスパッチする(TLAS が無い/機能が無効/RTPSO・PSO が無ければ
		///      0 でクリア)。write スロットは PIXEL_SHADER_RESOURCE 状態で
		///      終える。useDlssRayReconstruction が true の間はデノイズ
		///      ディスパッチと蓄積ピンポンを丸ごとスキップする — 生テクスチャを
		///      PIXEL_SHADER_RESOURCE へ遷移させるだけでよい(PrepareFrame() が
		///      既に合成シェーダの参照先をそこへ直接向けているため)。G-Buffer の
		///      深度/法線/速度が書き込み済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction);

	private:
		static constexpr Uint32 accumulationSlotCount = 2;
		static constexpr Uint32 viewCount = 2;

		GlobalIlluminationShader globalIlluminationShader_;
		GlobalIlluminationDenoiseShader denoiseShader_;

		ResourcePtr<ConstantBuffer<GlobalIlluminationRayConstantBuffer>> tuningBuffer_;

		/// [EN] Raw noisy RGBA16F radiance output of GlobalIlluminationRT.hlsl.
		///      Single-buffered — fully consumed by
		///      GlobalIlluminationDenoiseCS.hlsl the same frame it is written.
		/// [JP] GlobalIlluminationRT.hlsl の生ノイズ RGBA16F 放射輝度出力。
		///      単一バッファ — 書かれた同じフレーム内で
		///      GlobalIlluminationDenoiseCS.hlsl に消費し切られる。
		Microsoft::WRL::ComPtr<ID3D12Resource> radianceResource_;
		D3D12_RESOURCE_STATES radianceState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 radianceUnorderedAccessViewIndex_ = 0;
		Uint32 radianceShaderResourceViewIndex_ = 0;

		/// [EN] Ping-ponged accumulated (denoised) radiance, one independent
		///      pair per view (see RaytracingView) — same scheme as
		///      AmbientOcclusionRenderer's accumulated openness.
		/// [JP] ピンポン方式の蓄積(デノイズ済み)放射輝度。ビューごと
		///      (RaytracingView 参照)に独立した1ペア — AmbientOcclusionRenderer
		///      の蓄積開放度と同じ方式。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedRadianceResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedRadianceState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] A-Trous ping-pong scratch, one pair per view. Unlike
		///      accumulatedRadianceResource_ above, these are NOT frame-ring
		///      double-buffered - they are pure intermediate scratch, fully
		///      overwritten by every A-Trous pass every frame (their bindless
		///      indices are registered once in Create()/Resize(), see
		///      IndicesSystem::SetEditor/GameGlobalIlluminationAtrousScratchIndices).
		/// [JP] A-Trous ピンポンスクラッチ、ビューごとに1ペア。上の
		///      accumulatedRadianceResource_ と違い、こちらはフレームリング
		///      二重化しない - 純粋な中間スクラッチで、毎フレーム全ての
		///      A-Trous パスが全画素を上書きする(bindless インデックスは
		///      Create()/Resize() で一度だけ登録する。
		///      IndicesSystem::SetEditor/GameGlobalIlluminationAtrousScratchIndices
		///      参照)。
		Microsoft::WRL::ComPtr<ID3D12Resource> atrousScratchResource_[viewCount][2];
		D3D12_RESOURCE_STATES atrousScratchState_[viewCount][2] = {};
		Uint32 atrousScratchUnorderedAccessViewIndex_[viewCount][2] = {};
		Uint32 atrousScratchShaderResourceViewIndex_[viewCount][2] = {};

		/// [EN] Which slot holds the previous frame's finished result (this
		///      frame's history). Swapped once per frame at the top of
		///      PrepareFrame() — NOT in Dispatch(), which runs twice per frame
		///      (Editor + Game views). Same rule as AmbientOcclusionRenderer.
		/// [JP] 前フレームの完成結果(今フレームの history)を持つスロット。
		///      交換は PrepareFrame() の冒頭で1フレームに1回だけ — Dispatch()
		///      では行わない(Editor/Game 両ビューで1フレームに2回走るため)。
		///      AmbientOcclusionRenderer と同じルール。
		Uint32 historySlot_ = 0;

		/// [EN] 3-record shader table: raygen @ 0, miss @ 64, hitgroup @ 128.
		///      Bare 32-byte shader identifiers padded to the 64-byte table
		///      alignment; no local root arguments. Built once in Create() —
		///      identifiers are stable for the state object's lifetime.
		/// [JP] 3 レコードのシェーダテーブル: raygen @ 0、miss @ 64、
		///      hitgroup @ 128。32 バイトのシェーダ識別子のみを 64 バイトの
		///      テーブルアラインメントまでパディング、ローカルルート引数は無し。
		///      識別子はステートオブジェクトの生存期間中不変なので Create() で
		///      1 度だけ構築する。
		Microsoft::WRL::ComPtr<ID3D12Resource> shaderTableResource_;
		static constexpr Uint32 shaderTableRecordSize = 64;

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

		/// [EN] Logs the state-object/shader-table/PSO failure once instead of
		///      every frame.
		/// [JP] ステートオブジェクト/シェーダテーブル/PSO 失敗の警告を毎フレーム
		///      でなく 1 度だけログ出力する。
		Bool stateObjectMissingLogged_ = false;
	};
}
