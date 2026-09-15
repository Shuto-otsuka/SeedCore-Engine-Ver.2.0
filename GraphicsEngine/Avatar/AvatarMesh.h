#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/D3D12/Buffer/StructuredBuffer.h>

namespace SeedCore
{
	class BindlessHeap;

	class SEEDCORE_API AvatarMesh :public NonCopyable
	{
	public:
		AvatarMesh() = default;
		~AvatarMesh() = default;

		struct RegionMeshletRange
		{
			Uint32 meshletOffset_ = 0;
			Uint32 meshletCount_ = 0;
		};

		Bool Create(ID3D12Device* device, BindlessHeap* bindlessHeap, std::span<const Uint32> triangles, std::span<const Uint32> skinIndices, std::span<const Float> skinWeights, std::span<const Vector2> texcoords, std::span<const Vector3> neutralPositions, Uint32 bodyVertexCount, std::span<const Uint32> regionTriangleRanges);

		[[nodiscard]] const RegionMeshletRange& MeshletRangeForRegion(Uint32 regionIndex)const;

		[[nodiscard]] Uint32 RegionCount()const;

		void Update(std::span<const Vector3> positions, std::span<const Vector3> normals);

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
		static constexpr Uint32 maxVerticesPerMeshlet_ = 64;
		static constexpr Uint32 maxTrianglesPerMeshlet_ = 124;

		static constexpr Uint32 maxRegionCount_ = 4;

		Uint32 bodyVertexCount_ = 0;
		Uint32 regionCount_ = 0;
		RegionMeshletRange regionMeshletRanges_[maxRegionCount_];

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
