#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Model/OITBuffer.h>
#include <GraphicsEngine/Model/ModelInstanceData.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ModelResource;
	class AnimationResource;
	class Crister;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class IndicesSystem;
	class D3D12CommandList;
	class FrameBuffer;
	class GeometryBuffer;

	class SEEDCORE_API ModelRenderer
	{
	public:
		ModelRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ModelRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height);

		/// [EN] scene supplies the camera for CPU-side LOD desirability (same
		///      screen-space error metric as the AS) driving geometry streaming.
		/// [JP] scene は CPU 側 LOD 要求判定（AS と同じスクリーン誤差式）用の
		///      カメラを供給し、ジオメトリストリーミングを駆動する。
		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, World& world, const SceneConstantBuffer& scene, Entity selectedEntity = Entity::Null());

		void Upload();

		void DrawDepthPrepass(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawOpaque(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void Compose(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Wireframe overlay: draws opaque instances as wire over the composed
		///      frame buffer, depth-tested (read-only) against the scene depth.
		/// [JP] ワイヤーフレーム オーバーレイ: 合成済みフレームバッファ上に不透明
		///      インスタンスをワイヤーで描く。シーン深度で読み取りのみ深度テスト。
		void DrawWireframe(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Meshlet visualization: flat-colors each meshlet, depth-tested against
		///      the scene depth. Editor view-mode only.
		/// [JP] メッシュレット可視化: メッシュレットごとに単色塗り、シーン深度で
		///      深度テスト。エディタ表示モード専用。
		void DrawMeshlet(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawTransparent(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		/// [EN] Selection outline mask: draws the selected actor's instances as a
		///      solid mask (single R8_UNORM target, shared with Sprite/Billboard/
		///      Font — see Renderer::DrawSelectionOutline for the shared fullscreen
		///      edge-detect composite that reads it). Depth off: the mask covers
		///      the full silhouette regardless of nearer, unselected occluders —
		///      see the SelectionMask PSO comment in ModelShader.cpp for why.
		/// [JP] 選択アウトラインマスク: 選択中アクターのインスタンスを単色マスク
		///      （単一 R8_UNORM ターゲット。Sprite/Billboard/Font と共有 — これを
		///      読むフルスクリーンのエッジ検出合成は Renderer::DrawSelectionOutline
		///      参照）へ描く。深度オフで、手前の未選択オブジェクトに関わらず
		///      シルエット全体を描く（理由は ModelShader.cpp の SelectionMask PSO
		///      コメント参照）。
		void DrawSelectionMask(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS BoneMatrixBufferGPUAddress()const;

		[[nodiscard]] Bool TryGetAnimatedBoneOffset(EntityID entityID, Uint32& outBoneOffset)const;

	private:
		DynamicArray<ModelInstanceData> opaqueInstances_;
		DynamicArray<ModelInstanceData> transparentInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<ModelInstanceData>> instanceBuffer_;

		/// [EN] Bone palette (inverse bind matrix × joint global transform), rebuilt
		///      each Gather and uploaded each Upload. Skinned instances index into
		///      this via boneOffset_.
		/// [JP] ボーンパレット（逆バインド行列 × ジョイントのグローバルトランスフォーム）。
		///      毎 Gather で再構築し、毎 Upload でアップロードする。スキンインスタンスは
		///      boneOffset_ でこれを参照する。
		DynamicArray<Matrix> boneMatrices_;

		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> boneBuffer_;

		std::unordered_map<EntityID, Uint32> animatedBoneOffsets_;

		/// [EN] Each entity's world matrix as of the previous Gather() call -
		///      used to give ModelInstanceData::previousWorld_ (StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl's velocity output) the instance's OWN motion,
		///      not just the camera's. Read before this frame's worldMatrix
		///      overwrites the entry, written back after (see Gather()). An
		///      entity seen for the first time falls back to its current
		///      worldMatrix (zero velocity on spawn, not a garbage jump from a
		///      default-constructed identity).
		/// [JP] 各エンティティの、直近の Gather() 時点でのワールド行列 —
		///      ModelInstanceData::previousWorld_(StaticModelMS.hlsl/
		///      SkeletalModelMS.hlsl の速度出力)に、カメラだけでなく
		///      インスタンス自身の動きを反映させるために使う。今フレームの
		///      worldMatrix で上書きする前に読み、後で書き戻す(Gather() 参照)。
		///      初めて見るエンティティは今フレームの worldMatrix にフォール
		///      バックする(デフォルト構築の単位行列からの巨大な誤ジャンプでは
		///      なく、出現時は速度ゼロにする)。
		std::unordered_map<EntityID, Matrix> previousWorldMatrices_;

		Bool hasSkinnedOpaque_ = false;
		Bool hasSkinnedTransparent_ = false;
		Bool hasSelectedInstance_ = false;
		Bool hasSelectedSkinned_ = false;
		Bool uploaded_ = false;

		ModelShader modelShader_;
		OITBuffer oitBuffer_;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint maxInstanceCount_ = 0;
		Uint maxBoneCount_ = 0;

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
