#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Font/FontShader.h>

namespace SeedCore
{
	struct RootAddresses;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class ShaderResourceIndicesSystem;
	class FontResource;
	struct LoaderSystem;

	class FontRenderer
	{
	private:
		struct FontSpriteStructuredBuffer
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

		struct FontBillboardStructuredBuffer
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

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ShaderResourceIndicesSystem& shaderResourceIndicesSystem);

		void Gather(LoaderSystem& loader, FontResource& fontResource, World& world, Vector2 nativeScreenSize, std::span<const Entity> selectedEntities = {});

		void Upload();

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		DynamicArray<FontSpriteStructuredBuffer> spriteInstances_;
		DynamicArray<FontBillboardStructuredBuffer> billboardInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<FontSpriteStructuredBuffer>> spriteBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<FontBillboardStructuredBuffer>> billboardBuffer_;

		FontShader fontShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
		ShaderResourceIndicesSystem* shaderResourceIndicesSystem_ = nullptr;

		Uint maxCount_ = 0;
	};
}
