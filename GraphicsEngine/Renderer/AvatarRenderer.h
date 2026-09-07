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
	class BindlessHeap;
	class ShaderCache;
	class RootSignature;
	class PipelineStateObject;
	class D3D12CommandList;
	class AvatarMesh;
	class HumanCharacterEvaluator;

	class SEEDCORE_API AvatarRenderer :public NonCopyable
	{
	public:
		AvatarRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AvatarRenderer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Gather(const AvatarMesh& mesh, const HumanCharacterEvaluator& evaluator, const Matrix& worldMatrix);

		void Upload();

		void Begin(D3D12CommandList* cmdList);

		void Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene);

		void End(D3D12CommandList* cmdList);

		void RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ImGuiGPUHandle()const;

	private:
		static constexpr Uint32 maxMeshletsPerDispatch_ = 32;
		static constexpr Uint32 maxInstanceCount_ = 4096;
		static constexpr Uint32 maxBoneCount_ = 2048;

		ModelShader modelShader_;

		DynamicArray<ModelInstanceData> instances_;
		DynamicArray<Matrix> boneMatrices_;
		Bool uploaded_ = false;

		ResourcePtr<ReadOnlyStructuredBuffer<ModelInstanceData>> instanceBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> boneBuffer_;

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
