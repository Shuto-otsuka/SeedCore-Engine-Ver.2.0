#include <GraphicsEngine/Avatar/AvatarMesh.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Bool AvatarMesh::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, std::span<const Uint32> triangles, std::span<const Uint32> skinIndices, std::span<const Float> skinWeights, std::span<const Vector2> texcoords, std::span<const Vector3> neutralPositions, Uint32 bodyVertexCount, std::span<const Uint32> regionTriangleRanges)
	{
		bodyVertexCount_ = bodyVertexCount;
		if (bodyVertexCount_ == 0 || neutralPositions.size() < bodyVertexCount_)
		{
			return false;
		}

		baseTexcoords_.assign(texcoords.begin(), texcoords.begin() + bodyVertexCount_);

		std::unordered_map<Uint32, Uint32> localIndexMap;
		DynamicArray<Uint32> currentVertexIndices;
		DynamicArray<Uint8> currentPrimitiveIndices;

		auto flushMeshlet = [&]()
		{
			if (currentVertexIndices.empty())
			{
				return;
			}

			Meshlet meshlet;
			meshlet.vertexOffset_ = static_cast<Uint32>(vertexIndices_.size());
			meshlet.triangleOffset_ = static_cast<Uint32>(primitiveIndices_.size());
			meshlet.vertexCount_ = static_cast<Uint32>(currentVertexIndices.size());
			meshlet.triangleCount_ = static_cast<Uint32>(currentPrimitiveIndices.size() / 3);
			meshlets_.push_back(meshlet);

			vertexIndices_.insert(vertexIndices_.end(), currentVertexIndices.begin(), currentVertexIndices.end());
			primitiveIndices_.insert(primitiveIndices_.end(), currentPrimitiveIndices.begin(), currentPrimitiveIndices.end());

			localIndexMap.clear();
			currentVertexIndices.clear();
			currentPrimitiveIndices.clear();
		};

		/// [EN] Build meshlets per region so no meshlet straddles a region boundary -
		///      the renderer can then draw one region (one texture) at a time.
		/// [JP] メッシュレットをリージョン単位で構築し、リージョン境界をまたがない
		///      ようにする - レンダラーがリージョン(=テクスチャ)ごとに描ける。
		Uint32 triangleCount = static_cast<Uint32>(triangles.size() / 3);
		regionCount_ = Min(static_cast<Uint32>(regionTriangleRanges.size() / 2), maxRegionCount_);
		if (regionCount_ == 0)
		{
			regionCount_ = 1;
			regionMeshletRanges_[0] = { 0, 0 };
		}

		for (Uint32 regionIndex = 0; regionIndex < regionCount_; regionIndex++)
		{
			Uint32 firstTriangle = regionCount_ > 1 ? regionTriangleRanges[regionIndex * 2 + 0] : 0;
			Uint32 regionTriangleCount = regionCount_ > 1 ? regionTriangleRanges[regionIndex * 2 + 1] : triangleCount;
			Uint32 meshletOffset = static_cast<Uint32>(meshlets_.size());

			for (Uint32 localTriangle = 0; localTriangle < regionTriangleCount; localTriangle++)
			{
				Size triangleIndex = static_cast<Size>(firstTriangle + localTriangle) * 3;
				if (triangleIndex + 2 >= triangles.size())
				{
					break;
				}
				Uint32 globalVertices[3] = { triangles[triangleIndex], triangles[triangleIndex + 1], triangles[triangleIndex + 2] };

				Uint32 newVertexCount = 0;
				for (Uint32 corner = 0; corner < 3; corner++)
				{
					if (!localIndexMap.contains(globalVertices[corner]))
					{
						newVertexCount++;
					}
				}

				if (currentVertexIndices.size() + newVertexCount > maxVerticesPerMeshlet_ || currentPrimitiveIndices.size() / 3 + 1 > maxTrianglesPerMeshlet_)
				{
					flushMeshlet();
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

			flushMeshlet();
			regionMeshletRanges_[regionIndex].meshletOffset_ = meshletOffset;
			regionMeshletRanges_[regionIndex].meshletCount_ = static_cast<Uint32>(meshlets_.size()) - meshletOffset;
		}

		if (meshlets_.empty())
		{
			return false;
		}

		Uint32 alignedPrimitiveByteSize = (static_cast<Uint32>(primitiveIndices_.size()) + 3) & ~3u;
		primitiveIndices_.resize(alignedPrimitiveByteSize, 0);

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
			Float weight0 = Clamp(skinWeights[vertexIndex * 4 + 0], 0.0f, 1.0f);
			Float weight1 = Clamp(skinWeights[vertexIndex * 4 + 1], 0.0f, 1.0f);
			Float weight2 = Clamp(skinWeights[vertexIndex * 4 + 2], 0.0f, 1.0f);
			Float weight3 = Clamp(skinWeights[vertexIndex * 4 + 3], 0.0f, 1.0f);
			skin.weights_ =
				 static_cast<Uint32>(weight0 * 255.0f + 0.5f) |
				(static_cast<Uint32>(weight1 * 255.0f + 0.5f) << 8) |
				(static_cast<Uint32>(weight2 * 255.0f + 0.5f) << 16) |
				(static_cast<Uint32>(weight3 * 255.0f + 0.5f) << 24);
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

	void AvatarMesh::Update(std::span<const Vector3> positions, std::span<const Vector3> normals)
	{
		if (!IsCreated())
		{
			return;
		}

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

	const AvatarMesh::RegionMeshletRange& AvatarMesh::MeshletRangeForRegion(Uint32 regionIndex)const
	{
		return regionMeshletRanges_[regionIndex < regionCount_ ? regionIndex : 0];
	}

	Uint32 AvatarMesh::RegionCount()const
	{
		return regionCount_;
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
