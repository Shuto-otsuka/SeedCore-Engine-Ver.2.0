#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Font/FontShader.h>

namespace SeedCore
{
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class IndicesSystem;
	class FontResource;

	class FontRenderer
	{
	private:
		struct FontSpriteInstance
		{
			Vector2 position_;
			Vector2 size_;
			Vector2 uvMin_;
			Vector2 uvMax_;

			Color color_;
			Color outlineColor_;
			Color glowColor_;

			Uint textureIndex_;
			Float outlineWidth_;
			Float glowPower_;
			Float padding1_;

			Vector2 unitRange_;
			Uint selected_;
			Float padding2_;
		};

		struct FontBillboardInstance
		{
			Vector3 position_;
			Float padding0_;
			Vector3 rotation_;
			Float padding1_;

			Vector2 localPosition_;
			Vector2 localSize_;
			Vector2 uvMin_;
			Vector2 uvMax_;

			Color color_;
			Color outlineColor_;
			Color glowColor_;

			Uint textureIndex_;
			Float outlineWidth_;
			Float glowPower_;
			Float padding2_;

			Vector2 unitRange_;
			Uint faceCamera_;
			Uint selected_;
		};

	public:
		FontRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~FontRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem);

		void Gather(FontResource& fontResource, World& world, Vector2 nativeScreenSize, Entity selectedEntity = Entity::Null());

		void Upload();

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

		void DrawSelectionMaskBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

	private:
		DynamicArray<FontSpriteInstance> spriteInstances_;
		DynamicArray<FontBillboardInstance> billboardInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<FontSpriteInstance>> spriteBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<FontBillboardInstance>> billboardBuffer_;

		FontShader fontShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
		IndicesSystem* indicesSystem_ = nullptr;

		Uint maxCount_ = 0;
	};
}
