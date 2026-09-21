#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Texture/TextureShader.h>

namespace SeedCore
{
	struct RootAddresses;
	struct LoaderSystem;
	class TextureResource;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class ShaderResourceIndicesSystem;

	class TextureRenderer
	{
	private:
		struct TextureSpriteStructuredBuffer
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

		struct TextureBillboardStructuredBuffer
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
		TextureRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~TextureRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ShaderResourceIndicesSystem& shaderResourceIndicesSystem);

		void Gather(LoaderSystem& loader, TextureResource& resource, World& world, Vector2 nativeScreenSize, std::span<const Entity> selectedEntities = {});

		void Upload();

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		DynamicArray<TextureSpriteStructuredBuffer> spriteInstances_;
		DynamicArray<TextureBillboardStructuredBuffer> billboardInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<TextureSpriteStructuredBuffer>> spriteBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<TextureBillboardStructuredBuffer>> billboardBuffer_;

		Bool hasSelectedSpriteInstance_ = false;
		Bool hasSelectedBillboardInstance_ = false;

		TextureShader imageShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
		ShaderResourceIndicesSystem* shaderResourceIndicesSystem_ = nullptr;

		Uint maxCount_ = 0;

		Uint64 streamingFrame_ = 0;
	};
}
