#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>
#include <GraphicsEngine/Movie/MovieShader.h>

namespace SeedCore
{
	struct RootAddresses;
	class World;
	class BindlessHeap;
	class ShaderCache;
	class PipelineStateObject;
	class ShaderResourceIndicesSystem;
	class MovieResource;
	struct LoaderSystem;

	class MovieRenderer
	{
	private:
		struct MovieSpriteStructuredBuffer
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

		struct MovieBillboardStructuredBuffer
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

		struct MovieFullscreenStructuredBuffer
		{
			Color color_;
			Uint textureIndex_;
			Float textureAspect_;
			Vector2 padding0_;
		};

	public:
		MovieRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);

		~MovieRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, ShaderResourceIndicesSystem& shaderResourceIndicesSystem);

		void Gather(LoaderSystem& loader, MovieResource& movieResource, World& world, Vector2 nativeScreenSize, std::span<const Entity> selectedEntities = {});

		void Upload();

		void DrawFullscreen(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteSprite(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawSilhouetteBillboard(ID3D12GraphicsCommandList6* cmdList, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		DynamicArray<MovieSpriteStructuredBuffer> spriteInstances_;

		DynamicArray<MovieBillboardStructuredBuffer> billboardInstances_;

		DynamicArray<MovieFullscreenStructuredBuffer> fullscreenInstances_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieSpriteStructuredBuffer>> spriteBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieBillboardStructuredBuffer>> billboardBuffer_;

		ResourcePtr<ReadOnlyStructuredBuffer<MovieFullscreenStructuredBuffer>> fullscreenBuffer_;

		Bool hasSelectedSpriteInstance_ = false;

		Bool hasSelectedBillboardInstance_ = false;

		MovieShader movieShader_;

		BindlessHeap* bindlessHeap_ = nullptr;

		ShaderResourceIndicesSystem* shaderResourceIndicesSystem_ = nullptr;

		Uint maxCount_ = 0;
	};
}
