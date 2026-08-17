#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <GraphicsEngine/Raytracing/BottomLevelAccelerationStructure.h>
#include <GraphicsEngine/Raytracing/TopLevelAccelerationStructure.h>
#include <GraphicsEngine/D3D12/FrameRing.h>
#include <GraphicsEngine/Renderer/ShadowRenderer.h>
#include <GraphicsEngine/Renderer/AmbientOcclusionRenderer.h>
#include <GraphicsEngine/Renderer/SubsurfaceScatteringRenderer.h>
#include <GraphicsEngine/Renderer/ReflectionRenderer.h>
#include <GraphicsEngine/Renderer/RefractionRenderer.h>
#include <GraphicsEngine/Renderer/GlobalIlluminationRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricCloudScapesRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricStarRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricLightRenderer.h>
#include <GraphicsEngine/Renderer/WeatherParticleRenderer.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/Model/SkinnedPositionShader.h>
#include <GraphicsEngine/Model/Morph/MorphBlendShader.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ModelResource;
	class World;
	class Crister;
	class BindlessHeap;
	class D3D12CommandList;
	class ShaderCache;
	class IndicesSystem;
	class RootSignature;
	class PipelineStateObject;
	class ModelRenderer;
	class FrameBuffer;
	class GeometryBuffer;

	/**
	* [EN]
	* Owns the scene's raytracing acceleration structures, shared by every RT
	* effect (shadow, AO, GI, reflection, ...): one BLAS per unique mesh asset
	* (built once and cached — bind pose only, no per-frame skinning deformation
	* yet since there is no animation system), and one TLAS rebuilt every frame
	* from the current instance transforms.
	*
	* Deliberately independent from ModelRenderer's Gather (which splits
	* instances into meshlet/LOD dispatch batches for rasterization): this class
	* only needs one (Crister*, world matrix) pair per actor, so it does its own
	* minimal ECS traversal.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーンのレイトレーシング アクセラレーション構造を保持する。全 RT 機能
	* （シャドウ、AO、GI、反射...）で共有する: メッシュアセットごとに BLAS を
	* 1 つ（1 度構築してキャッシュ — アニメーションシステムがまだ無いため
	* バインドポーズのみ、フレームごとのスキン変形は無し）、TLAS は現在の
	* インスタンス変換から毎フレーム再構築する。
	*
	* ModelRenderer の Gather（ラスタライズ用にメシュレット/LOD ディスパッチ
	* バッチへ分割する）とは意図的に独立させている: このクラスはアクターごとに
	* (Crister*, ワールド行列) の組が 1 つあればよいため、独自の最小限の ECS
	* 走査を行う。
	*
	* Also owns the individual RT-effect renderers that consume the TLAS
	* (currently just ShadowRenderer; AO/GI/reflection would join here later),
	* so Renderer only ever talks to this one class for anything raytraced.
	* 併せて、TLAS を消費する個々の RT エフェクトのレンダラーも保持する
	* （現状は ShadowRenderer のみ。将来 AO/GI/反射もここに加わる想定）。
	* これにより Renderer はレイトレ関連について常にこのクラス 1 つとだけ話す。
	*/
	class RaytracingRenderer
	{
	public:
		RaytracingRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, RaytracingStateObject& raytracingStateObject);
		~RaytracingRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		/// [EN] Resizes every screen-space RT-effect buffer (Shadow/AO/SSS/
		///      Reflection/GI/VolumetricCloudScapes) to the new native
		///      resolution. VolumetricLightRenderer is intentionally excluded —
		///      its froxel volumes are a fixed 160x90x64 grid, independent of
		///      screen resolution. The BLAS/TLAS caches and tlasBindlessIndices_
		///      are also untouched — acceleration structures are geometry-sized,
		///      not screen-sized.
		/// [JP] 全スクリーン空間RTエフェクトバッファ(Shadow/AO/SSS/Reflection/
		///      GI/VolumetricCloudScapes)を新しいネイティブ解像度でリサイズする。
		///      VolumetricLightRenderer は対象外 — froxelボリュームは固定
		///      160x90x64グリッドで画面解像度に依存しない。BLAS/TLASキャッシュと
		///      tlasBindlessIndices_ も対象外 — 加速構造はジオメトリサイズであり
		///      画面サイズではない。
		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Collects (Crister*, world matrix) pairs for every active Mesh
		///      actor. Does not touch the GPU.
		/// [JP] 全ての有効な Mesh アクターについて (Crister*, ワールド行列) の
		///      組を収集する。GPU には触れない。
		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, World& world, const ModelRenderer& modelRenderer);

		/// [EN] Builds any BLAS not yet cached for meshes seen this Gather, then
		///      rebuilds the TLAS from the collected instances and refreshes its
		///      bindless SRV for the current frame-ring slot. Also registers the
		///      TLAS index into IndicesSystem and preps every owned RT-effect
		///      renderer's per-frame constant data (currently just
		///      ShadowRenderer::PrepareFrame) — all of this has no G-Buffer
		///      dependency, so it must run before IndicesSystem::UploadEditor
		///      bakes this frame's structured indices (see Renderer::EditorFlush).
		/// [JP] 今回の Gather で見つかった未キャッシュの BLAS を構築し、収集した
		///      インスタンスから TLAS を再構築、現在のフレームリングスロット用の
		///      bindless SRV を更新する。併せて TLAS インデックスを IndicesSystem
		///      へ登録し、保持している各 RT エフェクトレンダラーの毎フレーム定数
		///      データも準備する（現状は ShadowRenderer::PrepareFrame のみ）—
		///      これらはすべて G-Buffer に依存しないため、IndicesSystem::
		///      UploadEditor が今フレームの structured indices を確定する前に
		///      実行する必要がある（Renderer::EditorFlush 参照）。
		/// [EN] deltaTime/nightFactor feed VolumetricStarRenderer::PrepareFrame
		///      (shooting star spawn/lifetime advance). cameraPosition/
		///      totalTime/snowIntensity additionally feed
		///      WeatherParticleRenderer::PrepareFrame (particle recycling
		///      volume follows the camera, snowIntensity_ is WeatherSystem's
		///      fast "is it snowing now" signal - see Environment/Weather.h).
		/// [JP] deltaTime/nightFactor は VolumetricStarRenderer::PrepareFrame
		///      (流れ星のスポーン/寿命進行)へ渡す。cameraPosition/totalTime/
		///      snowIntensity はさらに WeatherParticleRenderer::PrepareFrame
		///      へ渡す(パーティクルの再スポーンボリュームはカメラに追従、
		///      snowIntensity は WeatherSystem の「今降っているか」の速い信号
		///      - Environment/Weather.h 参照)。
		void Build(D3D12CommandList* cmdList, ID3D12Device* device, const ModelRenderer& modelRenderer, Float deltaTime, Float nightFactor, const Vector3& cameraPosition, Float totalTime, Float snowIntensity);

		/// [EN] The actual shadow ray GPU work (or fallback clear if the TLAS
		///      isn't valid this frame) — requires the G-Buffer depth/normal to
		///      already be written, so it runs later than Build().
		/// [JP] 実際のシャドウレイ GPU 処理（今フレーム TLAS が無効ならフォール
		///      バッククリア）— G-Buffer の深度/法線が書き込み済みである必要が
		///      あるため、Build() より後に実行する。
		void DispatchShadow(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view);

		/// [EN] Same contract as DispatchShadow, for the AO pass.
		/// [JP] DispatchShadow と同じ契約。AO パス用。
		void DispatchAmbientOcclusion(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view);

		/// [EN] Same contract, for the SSS pass. No view parameter: the pass
		///      is deterministic (no temporal accumulation), so one shared
		///      transmittance texture is written and consumed per flush.
		/// [JP] 同じ契約の SSS パス用。ビュー引数は無し: このパスは決定論的
		///      (時間積分なし)なので、共有の透過率テクスチャ1枚を Flush ごとに
		///      書いて読む。
		void DispatchSubsurfaceScattering(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Same contract, for the reflection RTPSO pass. GGX-sampled and
		///      denoised per-view (like AO/GI), so it now takes a view
		///      parameter too — no longer deterministic once roughness > 0.
		/// [JP] 同じ契約の反射 RTPSO パス用。GGXサンプリング+ビューごとの
		///      デノイズ(AO/GIと同様)になったため、こちらもビュー引数を取る —
		///      roughness > 0 では決定論的ではなくなった。
		void DispatchReflection(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view);

		/// [EN] Same contract, for the refraction RTPSO pass. Deterministic
		///      (Snell-refracted ray per pixel, no importance sampling/denoiser)
		///      like SSS, so no view parameter either - one shared output.
		/// [JP] 同じ契約の屈折 RTPSO パス用。SSSと同様に決定論的(ピクセルごと
		///      Snell屈折レイ1本、重点サンプリング/デノイザ無し)なので、
		///      こちらもビュー引数は無し - 共有の出力1枚。
		void DispatchRefraction(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Same contract, for the volumetric cloud pass. Needs no TLAS
		///      at all (pure raymarch), so its enabled flag is the only gate.
		/// [JP] 同じ契約の雲パス用。TLAS を一切使わない(純レイマーチ)ので、
		///      有効フラグだけがゲート。
		/// [EN] Same contract, for the one-bounce diffuse GI RTPSO pass. Also
		///      view-shared, so no view parameter.
		/// [JP] 同じ契約の1バウンス拡散GI RTPSO パス用。こちらもビュー共有なので
		///      ビュー引数は無し。
		void DispatchGlobalIllumination(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view);

		void DispatchVolumetricCloudScapes(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Same contract, for the star/moon/shooting-star pass. Needs no
		///      TLAS (pure screen-space), so its enabled flag is the only gate.
		/// [JP] 同じ契約の星/月/流れ星パス用。TLAS を使わない(純スクリーン空間)
		///      ので、有効フラグだけがゲート。
		void DispatchVolumetricStar(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] The rain/snow particle compute simulate pass. Needs no TLAS or
		///      G-Buffer, so - like clouds/star - it can run any time, but
		///      should only run ONCE per frame (not once per view) since it
		///      advances one shared world-space simulation; Renderer calls this
		///      only from EditorFlush, never GameFlush.
		/// [JP] 雨/雪パーティクルのコンピュート・シミュレートパス。TLAS も
		///      G-Buffer も不要(雲/星と同じ)なのでいつ実行してもよいが、
		///      共有のワールド空間シミュレーションを1つ進めるだけなので
		///      フレームに1回だけ実行すること(ビューごとではない) -
		///      Renderer は EditorFlush からのみ呼び、GameFlush からは呼ばない。
		void SimulateWeatherParticles(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] The rain/snow particle mesh-shader draw pass - unlike Simulate,
		///      this DOES run once per view (Editor/Game), each from its own
		///      camera. Requires the G-Buffer depth already written and the
		///      caller to be inside a geometryBuffer->BeginDepth()/EndDepth()
		///      scope - see WeatherParticleRenderer::Draw.
		/// [JP] 雨/雪パーティクルのメッシュシェーダ描画パス - Simulate とは違い、
		///      こちらはビューごと(Editor/Game)に1回ずつ、それぞれ自分の
		///      カメラで実行する。G-Buffer の深度が書き込み済みで、呼び出し側が
		///      geometryBuffer->BeginDepth()/EndDepth() スコープ内であることが
		///      前提 - WeatherParticleRenderer::Draw 参照。
		void DrawWeatherParticles(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Same contract, for the froxel fog / volumetric light pipeline
		///      (god rays). Uses the TLAS when present but degrades gracefully
		///      without it (fog only, no geometry occlusion), so only the
		///      enabled flag gates it.
		/// [JP] 同じ契約の froxel フォグ/体積光パイプライン(ゴッドレイ)用。
		///      TLAS があれば使うが無くても動く(ジオメトリ遮蔽なしのフォグのみ)
		///      ので、ゲートは有効フラグのみ。
		void DispatchVolumetricLight(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Per-effect settings are copied into each renderer's tuning
		///      constant buffer every frame; the enabled flags are master
		///      on/off switches — when every RT effect is disabled, Build()
		///      skips BLAS/TLAS construction entirely (not just zeroing the
		///      shader-side effects), so disabling everything costs nothing on
		///      the GPU. A disabled individual effect skips its own dispatch
		///      (falls back to the fully-lit/open clear).
		/// [JP] 各エフェクトの設定は毎フレームそれぞれのチューニング用定数
		///      バッファへコピーされる。enabled フラグはマスターオンオフ —
		///      全 RT エフェクトが無効の間は Build() が BLAS/TLAS 構築そのものを
		///      スキップする(シェーダ側の効果をゼロにするだけではない)ので、
		///      全部無効なら GPU コストがかからない。個別に無効なエフェクトは
		///      自分のディスパッチだけスキップする(全照射/全開放クリアへの
		///      フォールバック)。
		void SetRaytracingSettings(const RaytracingContext& settings);

		/// [EN] Bindless index of the current frame's TLAS SRV
		///      (D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE), or
		///      0xFFFFFFFF if nothing was built this frame (empty scene).
		/// [JP] 現在フレームの TLAS SRV（D3D12_SRV_DIMENSION_RAYTRACING_
		///      ACCELERATION_STRUCTURE）の bindless インデックス。今フレーム何も
		///      構築されていなければ(シーンが空)0xFFFFFFFF。
		[[nodiscard]] Uint32 TLASBindlessIndex()const;

	private:
		struct PendingInstance
		{
			const Crister* crister_ = nullptr;
			Matrix worldMatrix_ = Matrix::Identity;
			EntityID entityID_ = 0;
			Bool hasSkeletalPose_ = false;
			Uint32 boneOffset_ = 0;

			/// [EN] Sampled morph weights for this frame, indexed the same
			///      way as crister_->SubMeshes() — morphWeights_[subMeshIndex]
			///      is empty when that SubMesh has no morphs_ or no
			///      Animator-driven weights this frame. hasMorphWeights_ is
			///      true when at least one entry is non-empty, gating the
			///      whole morph blend/BLAS path for this instance.
			/// [JP] このフレームでサンプリング済みのモーフウェイト。
			///      crister_->SubMeshes() と同じインデックスで
			///      morphWeights_[subMeshIndex] を引く — その SubMesh に
			///      morphs_ が無いか、今フレーム Animator 駆動のウェイトが
			///      無ければ空。hasMorphWeights_ は1つでも非空のエントリが
			///      あれば true — このインスタンスのモーフブレンド/BLAS
			///      経路全体のゲートになる。
			DynamicArray<DynamicArray<Float>> morphWeights_;
			Bool hasMorphWeights_ = false;
		};

		struct SkinnedPositionBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
			Uint32 capacity_ = 0;
		};

		/// [EN] Per (entity, SubMesh) UPLOAD-heap buffer of this frame's
		///      morph target weights (one float per target, mapped once and
		///      memcpy'd into each frame — see morphWeightBuffers_), bound
		///      as MorphBlendCS's morph_weights SRV (root descriptor, no
		///      bindless heap registration needed).
		/// [JP] (エンティティ, SubMesh) ごとの、今フレームのモーフターゲット
		///      ウェイト(ターゲットごとに float 1つ)を持つ UPLOAD ヒープ
		///      バッファ(一度だけ Map し毎フレーム memcpy —
		///      morphWeightBuffers_ 参照)。MorphBlendCS の morph_weights
		///      SRV(ルートディスクリプタ、bindless ヒープ登録不要)として
		///      束縛する。
		struct MorphWeightBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
			void* mappedPtr_ = nullptr;
			Uint32 capacity_ = 0;
		};

		/// [EN] Per-Crister GPU tables consumed by
		///      Reflection.hlsli::ResolveReflectionMaterial - the materials
		///      array (Crister::Materials(), or one white fallback entry if
		///      empty) and the per-triangle index into it (built from every
		///      SubMesh's materialIndex_/indexOffset_/indexCount_). Both live
		///      on an UPLOAD heap and are written once via Map/memcpy: they are
		///      small, built once per unique mesh (cached like the BLAS below,
		///      never rebuilt per-frame), and read rarely enough (one lookup
		///      per ray hit) that the DEFAULT-heap-plus-copy dance is not worth
		///      it here.
		/// [JP] Reflection.hlsli::ResolveReflectionMaterial が読む、Crister
		///      ごとの GPU テーブル — マテリアル配列(Crister::Materials()、
		///      無ければ白1件のフォールバック)と、そこへの三角形ごとの
		///      インデックス(各 SubMesh の
		///      materialIndex_/indexOffset_/indexCount_ から構築)。どちらも
		///      UPLOAD ヒープに置き Map/memcpy で一度だけ書く — 小さく、
		///      ユニークなメッシュごとに一度しか構築せず(下の BLAS と同じく
		///      キャッシュ、毎フレーム再構築しない)、読み取り頻度もレイが
		///      当たった時に1回程度なので、DEFAULT ヒープ+コピーの手間を
		///      掛ける価値がない。
		struct ReflectionMaterialTable
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> materialsResource_;
			Uint32 materialsShaderResourceViewIndex_ = 0;

			Microsoft::WRL::ComPtr<ID3D12Resource> triangleMaterialIndexResource_;
			Uint32 triangleMaterialIndexShaderResourceViewIndex_ = 0;
		};

		/// [JP] crister が初出のときだけ構築する(BLAS と同じライフサイクル —
		///      pendingBlasBuilds_ のループに相乗りする)。
		void BuildReflectionMaterialTable(ID3D12Device* device, const Crister* crister);

		std::unordered_map<const Crister*, ReflectionMaterialTable> reflectionMaterialTableCache_;

		std::unordered_map<const Crister*, ResourcePtr<BottomLevelAccelerationStructure>> blasCache_;

		std::unordered_map<EntityID, ResourcePtr<BottomLevelAccelerationStructure>> skinnedBlasCache_[FrameRing::frameCount];
		std::unordered_map<EntityID, SkinnedPositionBuffer> skinnedPositionBuffers_[FrameRing::frameCount];
		SkinnedPositionShader skinnedPositionShader_;

		/// [EN] Morph blend scratch positions (base rt_positions with
		///      active SubMeshes' vertex ranges overwritten by
		///      MorphBlendCS), one per morphed instance per frame-ring
		///      slot. Feeds SkinnedPositionCS's input in place of
		///      crister_->PositionBufferAddress() when an instance is
		///      both morphed and skinned (morph composes before skin), and
		///      feeds morphedBlasCache_'s BLAS build directly when an
		///      instance is morphed but not skinned.
		/// [JP] モーフブレンド用の一時位置(ベースの rt_positions に、
		///      有効な SubMesh の頂点範囲だけ MorphBlendCS が上書きした
		///      もの)。モーフのあるインスタンスごと・フレームリング
		///      スロットごとに1つ。インスタンスがモーフとスキンの両方を
		///      持つ場合(モーフはスキンより前に合成)、
		///      crister_->PositionBufferAddress() の代わりに
		///      SkinnedPositionCS の入力として使う。モーフのみでスキン無し
		///      の場合は、直接 morphedBlasCache_ の BLAS 構築に使う。
		std::unordered_map<EntityID, SkinnedPositionBuffer> morphedPositionBuffers_[FrameRing::frameCount];

		/// [EN] This frame's per-(entity, SubMesh) morph weight upload
		///      buffers — outer index matches crister_->SubMeshes(), same
		///      shape as PendingInstance::morphWeights_.
		/// [JP] このフレームの (エンティティ, SubMesh) ごとのモーフウェイト
		///      アップロードバッファ — 外側のインデックスは
		///      crister_->SubMeshes() と対応し、PendingInstance::
		///      morphWeights_ と同じ形。
		std::unordered_map<EntityID, DynamicArray<MorphWeightBuffer>> morphWeightBuffers_[FrameRing::frameCount];

		/// [EN] BLAS for a morphed-but-not-skinned instance, built directly
		///      from morphedPositionBuffers_ (no SkinnedPositionCS pass
		///      involved). Parallel cache to skinnedBlasCache_.
		/// [JP] モーフはあるがスキン無しのインスタンス用 BLAS。
		///      morphedPositionBuffers_ から直接構築する
		///      (SkinnedPositionCS パスは介さない)。skinnedBlasCache_ と
		///      並列のキャッシュ。
		std::unordered_map<EntityID, ResourcePtr<BottomLevelAccelerationStructure>> morphedBlasCache_[FrameRing::frameCount];

		MorphBlendShader morphBlendShader_;

		/// [EN] One entry per active Mesh actor, collected by Gather(). The BLAS
		///      address isn't resolved until Build(), once blasCache_ is caught up.
		/// [JP] 有効な Mesh アクターごとに 1 エントリ、Gather() が収集する。BLAS
		///      アドレスは blasCache_ が追いつく Build() まで解決しない。
		DynamicArray<PendingInstance> pendingInstances_;

		/// [EN] Meshes seen this Gather that are not yet in blasCache_ — built at
		///      the start of Build() (needs a command list, Gather doesn't have one).
		/// [JP] 今回の Gather で見つかったが blasCache_ に未登録のメッシュ —
		///      Build() の先頭で構築する（コマンドリストが要るため Gather では行わない）。
		DynamicArray<const Crister*> pendingBlasBuilds_;

		TopLevelAccelerationStructure tlas_;

		ResourcePtr<ShadowRenderer> shadowRenderer_;
		ShadowRayConstantBuffer shadowSettings_;
		Bool shadowEnabled_ = true;

		ResourcePtr<AmbientOcclusionRenderer> ambientOcclusionRenderer_;
		AmbientOcclusionRayConstantBuffer ambientOcclusionSettings_;
		Bool ambientOcclusionEnabled_ = false;

		ResourcePtr<SubsurfaceScatteringRenderer> subsurfaceScatteringRenderer_;
		SubsurfaceScatteringRayConstantBuffer subsurfaceScatteringSettings_;
		Bool subsurfaceScatteringEnabled_ = false;

		ResourcePtr<ReflectionRenderer> reflectionRenderer_;
		ReflectionRayConstantBuffer reflectionSettings_;
		Bool reflectionEnabled_ = false;

		ResourcePtr<RefractionRenderer> refractionRenderer_;
		RefractionRayConstantBuffer refractionSettings_;
		Bool refractionEnabled_ = false;

		ResourcePtr<GlobalIlluminationRenderer> globalIlluminationRenderer_;
		GlobalIlluminationRayConstantBuffer globalIlluminationSettings_;
		Bool globalIlluminationEnabled_ = false;

		ResourcePtr<VolumetricCloudScapesRenderer> volumetricCloudScapesRenderer_;
		VolumetricCloudScapesRayConstantBuffer volumetricCloudScapesSettings_;
		Bool volumetricCloudScapesEnabled_ = false;

		ResourcePtr<VolumetricStarRenderer> volumetricStarRenderer_;
		VolumetricStarRayConstantBuffer volumetricStarSettings_;
		Bool volumetricStarEnabled_ = false;

		ResourcePtr<WeatherParticleRenderer> weatherParticleRenderer_;
		Bool rainEnabled_ = false;
		RainSettings rainSettings_;
		Bool snowEnabled_ = false;
		SnowSettings snowSettings_;

		ResourcePtr<VolumetricLightRenderer> volumetricLightRenderer_;
		VolumetricLightRayConstantBuffer volumetricLightSettings_;
		Bool volumetricLightEnabled_ = false;

		/// [EN] Global switch: DLSS Ray Reconstruction (whole-frame denoise+
		///      upscale, driven by DlssRayReconstructionRenderer in Renderer)
		///      vs. each effect's own per-pixel spatio-temporal denoiser. When
		///      true, Shadow/AO/GI each bypass their own denoise dispatch (see
		///      their PrepareFrame/Dispatch) instead of running both.
		/// [JP] グローバル切り替え: DLSS Ray Reconstruction(フレーム全体の
		///      デノイズ+アップスケール、Renderer の
		///      DlssRayReconstructionRenderer が駆動)か、各エフェクト自前の
		///      画素ごとの空間+時間デノイザか。true の間は Shadow/AO/GI が
		///      それぞれ自前デノイズのディスパッチをバイパスする(両方は
		///      走らせない — PrepareFrame/Dispatch 参照)。
		Bool dlssRayReconstructionEnabled_ = false;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint32 tlasBindlessIndices_[FrameRing::frameCount] = { 0xFFFFFFFF, 0xFFFFFFFF };

		Bool tlasBuiltThisFrame_ = false;

		/// [EN] Logs BLAS/TLAS build failures once instead of every frame — see
		///      Build().
		/// [JP] BLAS/TLAS 構築失敗のログを毎フレームでなく 1 度だけ出す — Build() 参照。
		Bool blasBuildFailureLogged_ = false;
		Bool tlasBuildFailureLogged_ = false;
		Bool skinnedBlasBuildFailureLogged_ = false;
		Bool morphedBlasBuildFailureLogged_ = false;

		/// [EN] Reports the device-removed reason once instead of every frame.
		/// [JP] デバイス削除の理由を毎フレームでなく 1 度だけ報告する。
		Bool deviceRemovedLogged_ = false;
	};
}
