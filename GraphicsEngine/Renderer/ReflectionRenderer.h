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
	///      DeferredCompositePS.hlsl via
	///      structured_indices.reflection_ray_constant_index_. Must stay
	///      byte-for-byte in sync with the HLSL side.
	/// [JP] Raytracing/Reflection/Reflection.hlsli の
	///      ReflectionRayConstantBuffer と対応。ReflectionRT.hlsl と
	///      DeferredCompositePS.hlsl の両方が
	///      structured_indices.reflection_ray_constant_index_ 経由で読む。
	///      HLSL 側とバイト単位で一致させること。
	struct ReflectionRayConstantBuffer
	{
		Float rayTMax_ = 1000.0f;
		Float normalBias_ = 0.01f;

		/// [EN] Overall reflection intensity applied in DeferredCompositePS.hlsl.
		/// [JP] DeferredCompositePS.hlsl で適用する反射の全体強度。
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
	* RGBA16F radiance texture (rgb = incoming reflected radiance, a = 1 valid /
	* 0 invalid), then denoises it (ReflectionDenoiseCS.hlsl: spatial bilateral
	* filter + temporal reprojection with variance clipping against a
	* ping-ponged accumulation buffer, one independent chain per view) and
	* leaves the result in PIXEL_SHADER_RESOURCE state for
	* DeferredCompositePS.hlsl to sample. GGX importance sampling (see
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
	* (rgb=入射反射放射輝度、a=1 有効 / 0 無効)へディスパッチし、それを
	* デノイズ(ReflectionDenoiseCS.hlsl: 空間バイラテラルフィルタ+ビューごとに
	* 独立したピンポン蓄積バッファに対する分散クリッピング付き時間的
	* リプロジェクション)した上で、DeferredCompositePS.hlsl がサンプルできる
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
		///      (unless useDlssRayReconstruction) ReflectionDenoiseCS.hlsl into
		///      this frame's write slot (or clears it to 0 when there is no
		///      TLAS / the feature is off / the RTPSO/PSO is missing), leaving
		///      the write slot in PIXEL_SHADER_RESOURCE state. When
		///      useDlssRayReconstruction is true, the denoise dispatch and the
		///      accumulation ping-pong are skipped entirely — only the raw
		///      texture is transitioned to PIXEL_SHADER_RESOURCE, since
		///      PrepareFrame() already pointed the composite shader at it
		///      directly. Requires the G-Buffer depth/normal/velocity to
		///      already be written.
		/// [JP] 実際の GPU 処理: DispatchRays を生テクスチャへ、続けて
		///      (useDlssRayReconstruction でなければ) ReflectionDenoiseCS.hlsl
		///      を今フレームの write スロットへディスパッチする(TLAS が無い/
		///      機能が無効/RTPSO・PSO が無ければ 0 でクリア)。write スロットは
		///      PIXEL_SHADER_RESOURCE 状態で終える。useDlssRayReconstruction が
		///      true の間はデノイズディスパッチと蓄積ピンポンを丸ごとスキップ
		///      する — 生テクスチャを PIXEL_SHADER_RESOURCE へ遷移させるだけで
		///      よい(PrepareFrame() が既に合成シェーダの参照先をそこへ直接
		///      向けているため)。G-Buffer の深度/法線/速度が書き込み済みで
		///      あることが前提。
		void Dispatch(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, Bool tlasValid, RaytracingView view, Bool useDlssRayReconstruction);

	private:
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

		/// [EN] Ping-ponged accumulated (denoised) radiance, one independent
		///      pair per view (see RaytracingView) — same scheme as
		///      GlobalIlluminationRenderer's accumulated radiance.
		/// [JP] ピンポン方式の蓄積(デノイズ済み)放射輝度。ビューごと
		///      (RaytracingView 参照)に独立した1ペア — GlobalIlluminationRenderer
		///      の蓄積放射輝度と同じ方式。
		Microsoft::WRL::ComPtr<ID3D12Resource> accumulatedRadianceResource_[viewCount][accumulationSlotCount];
		D3D12_RESOURCE_STATES accumulatedRadianceState_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedUnorderedAccessViewIndex_[viewCount][accumulationSlotCount] = {};
		Uint32 accumulatedShaderResourceViewIndex_[viewCount][accumulationSlotCount] = {};

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
