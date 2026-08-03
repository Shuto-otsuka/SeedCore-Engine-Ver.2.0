#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Texture/ImageShader.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ImageResource;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class IndicesSystem;

	class ImageRenderer
	{
	private:
		struct ImageSpriteInstance
		{
			Vector2 position_;
			Float rotation_;
			Vector2 scale_;
			Float padding0_;
			Vector2 textureSize_;
			Vector2 texturePosition_;
			Vector2 pivot_;
			Vector2 padding1_;
			Color color_;
			Uint textureIndex_;
			Float scrollSpeed_;
			Vector2 scrollDirection_;
			Uint motionType_;
			Uint selected_;
			Vector3 padding2_;
		};

		struct ImageBillboardInstance
		{
			Vector3 position_;
			Vector3 rotation_;
			Vector2 scale_;
			Vector2 textureSize_;
			Vector2 texturePosition_;
			Vector2 pivot_;
			Vector2 padding0_;
			Color color_;
			Uint textureIndex_;
			Float scrollSpeed_;
			Vector2 scrollDirection_;
			Uint motionType_;
			Uint faceCamera_;
			Uint selected_;
			Vector2 padding1_;
		};

	public:
		ImageRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ImageRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem);

		void Gather(LoaderSystem& loader, ImageResource& resource, World& world, Entity selectedEntity = Entity::Null());

		void Upload();

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

	private:
		DynamicArray<ImageSpriteInstance> spriteInstances_;
		DynamicArray<ImageBillboardInstance> billboardInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<ImageSpriteInstance>> spriteBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<ImageBillboardInstance>> billboardBuffer_;

		Bool hasSelectedSpriteInstance_ = false;
		Bool hasSelectedBillboardInstance_ = false;

		ImageShader imageShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint maxCount_ = 0;

		Uint64 streamingFrame_ = 0;
	};
}
