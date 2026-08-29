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
	class MaterialResource;
	class Crister;
	class BindlessHeap;
	class ShaderCache;
	class RootSignature;
	class PipelineStateObject;
	class D3D12CommandList;

	/**
	* [EN]
	* Self-contained single-model material preview renderer for the Material
	* Viewer panel's 3D viewport. Same isolation model as
	* ModelTransformRenderer (own instance/bone StructuredBuffers, FrameBuffer,
	* SceneSystem, constant buffers - see that class's comment for why the
	* frame-ring buffers can't be shared). Renders meshAssetId's mesh with one
	* Surface (the ".material" surfaceAssetId, or the mesh's own embedded
	* surface per submesh when 0) applied, at bind pose, via the unlit
	* ModelPreviewPS.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マテリアルビューアパネルの3Dビューポート用、自己完結した単体モデル
	* マテリアルプレビューレンダラー。ModelTransformRenderer と同じ隔離
	* モデル(instance/bone の StructuredBuffer、FrameBuffer、SceneSystem、
	* 定数バッファをすべて専有 - フレームリングバッファを共有できない理由は
	* そのクラスのコメント参照)。meshAssetId のメッシュを、1つの Surface
	* (".material" の surfaceAssetId、0 なら submesh ごとにメッシュ内蔵の
	* Surface)を適用してバインドポーズで、アンリットの ModelPreviewPS
	* で描画する。
	*/
	class SEEDCORE_API MaterialRenderer :public NonCopyable
	{
	public:
		MaterialRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~MaterialRenderer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, MaterialResource& materialResource, Uint32 meshAssetId, Uint32 surfaceAssetId, const Matrix& worldMatrix);

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
