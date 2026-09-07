#include <GraphicsEngine/Avatar/AvatarMesh.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	namespace
	{
		constexpr Uint32 maxVerticesPerMeshlet = 64;
		constexpr Uint32 maxTrianglesPerMeshlet = 124;

		void BuildSimpleMeshlets(std::span<const Uint32> indices, DynamicArray<Meshlet>& outMeshlets, DynamicArray<Uint32>& outVertexIndices, DynamicArray<Uint8>& outPrimitiveIndices)
		{
			std::unordered_map<Uint32, Uint32> localIndexMap;
			DynamicArray<Uint32> currentVertexIndices;
			DynamicArray<Uint8> currentPrimitiveIndices;

			auto flush = [&]()
			{
				if (currentVertexIndices.empty())
				{
					return;
				}

				Meshlet meshlet;
				meshlet.vertexOffset_ = static_cast<Uint32>(outVertexIndices.size());
				meshlet.triangleOffset_ = static_cast<Uint32>(outPrimitiveIndices.size());
				meshlet.vertexCount_ = static_cast<Uint32>(currentVertexIndices.size());
				meshlet.triangleCount_ = static_cast<Uint32>(currentPrimitiveIndices.size() / 3);
				outMeshlets.push_back(meshlet);

				outVertexIndices.insert(outVertexIndices.end(), currentVertexIndices.begin(), currentVertexIndices.end());
				outPrimitiveIndices.insert(outPrimitiveIndices.end(), currentPrimitiveIndices.begin(), currentPrimitiveIndices.end());

				localIndexMap.clear();
				currentVertexIndices.clear();
				currentPrimitiveIndices.clear();
			};

			for (Size triangleIndex = 0; triangleIndex + 2 < indices.size(); triangleIndex += 3)
			{
				Uint32 globalVertices[3] = { indices[triangleIndex], indices[triangleIndex + 1], indices[triangleIndex + 2] };

				Uint32 newVertexCount = 0;
				for (Uint32 corner = 0; corner < 3; corner++)
				{
					if (!localIndexMap.contains(globalVertices[corner]))
					{
						newVertexCount++;
					}
				}

				if (currentVertexIndices.size() + newVertexCount > maxVerticesPerMeshlet || currentPrimitiveIndices.size() / 3 + 1 > maxTrianglesPerMeshlet)
				{
					flush();
				}

				for (Uint32 corner = 0; corner < 3; corner++)
				{
					Uint32 globalIndex = globalVertices[corner];
					auto found = localIndexMap.find(globalIndex);

					Uint32 localIndex;
					if (found == localIndexMap.end())
					{
						localIndex = static_cast<Uint32>(currentVertexIndices.size());
						currentVertexIndices.push_back(globalIndex);
						localIndexMap[globalIndex] = localIndex;
					}
					else
					{
						localIndex = found->second;
					}

					currentPrimitiveIndices.push_back(static_cast<Uint8>(localIndex));
				}
			}

			flush();
		}

		Uint32 QuantizeUnorm8(Float value)
		{
			Float clamped = std::clamp(value, 0.0f, 1.0f);
			return static_cast<Uint32>(clamped * 255.0f + 0.5f);
		}
	}

	Bool AvatarMesh::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, const HumanCharacterModel& model)
	{
		if (!model.IsLoaded())
		{
			return false;
		}

		bodyVertexCount_ = model.BodyVertexCount();
		if (bodyVertexCount_ == 0)
		{
			return false;
		}

		std::span<const Vector2> texcoords = model.Texcoords();
		baseTexcoords_.assign(texcoords.begin(), texcoords.begin() + bodyVertexCount_);

		BuildSimpleMeshlets(model.Triangles(), meshlets_, vertexIndices_, primitiveIndices_);
		if (meshlets_.empty())
		{
			return false;
		}

		Uint32 alignedPrimitiveByteSize = (static_cast<Uint32>(primitiveIndices_.size()) + 3) & ~3u;
		primitiveIndices_.resize(alignedPrimitiveByteSize, 0);

		std::span<const Uint32> skinIndices = model.SkinIndices();
		std::span<const Float> skinWeights = model.SkinWeights();
		skinVertices_.resize(bodyVertexCount_);
		for (Uint32 vertexIndex = 0; vertexIndex < bodyVertexCount_; vertexIndex++)
		{
			CompressedSkinVertex& skin = skinVertices_[vertexIndex];
			Uint32 joint0 = skinIndices[vertexIndex * 4 + 0];
			Uint32 joint1 = skinIndices[vertexIndex * 4 + 1];
			Uint32 joint2 = skinIndices[vertexIndex * 4 + 2];
			Uint32 joint3 = skinIndices[vertexIndex * 4 + 3];
			skin.jointsXY_ = (joint0 & 0xFFFF) | ((joint1 & 0xFFFF) << 16);
			skin.jointsZW_ = (joint2 & 0xFFFF) | ((joint3 & 0xFFFF) << 16);
			skin.weights_ =
				QuantizeUnorm8(skinWeights[vertexIndex * 4 + 0]) |
				(QuantizeUnorm8(skinWeights[vertexIndex * 4 + 1]) << 8) |
				(QuantizeUnorm8(skinWeights[vertexIndex * 4 + 2]) << 16) |
				(QuantizeUnorm8(skinWeights[vertexIndex * 4 + 3]) << 24);
		}

		texcoordMin_ = Vector2(0.0f, 0.0f);
		texcoordExtent_ = Vector2(1.0f, 1.0f);
		if (!baseTexcoords_.empty())
		{
			Vector2 texcoordMax = baseTexcoords_[0];
			texcoordMin_ = baseTexcoords_[0];
			for (const Vector2& texcoord : baseTexcoords_)
			{
				texcoordMin_ = Vector2::Min(texcoordMin_, texcoord);
				texcoordMax = Vector2::Max(texcoordMax, texcoord);
			}
			texcoordExtent_ = Vector2::Max(texcoordMax - texcoordMin_, Vector2(1e-6f, 1e-6f));
		}

		std::span<const Vector3> neutralPositions = model.Positions();
		Vector3 neutralMin = neutralPositions[0];
		Vector3 neutralMax = neutralPositions[0];
		for (Uint32 vertexIndex = 0; vertexIndex < bodyVertexCount_; vertexIndex++)
		{
			neutralMin = Vector3::Min(neutralMin, neutralPositions[vertexIndex]);
			neutralMax = Vector3::Max(neutralMax, neutralPositions[vertexIndex]);
		}
		Vector3 neutralExtent = neutralMax - neutralMin;
		positionMin_ = neutralMin - neutralExtent * 0.4f;
		positionExtent_ = Vector3::Max(neutralExtent * 1.8f, Vector3(1e-6f, 1e-6f, 1e-6f));

		scratchVertices_.resize(bodyVertexCount_);
		scratchBounds_.resize(meshlets_.size());

		vertexBuffer_ = MakePtr<ReadOnlyStructuredBuffer<CompressedVertex>>(device, bindlessHeap, bodyVertexCount_);
		skinVertexBuffer_ = MakePtr<ReadOnlyStructuredBuffer<CompressedSkinVertex>>(device, bindlessHeap, bodyVertexCount_);
		meshletBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Meshlet>>(device, bindlessHeap, static_cast<Uint>(meshlets_.size()));
		meshletBoundBuffer_ = MakePtr<ReadOnlyStructuredBuffer<MeshletBound>>(device, bindlessHeap, static_cast<Uint>(meshlets_.size()));
		vertexIndicesBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Uint32>>(device, bindlessHeap, static_cast<Uint>(vertexIndices_.size()));
		primitiveIndicesBuffer_ = MakePtr<ReadOnlyByteAddressBuffer>(device, bindlessHeap, alignedPrimitiveByteSize);

		return true;
	}

	void AvatarMesh::Update(const HumanCharacterEvaluator& evaluator)
	{
		if (!IsCreated())
		{
			return;
		}

		std::span<const Vector3> positions = evaluator.Positions();
		std::span<const Vector3> normals = evaluator.Normals();
		if (positions.size() < bodyVertexCount_ || normals.size() < bodyVertexCount_)
		{
			return;
		}

		for (Uint32 vertexIndex = 0; vertexIndex < bodyVertexCount_; vertexIndex++)
		{
			Vertex vertex;
			vertex.position_ = positions[vertexIndex];
			vertex.normal_ = normals[vertexIndex];
			vertex.tangent_ = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
			vertex.texcoord_ = baseTexcoords_[vertexIndex];
			scratchVertices_[vertexIndex] = Crister::EncodeVertex(vertex, positionMin_, positionExtent_, texcoordMin_, texcoordExtent_);
		}

		MeshletBound bound;
		bound.center_ = positionMin_ + positionExtent_ * 0.5f;
		bound.radius_ = positionExtent_.Length() * 0.5f;
		bound.coneAxis_ = Vector3(0.0f, 0.0f, 1.0f);
		bound.coneCutoff_ = -1.0f;
		std::ranges::fill(scratchBounds_, bound);

		vertexBuffer_->Update(scratchVertices_.data(), static_cast<Uint>(scratchVertices_.size()));
		meshletBoundBuffer_->Update(scratchBounds_.data(), static_cast<Uint>(scratchBounds_.size()));
		skinVertexBuffer_->Update(skinVertices_.data(), static_cast<Uint>(skinVertices_.size()));
		meshletBuffer_->Update(meshlets_.data(), static_cast<Uint>(meshlets_.size()));
		vertexIndicesBuffer_->Update(vertexIndices_.data(), static_cast<Uint>(vertexIndices_.size()));
		primitiveIndicesBuffer_->Update(primitiveIndices_.data(), static_cast<Uint>(primitiveIndices_.size()));
	}

	Bool AvatarMesh::IsCreated()const
	{
		return vertexBuffer_ != nullptr;
	}

	Uint AvatarMesh::VertexBufferIndex()const
	{
		return vertexBuffer_->Index();
	}

	Uint AvatarMesh::SkinVertexBufferIndex()const
	{
		return skinVertexBuffer_->Index();
	}

	Uint AvatarMesh::MeshletBufferIndex()const
	{
		return meshletBuffer_->Index();
	}

	Uint AvatarMesh::MeshletBoundBufferIndex()const
	{
		return meshletBoundBuffer_->Index();
	}

	Uint AvatarMesh::VertexIndicesBufferIndex()const
	{
		return vertexIndicesBuffer_->Index();
	}

	Uint AvatarMesh::PrimitiveIndicesBufferIndex()const
	{
		return primitiveIndicesBuffer_->Index();
	}

	Uint32 AvatarMesh::MeshletCount()const
	{
		return static_cast<Uint32>(meshlets_.size());
	}

	Uint32 AvatarMesh::BodyVertexCount()const
	{
		return bodyVertexCount_;
	}

	Vector3 AvatarMesh::PositionMin()const
	{
		return positionMin_;
	}

	Vector3 AvatarMesh::PositionExtent()const
	{
		return positionExtent_;
	}

	Vector2 AvatarMesh::TexcoordMin()const
	{
		return texcoordMin_;
	}

	Vector2 AvatarMesh::TexcoordExtent()const
	{
		return texcoordExtent_;
	}
}
