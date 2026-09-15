#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <FoundationEngine/Log/Assert.h>
#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Model/Transparent/OITBuffer.h>
#include <GraphicsEngine/Model/ModelCullingBuffer.h>
#include <GraphicsEngine/Model/ModelRecord.h>
#include <GraphicsEngine/Model/SoftbodyMesh.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	struct RootAddresses;
	struct LoaderSystem;
	class ModelResource;
	class MaterialResource;
	class AnimationResource;
	class Crister;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class ConstantIndicesSystem;
	class ShaderResourceIndicesSystem;
	class UnorderedAccessIndicesSystem;
	class D3D12CommandList;
	class FrameBuffer;
	class GeometryBuffer;

	/**
	* [EN]
	* Where the shell-fur forward pass' fur instances live in the shared
	* instance buffer. Mirrors the HLSL FurConstantBuffer
	* (Model/Model.hlsli); 16 bytes. Reached through
	* ConstantIndices::furIndex_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シェルファー前方パスのファーインスタンスが共有インスタンスバッファの
	* どこにあるか。HLSL の FurConstantBuffer（Model/Model.hlsli）と
	* 一致。16 バイト。ConstantIndices::furIndex_
	* 経由で参照する。
	*/
	struct FurConstantBuffer
	{
		Uint furInstanceOffset_ = 0;
		Uint furInstanceCount_ = 0;
		Vector2 furConstantBufferPadding0_;
	};
	SC_STATIC_ASSERT_SIZE(FurConstantBuffer, 16, "Model/Model.hlsli");

	class SEEDCORE_API ModelRenderer
	{
	public:
		ModelRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ModelRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ConstantIndicesSystem& constantIndicesSystem, ShaderResourceIndicesSystem& shaderResourceIndicesSystem, UnorderedAccessIndicesSystem& unorderedAccessIndicesSystem, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ConstantIndicesSystem& constantIndicesSystem, UnorderedAccessIndicesSystem& unorderedAccessIndicesSystem, Uint32 width, Uint32 height);

		/// [EN] scene supplies the camera for CPU-side LOD desirability (same
		///      screen-space error metric as the AS) driving geometry streaming.
		/// [JP] scene は CPU 側 LOD 要求判定（AS と同じスクリーン誤差式）用の
		///      カメラを供給し、ジオメトリストリーミングを駆動する。
		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, MaterialResource& materialResource, AnimationResource& animationResource, World& world, const SceneConstantBuffer& scene, Entity selectedEntity = Entity::Null());

		void Upload();

		void DrawDepthPrepass(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawOpaque(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void Compose(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		/// [EN] Wireframe overlay: draws opaque instances as wire over the composed
		///      frame buffer, depth-tested (read-only) against the scene depth.
		/// [JP] ワイヤーフレーム オーバーレイ: 合成済みフレームバッファ上に不透明
		///      インスタンスをワイヤーで描く。シーン深度で読み取りのみ深度テスト。
		void DrawWireframe(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		/// [EN] Meshlet visualization: flat-colors each meshlet, depth-tested against
		///      the scene depth. Editor view-mode only.
		/// [JP] メッシュレット可視化: メッシュレットごとに単色塗り、シーン深度で
		///      深度テスト。エディタ表示モード専用。
		void DrawMeshlet(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawTransparent(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawFurShell(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		/// [EN] Silhouette: draws the selected actor's instances as a
		///      solid mask (single R8_UNORM target, shared with Sprite/Billboard/
		///      Font — see OutlineRenderer::Draw for the shared fullscreen
		///      edge-detect composite that reads it). Depth off: the mask covers
		///      the full silhouette regardless of nearer, unselected occluders —
		///      see the Silhouette PSO comment in ModelShader.cpp for why.
		/// [JP] シルエット: 選択中アクターのインスタンスを単色マスク
		///      （単一 R8_UNORM ターゲット。Sprite/Billboard/Font と共有 — これを
		///      読むフルスクリーンのエッジ検出合成は OutlineRenderer::Draw
		///      参照）へ描く。深度オフで、手前の未選択オブジェクトに関わらず
		///      シルエット全体を描く（理由は ModelShader.cpp の Silhouette PSO
		///      コメント参照）。
		void DrawSilhouette(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS BoneMatrixBufferGPUAddress()const;

		[[nodiscard]] Bool TryGetAnimatedBoneOffset(EntityID entityID, Uint32& outBoneOffset)const;

		/// [EN] Looks up this frame's sampled morph target weights for one
		///      node of one entity (Animation::weights_[nodeIndex], sampled
		///      by SampleMorphWeights during Gather()). Returns false when
		///      the entity has no Animator-driven weights this frame, or
		///      that specific node has none.
		/// [JP] このフレームでサンプリング済みの、あるエンティティのある
		///      ノードに対するモーフターゲットウェイトを引く
		///      (Animation::weights_[nodeIndex]、Gather() 中に
		///      SampleMorphWeights でサンプリング済み)。今フレーム
		///      Animator 駆動のウェイトが無い、またはそのノードには無い
		///      場合 false。
		[[nodiscard]] Bool TryGetAnimatedMorphWeights(EntityID entityID, Int nodeIndex, DynamicArray<Float>& outWeights)const;

	private:
		DynamicArray<ModelStructuredBuffer> opaqueInstances_;
		DynamicArray<ModelStructuredBuffer> transparentInstances_;

		/// [EN] Copies of the opaque instances whose material is ShadingModel::Fur -
		///      the shell-fur forward pass draws each of these fur_shell_count_ times.
		/// [JP] マテリアルが ShadingModel::Fur の不透明インスタンスのコピー -
		///      シェルファー前方パスがこれらを fur_shell_count_ 回ずつ描画する。
		DynamicArray<ModelStructuredBuffer> furInstances_;
		static constexpr Uint32 furShellMax_ = 32;

		ResourcePtr<ConstantBuffer<FurConstantBuffer>> modelFurConstantBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<ModelStructuredBuffer>> instanceBuffer_;

		/// [EN] Bone palette (inverse bind matrix × joint global transform), rebuilt
		///      each Gather and uploaded each Upload. Skinned instances index into
		///      this via boneOffset_.
		/// [JP] ボーンパレット（逆バインド行列 × ジョイントのグローバルトランスフォーム）。
		///      毎 Gather で再構築し、毎 Upload でアップロードする。スキンインスタンスは
		///      boneOffset_ でこれを参照する。
		DynamicArray<Matrix> boneMatrices_;

		DynamicArray<Matrix> previousBoneMatrices_;

		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> boneBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> previousBoneBuffer_;

		std::unordered_map<EntityID, Uint32> animatedBoneOffsets_;

		std::unordered_map<EntityID, DynamicArray<Matrix>> animatedBonePalettes_;

		std::unordered_map<EntityID, DynamicArray<Matrix>> previousAnimatedBonePalettes_;

		/// [EN] Shared per-frame morph target weight buffer (read by the
		///      raster morph blend in the model mesh shaders and
		///      Model/Material/MaterialResolveCS.hlsl/
		///      Model/Transparent/ModelTransparentPS.hlsl): rebuilt each Gather (every morphed
		///      instance's sampled weights appended back to back) and
		///      uploaded each Upload. A morphed ModelStructuredBuffer indexes
		///      into this via morphWeightOffset_.
		/// [JP] 共有の毎フレームモーフターゲットウェイトバッファ
		///      (モデル用メッシュシェーダーと Model/Material/MaterialResolveCS.hlsl/
		///      Model/Transparent/ModelTransparentPS.hlsl が読む): 毎 Gather で再構築し
		///      (モーフ付きインスタンスのサンプリング済みウェイトを連続して
		///      詰める)、毎 Upload でアップロードする。モーフ付き
		///      ModelStructuredBuffer は morphWeightOffset_ でこれを参照する。
		DynamicArray<Float> morphWeights_;

		DynamicArray<Float> previousMorphWeights_;

		ResourcePtr<ReadOnlyStructuredBuffer<Float>> morphWeightBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<Float>> previousMorphWeightBuffer_;

		/// [EN] This frame's sampled morph target weights, keyed by entity
		///      then by target NODE index (Animation::weights_'s own key) —
		///      one row per that node's mesh's morph target count, in the
		///      mesh's own target order. Rebuilt each Gather via
		///      Animation::SampleMorphWeights; empty for an entity with no
		///      morph-driving Animator this frame.
		/// [JP] このフレームでサンプリング済みのモーフターゲットウェイト。
		///      エンティティ、次に対象ノード番号(Animation::weights_ 自身の
		///      キー)でキー付けする — そのノードのメッシュのモーフ
		///      ターゲット数ぶん、メッシュ自身のターゲット順で1行。毎
		///      Gather で Animation::SampleMorphWeights により再構築する。
		///      今フレームモーフを駆動する Animator が無いエンティティでは
		///      空。
		std::unordered_map<EntityID, std::unordered_map<Int, DynamicArray<Float>>> animatedMorphWeights_;

		std::unordered_map<EntityID, std::unordered_map<Int, DynamicArray<Float>>> previousAnimatedMorphWeights_;

		struct AnimatorHistory
		{
			Int stateIndex_ = -1;
			Float time_ = 0.0f;
		};

		std::unordered_map<EntityID, AnimatorHistory> animatorHistories_;

		std::unordered_map<EntityID, AnimatorHistory> previousAnimatorHistories_;

		/// [EN] Each entity's world matrix as of the previous Gather() call -
		///      used to give ModelStructuredBuffer::previousWorld_ (StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl's velocity output) the instance's OWN motion,
		///      not just the camera's. Read before this frame's worldMatrix
		///      overwrites the entry, written back after (see Gather()). An
		///      entity seen for the first time falls back to its current
		///      worldMatrix (zero velocity on spawn, not a garbage jump from a
		///      default-constructed identity).
		/// [JP] 各エンティティの、直近の Gather() 時点でのワールド行列 —
		///      ModelStructuredBuffer::previousWorld_(StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl の速度出力)に、カメラだけでなく
		///      インスタンス自身の動きを反映させるために使う。今フレームの
		///      worldMatrix で上書きする前に読み、後で書き戻す(Gather() 参照)。
		///      初めて見るエンティティは今フレームの worldMatrix にフォール
		///      バックする(デフォルト構築の単位行列からの巨大な誤ジャンプでは
		///      なく、出現時は速度ゼロにする)。
		std::unordered_map<EntityID, Matrix> previousWorldMatrices_;

		/// [EN] One SoftbodyMesh per Softbody-bearing Actor, built once
		///      (SoftbodyMesh::Create) the first time that Actor is seen and
		///      re-quantised every Gather (SoftbodyMesh::Update) — see
		///      SoftbodyMesh's class comment for why Softbody bypasses the
		///      Crister cluster/LOD streaming pipeline entirely instead of
		///      reusing ModelStructuredBuffer's usual vertexBufferIndex_ path.
		/// [JP] Softbody を持つ Actor ごとに 1 つの SoftbodyMesh。その Actor を
		///      初めて見た時に一度だけ構築し（SoftbodyMesh::Create）、毎
		///      Gather で再量子化する（SoftbodyMesh::Update）— Softbody が
		///      ModelStructuredBuffer の通常の vertexBufferIndex_ 経路
		///      （Crister のクラスタ/LOD ストリーミングパイプライン）を
		///      使わずに完全にバイパスする理由は SoftbodyMesh のクラス
		///      コメント参照。
		std::unordered_map<EntityID, ResourcePtr<SoftbodyMesh>> softbodyMeshes_;

		Bool hasSkinnedOpaque_ = false;
		Bool hasSkinnedTransparent_ = false;
		Bool hasSelectedInstance_ = false;
		Bool hasSelectedSkinned_ = false;
		Bool uploaded_ = false;

		ModelShader modelShader_;
		OITBuffer oitBuffer_;
		ModelCullingBuffer modelCullingBuffer_;

		ID3D12Device* device_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;
		ConstantIndicesSystem* constantIndicesSystem_ = nullptr;
		ShaderResourceIndicesSystem* shaderResourceIndicesSystem_ = nullptr;

		Uint maxInstanceCount_ = 0;
		Uint maxBoneCount_ = 0;
		Uint maxMorphWeightCount_ = 0;

		/// [EN] Geometry streaming state: frame counter for the eviction age
		///      guard and this frame's page requests (deduplicated, capped per
		///      frame to avoid upload hitches).
		/// [JP] ジオメトリストリーミング状態: 追い出し経過フレームガード用の
		///      フレームカウンタと、今フレームのページ要求（重複除去・アップロード
		///      ヒッチ防止のためフレームあたり上限あり）。
		struct TextureStreamingRequest
		{
			Crister* crister_ = nullptr;
			Uint32 textureIndex_ = 0;
			Uint32 desiredMip_ = 0;
		};

		Uint64 streamingFrame_ = 0;
		DynamicArray<std::pair<Crister*, Uint32>> geometryStreamingRequests_;
		DynamicArray<TextureStreamingRequest> textureStreamingRequests_;
	};
}
