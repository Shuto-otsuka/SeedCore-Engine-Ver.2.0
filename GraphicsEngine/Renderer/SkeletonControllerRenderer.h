#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Interop/ColliderInstance.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Model/ModelInstanceData.h>
#include <GraphicsEngine/Shape/Collider/ColliderLineShader.h>
#include <GraphicsEngine/Renderer/ColliderRenderer.h>
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

	class SEEDCORE_API SkeletonControllerRenderer :public NonCopyable
	{
	public:
		SkeletonControllerRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~SkeletonControllerRenderer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix, Int selectedNodeIndex);

		void Upload();

		void Begin(D3D12CommandList* cmdList);

		void Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene);

		void DrawBones(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap);

		void End(D3D12CommandList* cmdList);

		void RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ImGuiGPUHandle()const;

	private:
		static constexpr Uint maxBoneInstanceCount_ = 2048;

		static constexpr Uint icosphereSubdivisionLevel_ = 3;

		static constexpr Uint boneConeRingSegments_ = 32;
		static constexpr Uint boneConeVerticalLineCount_ = 8;

		static constexpr Uint threadsPerGroup_ = 128;

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

		ColliderLineShader boneLineShader_;
		PipelineStateObject boneLinePipelineStateObject_;

		DynamicArray<ColliderInstance> boneInstances_;
		ResourcePtr<ReadOnlyStructuredBuffer<ColliderInstance>> boneInstanceBuffer_;
		ResourcePtr<ConstantBuffer<ColliderInstanceConstants>> boneInstanceConstantsBuffer_;

		DynamicArray<Vector3> sphereEdgeData_;
		ResourcePtr<ReadOnlyStructuredBuffer<Vector3>> sphereEdgeBuffer_;
		Uint sphereEdgeCount_ = 0;

		Uint groupsPerBoneInstance_ = 1;

		DescriptorHeap* imguiHeap_ = nullptr;
		Uint32 imguiShaderResourceViewIndex_ = 0;
	};
}
