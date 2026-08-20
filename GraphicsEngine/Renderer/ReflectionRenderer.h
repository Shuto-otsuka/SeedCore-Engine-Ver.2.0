#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/SerializeFallback.h>
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

		/// [EN] Loaded field by field through TryLoadField rather than as one
		///      archive() call. A single archive() with several fields is
		///      atomic in the wrong direction: if any one name is missing - an
		///      older save, a field added or renamed since - cereal throws, and
		///      because RaytracingContext wraps this whole struct in its own
		///      TryLoadField, that exception is swallowed and EVERY setting in
		///      here silently reverts to its default. Per-field loading limits
		///      the damage to the field that actually changed.
		/// [JP] 1回の archive() ではなく TryLoadField でフィールドごとに読む。
		///      複数フィールドをまとめた archive() は悪い意味で不可分で、名前が
		///      1つでも欠けると(古い保存データ、後から追加・改名したフィールド)
		///      cereal が例外を投げる。そして RaytracingContext はこの構造体全体を
		///      自身の TryLoadField で包んでいるため、その例外は握り潰され、
		///      【ここの設定が全部黙って既定値に戻る】。フィールドごとに読めば、
		///      被害は実際に変わったフィールドだけで済む。
		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "rayTMax", rayTMax_);
			TryLoadField(archive, "normalBias", normalBias_);
			TryLoadField(archive, "strength", strength_);
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

		/// [EN] glTF alphaMode (0 OPAQUE / 1 MASK / 2 BLEND), alphaCutoff and
		///      baseColorFactor.a - read by Reflection.hlsli's
		///      IsReflectionMaterialPassthrough.
		/// [JP] glTF の alphaMode(0 OPAQUE / 1 MASK / 2 BLEND)、alphaCutoff、
		///      baseColorFactor.a — Reflection.hlsli の
		///      IsReflectionMaterialPassthrough が読む。
		Uint32 alphaMode_ = 0;
		Float alphaCutoff_ = 0.5f;
		Float baseColorAlpha_ = 1.0f;

		/// [EN] KHR_materials_volume thickness. Zero means thin-walled, which
		///      RefractionRT.hlsl uses to skip Beer-Lambert absorption.
		/// [JP] KHR_materials_volume の厚み。0 は thin-walled を意味し、
		///      RefractionRT.hlsl はそれを見て Beer-Lambert 吸収を飛ばす。
		Float thicknessFactor_ = 0.0f;
		Uint32 thicknessTextureIndex_ = 0xFFFFFFFF;
		Float materialPadding_ = 0.0f;
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
	* averaged hit distance in world units), then runs the 5-pass SVGF chain
	* over it (ReflectionDenoiseCS.hlsl: dual-reprojection temporal accumulation
	* with moment tracking -> spatial variance estimate for short history ->
	* three variance-guided A-Trous wavelet iterations, one independent chain
	* per view) and leaves the result in PIXEL_SHADER_RESOURCE state for
	* DeferredLightingPS.hlsl to sample. GGX importance sampling (see
	* ReflectionRT.hlsl) degenerates to the old exact mirror ray at roughness 0,
	* so this is a superset of the v1 behavior rather than a replacement of it —
	* same SVGF structure as ShadowRenderer, extended from a 2-channel binary
	* visibility signal to continuous HDR RGB radiance plus a hit-point virtual
	* motion reprojection candidate. Also owns the per-frame instance table
	* (InstanceID() -> vertex/index/material data) that RaytracingRenderer
	* fills while building the TLAS, and the 3-record shader table
	* (raygen/miss/hitgroup — global root signature only, so records are bare
	* shader identifiers).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレ光沢反射の RTPSO パス(ReflectionRT.hlsl — DispatchRays による
	* raygen / miss / closesthit)を生の単一バッファ RGBA16F 放射輝度テクスチャ
	* (rgb=入射反射放射輝度、a=レイの平均ヒット距離・ワールド単位)へ
	* ディスパッチし、それに対して5パスの SVGF チェーンを回した
	* (ReflectionDenoiseCS.hlsl: モーメント追跡つき二重リプロジェクション時間的
	* 蓄積 → 履歴が短いピクセル向けの空間的分散推定 → 分散誘導 A-Trous
	* ウェーブレット3反復。ビューごとに独立したチェーン)上で、
	* DeferredLightingPS.hlsl がサンプルできるよう PIXEL_SHADER_RESOURCE 状態に
	* しておく。GGX 重点サンプリング(ReflectionRT.hlsl 参照)は roughness 0 で
	* 旧・厳密ミラーレイへ縮退するため、これは v1 の置き換えではなく上位互換 —
	* ShadowRenderer と同じ SVGF 構成を、2チャンネルの二値可視性信号から連続値の
	* HDR RGB放射輝度+ヒット点仮想モーションのリプロジェクション候補へ拡張した
	* もの。RaytracingRenderer が TLAS 構築時に詰めるインスタンステーブル
	* (InstanceID() → 頂点/インデックス/マテリアル)と、3 レコードの
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
		///      reflection read index instead of the SVGF-denoised one — DLSS
		///      Ray Reconstruction denoises the whole composited frame itself,
		///      so this renderer's own temporal accumulation is skipped
		///      entirely to avoid double-denoising.
		/// [JP] チューニング用定数バッファを更新し(フレーム番号を焼き込む)、
		///      今フレームのピンポン history/write スロットを決め、bindless
		///      インデックスを IndicesSystem へ登録する。
		///      IndicesSystem::UploadEditor/UploadGame が今フレームのインデックスを
		///      確定する前に呼ぶこと。GPU 処理は無い。useDlssRayReconstruction が
		///      true の間は、このビューの「最終的な」反射読み取りインデックスに
		///      SVGFデノイズ済みSRVではなく生の単一バッファ放射輝度SRV
		///      (radianceShaderResourceViewIndex_)を登録する — DLSS Ray
		///      Reconstruction が合成フレーム全体を自身でデノイズするため、
		///      この Renderer 自身の時間的蓄積は二重デノイズを避けるため丸ごと
		///      スキップする。
		void PrepareFrame(const ReflectionRayConstantBuffer& settings, Bool useDlssRayReconstruction);

		/// [EN] The actual GPU work: DispatchRays into the raw texture, then
		///      (unless useDlssRayReconstruction) the SVGF chain — dual-
		///      reprojection temporal accumulation into scratch0 (plus this
		///      frame's moments/history length/depth-normal), FilterMoments
		///      into scratch1, A-Trous step1 back into scratch0, A-Trous step2
		///      into this frame's history write slot (the feedback tap),
		///      A-Trous step4 into the per-view denoised output, which is left
		///      in PIXEL_SHADER_RESOURCE state. When there is no TLAS / the
		///      feature is off / the RTPSO/PSO is missing, the denoised output
		///      is cleared to 0 and the history length to 0 instead. When
		///      useDlssRayReconstruction is true, the whole chain is skipped —
		///      only the raw texture is transitioned to PIXEL_SHADER_RESOURCE,
		///      since PrepareFrame() already pointed the composite shader at
		///      it directly. Requires the G-Buffer depth/normal/velocity to
		///      already be written.
		/// [JP] 実際の GPU 処理: DispatchRays を生テクスチャへ、続けて
		///      (useDlssRayReconstruction でなければ) SVGF チェーン —
		///      二重リプロジェクション時間的蓄積を scratch0 へ(今フレームの
		///      モーメント/履歴長/深度法線も併せて)、FilterMoments を
		///      scratch1 へ、A-Trous step1 を scratch0 へ戻し、A-Trous step2 を
		///      今フレームの history write スロット(フィードバックタップ)へ、
		///      A-Trous step4 をビューごとの denoised 出力へ書き、それを
		///      PIXEL_SHADER_RESOURCE 状態で終える。TLAS が無い/機能が無効/
		///      RTPSO・PSO が無ければ、代わりに denoised 出力を 0、履歴長を 0
		///      でクリアする。useDlssRayReconstruction が true の間はチェーンを
		///      丸ごとスキップする — 生テクスチャを PIXEL_SHADER_RESOURCE へ
		///      遷移させるだけでよい(PrepareFrame() が既に合成シェーダの参照先を
		///      そこへ直接向けているため)。G-Buffer の深度/法線/速度が書き込み
		///      済みであることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction);

	private:
		/// [EN] Allocates the raw texture and every per-view buffer of the SVGF
		///      chain. Shared by Create() and Resize() so the two can never
		///      drift apart as the chain gains or loses a buffer.
		/// [JP] raw テクスチャと、ビューごとの SVGF チェーン全バッファを確保
		///      する。Create() と Resize() で共有し、チェーンにバッファが増減しても
		///      両者がずれないようにする。
		void CreateResources(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		static constexpr Uint32 accumulationSlotCount = 2;
		static constexpr Uint32 viewCount = 2;

		ReflectionShader reflectionShader_;
		ReflectionDenoiseShader denoiseShader_;

		ResourcePtr<ConstantBuffer<ReflectionRayConstantBuffer>> tuningBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<ReflectionInstanceData>> instanceTable_;

		/// [EN] Raw noisy RGBA16F radiance output of ReflectionRT.hlsl (rgb =
		///      radiance, a = averaged hit distance in world units).
		///      Single-buffered — fully consumed by ReflectionDenoiseCS.hlsl
		///      the same frame it is written.
		/// [JP] ReflectionRT.hlsl の生ノイズ RGBA16F 放射輝度出力(rgb=放射輝度、
		///      a=平均ヒット距離・ワールド単位)。単一バッファ — 書かれた同じ
		///      フレーム内で ReflectionDenoiseCS.hlsl に消費し切られる。
		Microsoft::WRL::ComPtr<ID3D12Resource> radianceResource_;
		D3D12_RESOURCE_STATES radianceState_ = D3D12_RESOURCE_STATE_COMMON;
		Uint32 radianceUnorderedAccessViewIndex_ = 0;
		Uint32 radianceShaderResourceViewIndex_ = 0;

		/// [EN] SVGF's temporal history: rgb = filtered radiance, a = its
		///      variance. Ping-ponged, one independent pair per view (see
		///      RaytracingView). This is the FEEDBACK TAP, not the final
		///      image - ATrousPass2 writes it and next frame's reprojection
		///      reads it, while the image DeferredLightingPS.hlsl samples
		///      comes from denoisedResource_ below.
		/// [JP] SVGF の時間的履歴。rgb = フィルタ済み放射輝度、a = その分散。
		///      ピンポン方式で、ビューごと(RaytracingView 参照)に独立した1ペア。
		///      これは【フィードバックタップ】であって最終画ではない —
		///      ATrousPass2 が書き、次フレームのリプロジェクションが読む。
		///      DeferredLightingPS.hlsl がサンプルする画は下の denoisedResource_。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedRadianceResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedRadianceState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Per-pixel first/second luminance moments, packed as (1st, 2nd).
		///      Ping-ponged alongside the history above — SVGF derives its
		///      variance from the temporally accumulated moments, so they have
		///      to survive the frame exactly like the radiance does.
		/// [JP] ピクセルごとの輝度の1次/2次モーメント。(1次, 2次)の順で詰める。
		///      上の履歴と同じくピンポンする — SVGF は時間蓄積したモーメントから
		///      分散を求めるので、放射輝度と全く同様にフレームをまたいで保持する
		///      必要がある。
		Microsoft::WRL::ComPtr<ID3D12Resource> momentsResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES momentsState_[viewCount][accumulationSlotCount] = {};
		Uint32 momentsUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 momentsShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Per-pixel count of successfully reprojected frames. Drives the
		///      max(alpha, 1/length) blend factor and the switch to the
		///      spatial variance estimate. Ping-ponged.
		/// [JP] ピクセルごとのリプロジェクション成功フレーム数。
		///      max(alpha, 1/履歴長) のブレンド係数と、空間的分散推定への
		///      切り替え判定を駆動する。ピンポンする。
		Microsoft::WRL::ComPtr<ID3D12Resource> historyLengthResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES historyLengthState_[viewCount][accumulationSlotCount] = {};
		Uint32 historyLengthUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 historyLengthShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Packed (view depth, depth derivative, oct normal) copy of this
		///      frame's surface. Ping-ponged because SVGF's temporal
		///      consistency test compares against the PREVIOUS frame's
		///      version, and the engine's G-Buffer is single-buffered so it
		///      cannot be read back.
		/// [JP] 今フレームの面を (ビュー深度, 深度勾配, oct法線) で詰めたコピー。
		///      SVGF の時間的整合性テストが【前フレーム】の値と比較するため
		///      ピンポンする — エンジンの G-Buffer は単一バッファで、前フレームを
		///      読み戻せないため。
		Microsoft::WRL::ComPtr<ID3D12Resource> depthNormalResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES depthNormalState_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 depthNormalShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

		/// [EN] Fully filtered RGBA16F radiance DeferredLightingPS.hlsl
		///      samples (rgb = radiance, a = 1 valid / 0 background) — the
		///      output of the last A-Trous iteration. Single buffered per
		///      view: it is consumed the same frame it is written and never
		///      feeds back, which is exactly what lets the history above stop
		///      at the earlier, sharper feedback tap.
		/// [JP] DeferredLightingPS.hlsl がサンプルする、完全にフィルタ済みの
		///      RGBA16F 放射輝度(rgb=放射輝度、a=1有効/0背景) — 最後の
		///      A-Trous 反復の出力。ビューごとに単一バッファ: 書かれた同じ
		///      フレームで消費されフィードバックしない。これがあるからこそ、
		///      上の履歴を「より早く、よりシャープな」フィードバックタップで
		///      止められる。
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
		///      frame's history). Swapped once per frame at the top of
		///      PrepareFrame() — NOT in Dispatch(), which runs twice per frame
		///      (Editor + Game views).
		/// [JP] 前フレームの完成結果(今フレームの history)を持つスロット。
		///      交換は PrepareFrame() の冒頭で1フレームに1回だけ — Dispatch()
		///      では行わない(Editor/Game 両ビューで1フレームに2回走るため)。
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
		///      ClearUnorderedAccessViewFloat alongside the shader-visible
		///      ones. Only the surfaces actually cleared on the "nothing to
		///      trace" path need one: the raw texture, the per-view denoised
		///      output, and the history length (cleared to 0 so the filter
		///      re-converges from scratch rather than trusting a stale
		///      history).
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
		///      reads as zero, and every buffer here is fed back into itself the
		///      next frame - so a single uninitialized texel is not a one-frame
		///      glitch, it is latched in permanently. Undefined FP16 bit patterns
		///      are readily NaN, and because textures are stored swizzled the
		///      garbage surfaces as rectangular blocks rather than noise.
		/// [JP] 生成以降に履歴チェーンを 0 で埋めたかどうか。D3D12 は生成直後の
		///      committed リソースが 0 で読める保証をせず、ここのバッファは全て
		///      翌フレーム自分自身へ戻る — つまり未初期化テクセルが 1 つでもあると
		///      1 フレームの乱れでは済まず、恒久的に焼き付く。未定義の fp16
		///      ビットパターンは容易に NaN になり、テクスチャはスウィズルされて
		///      配置されるため、ノイズではなく矩形の塊として現れる。
		Bool historyCleared_ = false;

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
