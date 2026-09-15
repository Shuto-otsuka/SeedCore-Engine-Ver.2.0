#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/Model/ModelShader.h>
#include <GraphicsEngine/Model/Culling/ModelCullingBuffer.h>
#include <GraphicsEngine/Model/ModelRecord.h>
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

	class SEEDCORE_API AvatarRenderer :public NonCopyable
	{
	public:
		AvatarRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AvatarRenderer();

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height);

		void Gather(const AvatarMesh& mesh, Uint32 boneCount, const Matrix& worldMatrix, std::span<const Uint32> regionTextureIndices);

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
		ModelCullingBuffer modelCullingBuffer_;

		DynamicArray<ModelStructuredBuffer> instances_;
		DynamicArray<Matrix> boneMatrices_;
		Bool uploaded_ = false;

		ResourcePtr<ReadOnlyStructuredBuffer<ModelStructuredBuffer>> instanceBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Matrix>> boneBuffer_;

		BindlessHeap* bindlessHeap_ = nullptr;

		DescriptorHeap renderTargetViewHeap_;
		DescriptorHeap depthStencilViewHeap_;
		ResourcePtr<FrameBuffer> frameBuffer_;

		ResourcePtr<SceneSystem> sceneSystem_;

		ConstantIndices constantIndices_{};
		ShaderResourceIndices shaderResourceIndices_{};
		ResourcePtr<ConstantBuffer<ConstantIndices>> constantIndicesBuffer_;
		ResourcePtr<ConstantBuffer<ShaderResourceIndices>> shaderResourceIndicesBuffer_;

		DescriptorHeap* imguiHeap_ = nullptr;
		Uint32 imguiShaderResourceViewIndex_ = 0;
	};
}
