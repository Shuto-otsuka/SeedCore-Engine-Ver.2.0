#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Model/ModelInstanceData.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/IndicesSystem.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ModelResource;
	class AnimationResource;
	class Crister;
	class BindlessHeap;
	class ShaderCache;
	class RootSignature;
	class PipelineStateObject;
	class D3D12CommandList;

	/**
	* [EN]
	* Fully self-contained single-model preview renderer for the Timeline
	* panel's 3D viewport. Deliberately shares NOTHING with ModelRenderer,
	* IndicesSystem, or ModelTransformRenderer — its own instance/bone
	* StructuredBuffers, its own FrameBuffer, its own SceneSystem, and its own
	* ConstantIndices/StructuredIndices constant buffers. This is not an
	* optimization choice: those buffers are frame-ring (one physical slot
	* per frame, re-written by every Update() call that frame), and the GPU
	* only reads their contents at draw-execution time — after the whole
	* frame's command list has been recorded. Reusing Editor/Game/Canvas's
	* shared slots would mean whichever view's Update() runs last silently
	* overwrites what EVERY view's already-recorded draw calls read when the
	* GPU actually executes them, corrupting Editor/Game/Canvas's camera and
	* model-instance data for that frame.
	*
	* Uses the unlit ModelPreviewPS (base color + emissive only) via
	* ModelShader::GetPipelineStatePreviewStatic/Skeletal — so it also never
	* needs to read the shared LightSystem/sky IBL state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Timelineパネルの3Dビューポート用、完全に自己完結した単体モデル
	* プレビューレンダラー。ModelRenderer、IndicesSystem、
	* ModelTransformRendererとは意図的に何も共有しない —
	* instance/boneのStructuredBuffer、FrameBuffer、SceneSystem、
	* ConstantIndices/StructuredIndices定数バッファ、すべて専有する。これは
	* 最適化上の選択ではない: これらのバッファはフレームリング(フレームあたり
	* 物理スロット1つで、そのフレーム中のUpdate()呼び出しのたびに上書きされる)
	* であり、GPUはフレーム全体のコマンドリストが記録し終わった後の描画実行
	* タイミングで初めて中身を読む。Editor/Game/Canvasの共有スロットを再利用
	* すると、最後にUpdate()を呼んだビューが、全ビューの記録済み描画コマンドが
	* GPU実行時に読む内容を黙って上書きしてしまい、そのフレームの
	* Editor/Game/Canvasのカメラやモデルインスタンスデータが壊れる。
	*
	* アンリットのModelPreviewPS(baseColor + emissiveのみ)を
	* ModelShader::GetPipelineStatePreviewStatic/Skeletal経由で使うため、
	* 共有LightSystem/空IBLの状態も読む必要が無い。
	*/
	class SEEDCORE_API TimelineRenderer :public NonCopyable
	{
	public:
		TimelineRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~TimelineRenderer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		/// [EN] Isolated single-model gather: bypasses ECS/World entirely,
		///      samples animationAssetId's pose onto meshAssetId's Crister at
		///      time, and rebuilds this renderer's own instance/bone arrays.
		/// [JP] 独立した単体モデルgather: ECS/Worldを一切経由せず、
		///      animationAssetIdのポーズをtime時点でmeshAssetIdのCristerへ
		///      サンプリングし、このレンダラー専有のinstance/bone配列を
		///      再構築する。
		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void Upload();

		void Begin(D3D12CommandList* cmdList);

		void Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene);

		void End(D3D12CommandList* cmdList);

		void RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ImGuiGPUHandle()const;

	private:
		ModelShader modelShader_;

		DynamicArray<ModelInstanceData> opaqueInstances_;
		DynamicArray<ModelInstanceData> transparentInstances_;
		DynamicArray<Matrix> boneMatrices_;

		ResourcePtr<ReadOnlyStructuredBuffer<ModelInstanceData>> instanceBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> boneBuffer_;

		Bool hasSkinnedOpaque_ = false;
		Bool uploaded_ = false;

		Uint maxInstanceCount_ = 0;
		Uint maxBoneCount_ = 0;

		Uint64 streamingFrame_ = 0;
		DynamicArray<std::pair<Crister*, Uint32>> streamingRequests_;

		BindlessHeap* bindlessHeap_ = nullptr;

		DescriptorHeap renderTargetViewHeap_;
		DescriptorHeap depthStencilViewHeap_;
		ResourcePtr<FrameBuffer> frameBuffer_;

		ResourcePtr<SceneSystem> sceneSystem_;

		ConstantIndices constantIndices_{};
		StructuredIndices structuredIndices_{};
		ResourcePtr<ConstantBuffer<ConstantIndices>> constantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<StructuredIndices>> structuredIndicesBuffer_;

		DescriptorHeap* imguiHeap_ = nullptr;
		Uint32 imguiShaderResourceViewIndex_ = 0;
	};
}
