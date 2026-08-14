#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Raytracing/Reflection/ReflectionShader.h>
#include <GraphicsEngine/Raytracing/Reflection/ReflectionDenoiseShader.h>
#include <GraphicsEngine/Raytracing/RaytracingView.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class IndicesSystem;

	/// [EN] Mirrors Raytracing/Reflection/Reflection.hlsli's
	///      ReflectionRayConstantBuffer — read by both ReflectionRT.hlsl and
	///      DeferredLightingPS.hlsl via
	///      structured_indices.reflection_ray_constant_index_. Must stay
	///      byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/Reflection/Reflection.hlsli の
	///      ReflectionRayConstantBuffer と対応。ReflectionRT.hlsl と
	///      DeferredLightingPS.hlsl の両方が
	///      structured_indices.reflection_ray_constant_index_ 経由で読む。
	///      HLSL 側とバイト単位で一致させること。
	struct ReflectionRayConstantBuffer
	{
		Float rayTMax_ = 1000.0f;
		Float normalBias_ = 0.01f;

		/// [EN] Overall reflection intensity applied in DeferredLightingPS.hlsl.
		/// [JP] DeferredLightingPS.hlsl で適用する反射の全体強度。
		Float strength_ = 1.0f;

		/// [EN] Incremented once per frame by ReflectionRenderer (not the UI) —
		///      rotates the GGX importance sample so the roughness-driven 1spp
		///      noise averages out over time instead of being a fixed pattern.
		/// [JP] ReflectionRenderer が毎フレーム1つずつ加算する(UI からは触らない)
		///      — GGX 重点サンプルを回して、ラフネス由来の 1spp ノイズが固定
		///      パターンにならず時間的に平均化されるようにする。
		Uint32 frameIndex_ = 0;

		template<class Archive>
		void serialize(Archive& archive)
		{
			archive(
				cereal::make_nvp("rayTMax", rayTMax_),
				cereal::make_nvp("normalBias", normalBias_),
				cereal::make_nvp("strength", strength_));
		}
	};

	/// [EN] Mirrors Reflection.hlsli's ReflectionMaterialData — one entry per
	///      material slot in a mesh's Crister::Materials() list. Uploaded once
	///      per unique Crister (RaytracingRenderer::BuildReflectionMaterialTable).
	/// [JP] Reflection.hlsli の ReflectionMaterialData と対応。メッシュの
	///      Crister::Materials() 一覧のマテリアル1枠につき1エントリ。ユニークな
	///      Crister ごとに一度だけアップロードする
	///      (RaytracingRenderer::BuildReflectionMaterialTable)。
	struct ReflectionMaterialData
	{
		Float baseColor_[3] = { 1.0f, 1.0f, 1.0f };
		Uint32 baseColorTextureIndex_ = 0xFFFFFFFF;

		/// [EN] KHR_materials_ior/transmission/volume - unused by Reflection
		///      itself, read by Raytracing/Refraction/RefractionRT.hlsl via the
		///      same per-triangle table (ResolveReflectionMaterial).
		/// [JP] KHR_materials_ior/transmission/volume - Reflection自体は使わない。
		///      Raytracing/Refraction/RefractionRT.hlsl が同じ三角形単位の
		///      テーブル(ResolveReflectionMaterial)経由で読む。
		Float ior_ = 1.5f;
		Float transmissionFactor_ = 0.0f;
		Float volumeAttenuationColor_[3] = { 1.0f, 1.0f, 1.0f };
		Float volumeAttenuationDistance_ = FLT_MAX;
	};

	/// [EN] Mirrors Reflection.hlsli's ReflectionInstanceData — one entry per
	///      TLAS instance (same order), indexed by InstanceID() in the
	///      closesthit shader. Structured buffer: tight packing.
	/// [JP] Reflection.hlsli の ReflectionInstanceData と対応。TLAS インスタンス
	///      ごとに1エントリ(同じ順序)、closesthit が InstanceID() で引く。
	///      StructuredBuffer なので詰めパッキング。
	struct ReflectionInstanceData
	{
		Uint32 vertexBufferIndex_ = 0;
		Uint32 indexBufferIndex_ = 0;

		/// [EN] Bindless SRV of StructuredBuffer<ReflectionMaterialData> - the
		///      mesh's full material list, resolved per-hit-triangle via
		///      triangleMaterialIndexBufferIndex_ below rather than a single
		///      flat color for the whole instance. See
		///      RaytracingRenderer::BuildReflectionMaterialTable and
		///      Reflection.hlsli's ResolveReflectionMaterial.
		/// [JP] StructuredBuffer<ReflectionMaterialData> の bindless SRV —
		///      メッシュの全マテリアル一覧。インスタンス全体で単一色にせず、
		///      下の triangleMaterialIndexBufferIndex_ 経由でヒット三角形ごとに
		///      解決する。RaytracingRenderer::BuildReflectionMaterialTable と
		///      Reflection.hlsli の ResolveReflectionMaterial 参照。
		Uint32 materialDataIndex_ = 0;

		/// [EN] Bindless SRV of StructuredBuffer<uint>, one entry per triangle,
		///      mapping PrimitiveIndex() to an index into materialDataIndex_'s
		///      array. Built once per unique Crister from each SubMesh's
		///      materialIndex_ / indexOffset_ / indexCount_ - a multi-material
		///      mesh (e.g. a Cornell box modeled as one Crister with a
		///      red/green/white wall per submesh) needs this: using only
		///      Materials()[0] for the whole instance made every ray-traced
		///      bounce off such a mesh come back the same single colour
		///      regardless of which wall it actually hit, silently killing
		///      color bleeding.
		/// [JP] StructuredBuffer<uint> の bindless SRV。三角形1つにつき1要素で、
		///      PrimitiveIndex() を materialDataIndex_ の配列インデックスへ
		///      対応付ける。ユニークな Crister ごとに一度だけ、各 SubMesh の
		///      materialIndex_ / indexOffset_ / indexCount_ から構築する —
		///      複数マテリアルのメッシュ(例: 赤/緑/白の壁をサブメッシュに持つ
		///      1つの Crister で作った Cornell box)にはこれが必須: インスタンス
		///      全体で Materials()[0] だけを使っていたときは、そのメッシュに
		///      当たったレイトレのバウンスが、実際にどの壁に当たったかに
		///      関わらず全部同じ1色で返ってきており、色滲みが黙って死んでいた。
		Uint32 triangleMaterialIndexBufferIndex_ = 0;

		/// [EN] The compressed vertex UVs are UNORM within this AABB, so the
		///      closesthit shader needs it to decode them (Crister::TexcoordMin /
		///      TexcoordExtent — the same values ModelRenderer feeds the raster
		///      path through its instance data).
		/// [JP] 圧縮頂点の UV はこの AABB 内の UNORM なので、closesthit が
		///      デコードするのに必要(Crister::TexcoordMin / TexcoordExtent —
		///      ラスタ経路で ModelRenderer がインスタンスデータ経由で渡している
		///      のと同じ値)。
		Float texcoordMin_[2] = { 0.0f, 0.0f };
		Float texcoordExtent_[2] = { 1.0f, 1.0f };
	};

	/**
	* [EN]
	* Dispatches the ray-traced glossy reflection RTPSO pass (ReflectionRT.hlsl —
	* raygen / miss / closesthit via DispatchRays) into a raw single-buffered
	* RGBA16F radiance texture (rgb = incoming reflected radiance, a = the ray's
	* normalized hit distance), then runs the 5-pass ReBLUR chain over it
	* (ReflectionDenoiseCS.hlsl: pre-pass spatial reuse -> temporal accumulation
	* with surface + virtual motion -> history fix -> blur -> post-blur, one
	* independent chain per view) and leaves the result in
	* PIXEL_SHADER_RESOURCE state for DeferredLightingPS.hlsl to sample. GGX importance sampling (see
	* ReflectionRT.hlsl) degenerates to the old exact mirror ray at roughness 0,
	* so this is a superset of the v1 behavior rather than a replacement of it —
	* same structure as GlobalIlluminationRenderer, extended from GI's
	* single-bounce hemisphere sample to a roughness-driven GGX lobe. Also owns
	* the per-frame instance table (InstanceID() -> vertex/index/material data)
	* that RaytracingRenderer fills while building the TLAS, and the 3-record
	* shader table (raygen/miss/hitgroup — global root signature only, so
	* records are bare shader identifiers).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ光沢反射の RTPSO パス(ReflectionRT.hlsl — DispatchRays による
	* raygen / miss / closesthit)を生の単一バッファ RGBA16F 放射輝度テクスチャ
	* (rgb=入射反射放射輝度、a=レイの正規化ヒット距離)へディスパッチし、それに
	* 対して5パスの ReBLUR チェーンを回した(ReflectionDenoiseCS.hlsl: プリパスの
	* 空間再利用 → 面モーション+仮想モーションによる時間的蓄積 → ヒストリ
	* フィックス → ブラー → ポストブラー。ビューごとに独立したチェーン)上で、
	* DeferredLightingPS.hlsl がサンプルできる
	* よう PIXEL_SHADER_RESOURCE 状態にしておく。GGX 重点サンプリング
	* (ReflectionRT.hlsl 参照)は roughness 0 で旧・厳密ミラーレイへ縮退するため、
	* これは v1 の置き換えではなく上位互換。GlobalIlluminationRenderer と同じ
	* 構成を、GI の単一バウンス半球サンプルから roughness 駆動の GGX ローブへ
	* 拡張したもの。RaytracingRenderer が TLAS 構築時に詰めるインスタンス
	* テーブル(InstanceID() → 頂点/インデックス/マテリアル)と、3 レコードの
	* シェーダテーブル(raygen/miss/hitgroup — グローバルルートシグネチャのみ
	* なのでレコードはシェーダ識別子だけ)も、ここが持つ。
	*/
	class ReflectionRenderer
	{
	public:
		/// [EN] Max TLAS instances the per-frame instance table can hold.
		/// [JP] インスタンステーブルが保持できる TLAS インスタンスの最大数。
		static constexpr Uint32 maxInstances = 4096;

		ReflectionRenderer(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject, PipelineStateObject& pipelineStateObject);
		~ReflectionRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Uploads this frame's instance table (same order as the TLAS
		///      instance descs; entry i belongs to InstanceID() == i). Called
		///      by RaytracingRenderer::Build right after collecting the
		///      instances.
		/// [JP] 今フレームのインスタンステーブルをアップロードする(TLAS の
		///      インスタンス desc と同じ順序。エントリ i が InstanceID() == i)。
		///      RaytracingRenderer::Build がインスタンス収集直後に呼ぶ。
		void UpdateInstanceTable(const ReflectionInstanceData* data, Uint32 count);

		/// [EN] Updates the tuning constant buffer (stamping the frame counter),
		///      decides this frame's history/write ping-pong slot, and
		///      registers every bindless index into IndicesSystem. Must run
		///      before IndicesSystem::UploadEditor/UploadGame bakes this frame's
		///      indices. No GPU work. When useDlssRayReconstruction is true,
		///      registers the RAW single-buffered radiance SRV
		///      (radianceShaderResourceViewIndex_) as this view's "final"
		///      reflection read index instead of the ping-ponged denoised one —
		///      DLSS Ray Reconstruction denoises the whole composited frame
		///      itself, so this renderer's own temporal accumulation is skipped
		///      entirely to avoid double-denoising.
		/// [JP] チューニング用定数バッファを更新し(フレーム番号を焼き込む)、
		///      今フレームのピンポン history/write スロットを決め、bindless
		///      インデックスを IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームのインデックスを
		///      確定する前に呼ぶこと。GPU 処理は無い。useDlssRayReconstruction が
		///      true の間は、このビューの「最終的な」反射読み取りインデックスに
		///      ピンポンデノイズ済みSRVではなく生の単一バッファ放射輝度SRV
		///      (radianceShaderResourceViewIndex_)を登録する — DLSS Ray
		///      Reconstruction が合成フレーム全体を自身でデノイズするため、
		///      この Renderer 自身の時間的蓄積は二重デノイズを避けるため丸ごと
		///      スキップする。
		void PrepareFrame(const ReflectionRayConstantBuffer& settings, Bool useDlssRayReconstruction);

		/// [EN] The actual GPU work: DispatchRays into the raw texture, then
		///      (unless useDlssRayReconstruction) the ReBLUR chain — PrePass
		///      into scratch0, temporal accumulation into scratch1, HistoryFix
		///      back into scratch0, Blur into scratch1, PostBlur into this
		///      frame's write slot, which is left in PIXEL_SHADER_RESOURCE
		///      state. When there is no TLAS / the feature is off / the
		///      RTPSO/PSO is missing, the write slot is cleared to 0 and the
		///      accumulation speed to 0 instead. When useDlssRayReconstruction
		///      is true, the whole chain is skipped — only the raw texture is
		///      transitioned to PIXEL_SHADER_RESOURCE, since PrepareFrame()
		///      already pointed the composite shader at it directly. Requires
		///      the G-Buffer depth/normal/velocity to already be written.
		/// [JP] 実際の GPU 処理: DispatchRays を生テクスチャへ、続けて
		///      (useDlssRayReconstruction でなければ) ReBLUR チェーン —
		///      PrePass を scratch0 へ、時間的蓄積を scratch1 へ、HistoryFix を
		///      scratch0 へ戻し、Blur を scratch1 へ、PostBlur を今フレームの
		///      write スロットへ書き、それを PIXEL_SHADER_RESOURCE 状態で終える。
		///      TLAS が無い/機能が無効/RTPSO・PSO が無ければ、代わりに write
		///      スロットを 0、蓄積速度を 0 でクリアする。
		///      useDlssRayReconstruction が true の間はチェーンを丸ごと
		///      スキップする — 生テクスチャを PIXEL_SHADER_RESOURCE へ遷移させる
		///      だけでよい(PrepareFrame() が既に合成シェーダの参照先をそこへ
		///      直接向けているため)。G-Buffer の深度/法線/速度が書き込み済み
		///      であることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction);

	private:
		/// [EN] Allocates the raw texture and every per-view buffer of the
		///      ReBLUR chain. Shared by Create() and Resize() so the two can
		///      never drift apart as the chain gains or loses a buffer.
		/// [JP] raw テクスチャと、ビューごとの ReBLUR チェーン全バッファを確保
		///      する。Create() と Resize() で共有し、チェーンにバッファが増減しても
		///      両者がずれないようにする。
		void CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		static constexpr Uint32 accumulationSlotCount = 2;
		static constexpr Uint32 viewCount = 2;

		ReflectionShader reflectionShader_;
		ReflectionDenoiseShader denoiseShader_;

		ResourcePtr<ConstantBuffer<ReflectionRayConstantBuffer>> tuningBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<ReflectionInstanceData>> instanceTable_;

		/// [EN] Raw noisy RGBA16F radiance output of ReflectionRT.hlsl.
		///      Single-buffered — fully consumed by ReflectionDenoiseCS.hlsl
		///      the same frame it is written.
		/// [JP] ReflectionRT.hlsl の生ノイズ RGBA16F 放射輝度出力。
		///      単一バッファ — 書かれた同じフレーム内で
		///      ReflectionDenoiseCS.hlsl に消費し切られる。
		Microsoft::WRL::ComPtr<ID3D12Resource> radianceResource_;
		D3D12_RESOURCE_STATES radianceState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 radianceUnorderedAccessViewIndex_ = 0;
		Uint32 radianceShaderResourceViewIndex_ = 0;

		/// [EN] Ping-ponged ReBLUR output, one independent pair per view (see
		///      RaytracingView). rgb = denoised radiance, a = the accumulated
		///      NORMALIZED HIT DISTANCE — PostBlur writes it, next frame's
		///      temporal accumulation reads it back as history, and
		///      DeferredLightingPS.hlsl samples the same texture. The alpha is
		///      not a validity flag: ReBLUR derives its whole blur radius from
		///      that distance, so it has to survive into the history.
		/// [JP] ピンポン方式の ReBLUR 出力。ビューごと(RaytracingView 参照)に
		///      独立した1ペア。rgb = デノイズ済み放射輝度、a = 蓄積済みの
		///      【正規化ヒット距離】。PostBlur が書き、次フレームの時間的蓄積が
		///      履歴として読み戻し、DeferredLightingPS.hlsl も同じテクスチャを
		///      サンプルする。アルファは有効フラグではない — ReBLUR はブラー半径の
		///      全てをこの距離から導くため、履歴まで持ち越す必要がある。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedRadianceResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedRadianceState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Per-pixel accumulated frame count. This is ReBLUR's central
		///      state: it sets the temporal blend factor (1 / (1 + frames)), it
		///      shrinks every spatial radius as a pixel converges, and it is what
		///      HistoryFix tests to find the pixels that have no usable history.
		///      Ping-ponged alongside the radiance above.
		/// [JP] ピクセルごとの蓄積フレーム数。ReBLUR の中心的な状態:
		///      時間ブレンド係数 (1 / (1 + フレーム数)) を決め、ピクセルが収束する
		///      につれて全ての空間半径を縮め、HistoryFix が「使える履歴が無い
		///      ピクセル」を見つけるための判定値でもある。上の放射輝度と同じく
		///      ピンポンする。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumSpeedResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumSpeedState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumSpeedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumSpeedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] ReBLUR's short-history luma. Ping-ponged like the radiance above.
		///      Luminance only — the clamp it feeds only needs a magnitude, and
		///      keeping chroma out of it is what lets the clamp correct lag
		///      without disturbing the colour the long history resolved. See
		///      REFLECTION_MAX_FAST_ACCUM_FRAME_NUM in ReflectionDenoiseCS.hlsl
		///      for why a second history exists at all.
		/// [JP] ReBLUR の短期履歴ルミナンス。上の放射輝度と同じくピンポンする。
		///      ルミナンスのみ — これが供給するクランプに必要なのは大きさだけで、
		///      色度を含めないからこそ、長期履歴が解いた色を乱さずにラグだけを
		///      補正できる。そもそも2本目の履歴を持つ理由は
		///      ReflectionDenoiseCS.hlsl の REFLECTION_MAX_FAST_ACCUM_FRAME_NUM 参照。
		Microsoft::WRL::ComPtr<ID3D12Resource> fastHistoryResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES fastHistoryState_[viewCount][accumulationSlotCount] = {};
		Uint32 fastHistoryUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 fastHistoryShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Packed (view depth, oct normal, roughness) copy of this frame's
		///      surface. Ping-ponged because ReBLUR's disocclusion test and its
		///      virtual-motion confidence both compare against the PREVIOUS
		///      frame's version, and the engine's G-Buffer is single-buffered so
		///      it cannot be read back.
		/// [JP] 今フレームの面を (ビュー深度, oct法線, ラフネス) で詰めたコピー。
		///      ReBLUR のディスオクルージョン判定と仮想モーションの信頼度が
		///      どちらも【前フレーム】の値と比較するためピンポンする —
		///      エンジンの G-Buffer は単一バッファで、前フレームを読み戻せない。
		Microsoft::WRL::ComPtr<ID3D12Resource> depthNormalResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES depthNormalState_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Ping-pong scratch the pre-pass / temporal / history-fix / blur
		///      chain bounces between, one pair per view. Pure scratch - always
		///      fully overwritten by the pass that writes it.
		/// [JP] プリパス/時間蓄積/ヒストリフィックス/ブラーのチェーンが往復する
		///      ピンポンスクラッチ、ビューごとに1ペア。純粋なスクラッチで、
		///      書き込むパスが必ず全画素を上書きする。
		Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource_[viewCount][2];
		D3D12_RESOURCE_STATES scratchState_[viewCount][2] = {};
		Uint32 scratchUnorderedAccessViewIndex_[viewCount][2] = {};
		Uint32 scratchShaderResourceViewIndex_[viewCount][2] = {};

		/// [EN] Which slot holds the previous frame's finished result (this
		///      frame's history). Swapped once per frame at the top of
		///      PrepareFrame() — NOT in Dispatch(), which runs twice per frame
		///      (Editor + Game views). Same rule as GlobalIlluminationRenderer.
		/// [JP] 前フレームの完成結果(今フレームの history)を持つスロット。
		///      交換は PrepareFrame() の冒頭で1フレームに1回だけ — Dispatch()
		///      では行わない(Editor/Game 両ビューで1フレームに2回走るため)。
		///      GlobalIlluminationRenderer と同じルール。
		Uint32 historySlot_ = 0;

		/// [EN] 3-record shader table: raygen @ 0, miss @ 64, hitgroup @ 128
		///      (records are bare 32-byte shader identifiers padded to the
		///      64-byte table alignment; no local root arguments). Built once
		///      in Create() — identifiers are stable for the state object's
		///      lifetime.
		/// [JP] 3 レコードのシェーダテーブル: raygen @ 0、miss @ 64、
		///      hitgroup @ 128(レコードは 32 バイトのシェーダ識別子のみで、
		///      64 バイトのテーブルアラインメントまでパディング。ローカル
		///      ルート引数は無し)。識別子はステートオブジェクトの生存期間中
		///      不変なので Create() で 1 度だけ構築する。
		Microsoft::WRL::ComPtr<ID3D12Resource> shaderTableResource_;
		static constexpr Uint32 shaderTableRecordSize = 64;

		/// [EN] Non-shader-visible UAV descriptors required by
		///      ClearUnorderedAccessViewFloat alongside the shader-visible ones.
		///      Only the surfaces actually cleared on the "nothing to trace" path
		///      need one: the raw texture, the accumulated output the composite
		///      reads, and the accumulation speed (cleared to 0 so the filter
		///      re-converges from scratch rather than trusting a stale history).
		/// [JP] ClearUnorderedAccessViewFloat がシェーダ可視の UAV と併せて
		///      要求する、非シェーダ可視の UAV ディスクリプタ。「追跡対象なし」
		///      経路で実際にクリアする面だけが必要 — raw、composite が読む
		///      accumulated 出力、そして蓄積速度(0 でクリアし、古い履歴を信用せず
		///      ゼロから収束し直させる)。
		DescriptorHeap clearHeap_;
		Uint32 clearRawIndex_ = 0;
		Uint32 clearAccumulatedIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 clearAccumSpeedIndex_[viewCount][accumulationSlotCount] = {};

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
