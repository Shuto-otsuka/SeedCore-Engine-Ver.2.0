#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Movie/MovieShader.h>

namespace SeedCore
{
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class IndicesSystem;
	class MovieResource;

	class MovieRenderer
	{
	private:
		struct MovieSpriteInstance
		{
			Vector2 position_;
			Float rotation_;
			Vector2 scale_;
			Float padding0_;
			Vector2 size_;
			Vector2 padding1_;
			Vector2 pivot_;
			Vector2 padding2_;
			Color color_;
			Uint textureIndex_;
			Uint selected_;
			Vector2 padding3_;
		};

		struct MovieBillboardInstance
		{
			Vector3 position_;
			Vector3 rotation_;
			Vector2 scale_;
			Vector2 size_;
			Vector2 padding0_;
			Vector2 pivot_;
			Vector2 padding1_;
			Color color_;
			Uint textureIndex_;
			Uint faceCamera_;
			Uint selected_;
			Float padding2_;
		};

		struct MovieFullscreenInstance
		{
			Color color_;
			Uint textureIndex_;
			Float textureAspect_;
			Vector2 padding0_;
		};

	public:
		MovieRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);

		~MovieRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem);

		void Gather(MovieResource& movieResource, World& world, Entity selectedEntity = Entity::Null());

		void Upload();

		void DrawFullscreen(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

	private:
		DynamicArray<MovieSpriteInstance> spriteInstances_;

		DynamicArray<MovieBillboardInstance> billboardInstances_;

		DynamicArray<MovieFullscreenInstance> fullscreenInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieSpriteInstance>> spriteBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieBillboardInstance>> billboardBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieFullscreenInstance>> fullscreenBuffer_;

		Bool hasSelectedSpriteInstance_ = false;

		Bool hasSelectedBillboardInstance_ = false;

		MovieShader movieShader_;

		BindlessHeap* bindlessHeap_ = nullptr;

		IndicesSystem* indicesSystem_ = nullptr;

		Uint maxCount_ = 0;
	};
}
