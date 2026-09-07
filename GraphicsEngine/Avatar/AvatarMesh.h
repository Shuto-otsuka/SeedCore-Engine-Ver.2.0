#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>

namespace SeedCore
{
	class BindlessHeap;
	class HumanCharacterModel;
	class HumanCharacterEvaluator;

	class SEEDCORE_API AvatarMesh :public NonCopyable
	{
	public:
		AvatarMesh() = default;
		~AvatarMesh() = default;

		Bool Create(ID3D12Device* device, BindlessHeap* bindlessHeap, const HumanCharacterModel& model);

		void Update(const HumanCharacterEvaluator& evaluator);

		[[nodiscard]] Bool IsCreated()const;

		[[nodiscard]] Uint VertexBufferIndex()const;
		[[nodiscard]] Uint SkinVertexBufferIndex()const;
		[[nodiscard]] Uint MeshletBufferIndex()const;
		[[nodiscard]] Uint MeshletBoundBufferIndex()const;
		[[nodiscard]] Uint VertexIndicesBufferIndex()const;
		[[nodiscard]] Uint PrimitiveIndicesBufferIndex()const;

		[[nodiscard]] Uint32 MeshletCount()const;
		[[nodiscard]] Uint32 BodyVertexCount()const;
		[[nodiscard]] Vector3 PositionMin()const;
		[[nodiscard]] Vector3 PositionExtent()const;
		[[nodiscard]] Vector2 TexcoordMin()const;
		[[nodiscard]] Vector2 TexcoordExtent()const;

	private:
		Uint32 bodyVertexCount_ = 0;

		DynamicArray<Vector2> baseTexcoords_;

		DynamicArray<Meshlet> meshlets_;
		DynamicArray<Uint32> vertexIndices_;
		DynamicArray<Uint8> primitiveIndices_;
		DynamicArray<CompressedSkinVertex> skinVertices_;

		Vector2 texcoordMin_ = { 0.0f, 0.0f };
		Vector2 texcoordExtent_ = { 1.0f, 1.0f };
		Vector3 positionMin_ = { 0.0f, 0.0f, 0.0f };
		Vector3 positionExtent_ = { 1.0f, 1.0f, 1.0f };

		DynamicArray<CompressedVertex> scratchVertices_;
		DynamicArray<MeshletBound> scratchBounds_;

		ResourcePtr<ReadOnlyStructuredBuffer<CompressedVertex>> vertexBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<CompressedSkinVertex>> skinVertexBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Meshlet>> meshletBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<MeshletBound>> meshletBoundBuffer_;
		ResourcePtr<ReadOnlyStructuredBuffer<Uint32>> vertexIndicesBuffer_;
		ResourcePtr<ReadOnlyByteAddressBuffer> primitiveIndicesBuffer_;
	};
}
