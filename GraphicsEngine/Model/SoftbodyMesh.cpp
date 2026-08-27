#include <GraphicsEngine/Model/SoftbodyMesh.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	namespace
	{
		constexpr Uint32 maxVerticesPerMeshlet = 64;
		constexpr Uint32 maxTrianglesPerMeshlet = 124;

		/// [EN] Greedy meshlet packer: walks the (global-index) triangle list
		///      in order, keeping a running "current meshlet" vertex set, and
		///      flushes into a new meshlet whenever adding the next triangle
		///      would exceed the 64-vertex/124-triangle mesh-shader limits.
		///      Not clustering-quality (no spatial locality optimisation like
		///      ModelLoader::BuildMeshlets's meshoptimizer-equivalent), just
		///      correct — acceptable for a Softbody's small, non-streamed
		///      mesh. outVertexIndices holds, per meshlet-local slot, the
		///      GLOBAL vertex index (mirrors Crister::vertexIndices_);
		///      outPrimitiveIndices holds, per triangle corner, the
		///      meshlet-LOCAL slot index 0..vertexCount-1 (mirrors
		///      Crister::primitiveIndices_) — same two-level indirection
		///      StaticModelMS.hlsl already expects.
		/// [JP] 貪欲法のメシュレットパッカー: (グローバルインデックスの)
		///      三角形リストを順に走査し、「現在のメシュレット」の頂点集合を
		///      保持しながら、次の三角形を足すと 64頂点/124三角形の
		///      メッシュシェーダ制限を超える時点で新しいメシュレットへ
		///      フラッシュする。クラスタリング品質は狙わない（
		///      ModelLoader::BuildMeshlets のような meshoptimizer 相当の
		///      空間局所性最適化は無し）が、正しさは保つ — Softbody の
		///      小さく非ストリーミングなメッシュには十分。outVertexIndices は
		///      メシュレットローカルスロットごとに GLOBAL 頂点インデックスを
		///      持つ（Crister::vertexIndices_ と同じ）。outPrimitiveIndices は
		///      三角形の頂点1つごとにメシュレットLOCALスロットインデックス
		///      0..vertexCount-1 を持つ（Crister::primitiveIndices_ と同じ）
		///      — StaticModelMS.hlsl が既に期待するのと同じ二段間接参照。
		void BuildSimpleMeshlets(const DynamicArray<Uint32>& indices, DynamicArray<Meshlet>& outMeshlets, DynamicArray<Uint32>& outVertexIndices, DynamicArray<Uint8>& outPrimitiveIndices)
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
	}

	/**
	* [EN]
	* For each full-resolution render vertex, finds the bindMaxWeights_
	* nearest simulation-proxy vertices (by bind-pose distance) via brute
	* force (both lists are small enough that a KD-tree wouldn't pay for
	* itself at this one-time build cost) and weights them by inverse
	* squared distance, normalised to sum to 1.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フル解像度描画頂点ごとに、最も近いシミュレーション用プロキシ頂点
	* （バインドポーズ距離）を bindMaxWeights_ 個、総当たりで見つける
	* （どちらのリストも小さいため、この一度きりの構築コストで KD-tree は
	* 割に合わない）。距離の逆二乗で重み付けし、合計が1になるよう正規化する。
	*/
	void SoftbodyMesh::BindRenderVerticesToProxy(const DynamicArray<Vertex>& renderVertices, const DynamicArray<Vertex>& proxyVertices, DynamicArray<VertexBinding>& outBindings)
	{
		outBindings.resize(renderVertices.size());

		for (Size renderIndex = 0; renderIndex < renderVertices.size(); renderIndex++)
		{
			const Vector3& renderPosition = renderVertices[renderIndex].position_;

			Uint32 bestIndex[bindMaxWeights_] = {};
			Float bestDistanceSquared[bindMaxWeights_] = {};
			Uint32 bestCount = 0;

			for (Size proxyIndex = 0; proxyIndex < proxyVertices.size(); proxyIndex++)
			{
				Float distanceSquared = (proxyVertices[proxyIndex].position_ - renderPosition).LengthSquared();

				if (bestCount < bindMaxWeights_)
				{
					bestIndex[bestCount] = static_cast<Uint32>(proxyIndex);
					bestDistanceSquared[bestCount] = distanceSquared;
					bestCount++;
				}
				else
				{
					Uint32 worst = 0;
					for (Uint32 k = 1; k < bindMaxWeights_; k++)
					{
						if (bestDistanceSquared[k] > bestDistanceSquared[worst])
						{
							worst = k;
						}
					}

					if (distanceSquared < bestDistanceSquared[worst])
					{
						bestIndex[worst] = static_cast<Uint32>(proxyIndex);
						bestDistanceSquared[worst] = distanceSquared;
					}
				}
			}

			VertexBinding binding;
			Float weightSum = 0.0f;
			for (Uint32 k = 0; k < bindMaxWeights_; k++)
			{
				if (k < bestCount)
				{
					Float weight = 1.0f / (bestDistanceSquared[k] + 1e-6f);
					binding.proxyIndex_[k] = bestIndex[k];
					binding.weight_[k] = weight;
					weightSum += weight;
				}
				else
				{
					binding.proxyIndex_[k] = bestCount > 0 ? bestIndex[0] : 0;
					binding.weight_[k] = 0.0f;
				}
			}

			if (weightSum > 0.0f)
			{
				for (Uint32 k = 0; k < bindMaxWeights_; k++)
				{
					binding.weight_[k] /= weightSum;
				}
			}

			outBindings[renderIndex] = binding;
		}
	}

	Bool SoftbodyMesh::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, const Crister& crister)
	{
		DynamicArray<Uint32> indices;
		if (!crister.SoftbodyFinestVertices(bindPoseVertices_, indices))
		{
			return false;
		}

		DynamicArray<Uint32> proxyIndices;
		if (!crister.SoftbodyCoarsestVertices(proxyBindPoseVertices_, proxyIndices))
		{
			return false;
		}

		BuildSimpleMeshlets(indices, meshlets_, vertexIndices_, primitiveIndices_);
		if (meshlets_.empty())
		{
			return false;
		}

		/// [EN] ReadOnlyByteAddressBuffer capacity/updates are 4-byte aligned.
		/// [JP] ReadOnlyByteAddressBuffer の容量/更新は4バイト境界であること。
		Uint32 alignedPrimitiveByteSize = (static_cast<Uint32>(primitiveIndices_.size()) + 3) & ~3u;
		primitiveIndices_.resize(alignedPrimitiveByteSize, 0);

		texcoordMin_ = crister.TexcoordMin();
		texcoordExtent_ = crister.TexcoordExtent();

		BindRenderVerticesToProxy(bindPoseVertices_, proxyBindPoseVertices_, renderVertexBindings_);

		scratchProxyDisplacements_.resize(proxyBindPoseVertices_.size());
		scratchDeformedPositions_.resize(bindPoseVertices_.size());
		scratchVertices_.resize(bindPoseVertices_.size());
		scratchBounds_.resize(meshlets_.size());

		vertexBuffer_ = MakePtr<ReadOnlyStructuredBuffer<CompressedVertex>>(device, bindlessHeap, static_cast<Uint>(bindPoseVertices_.size()));
		meshletBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Meshlet>>(device, bindlessHeap, static_cast<Uint>(meshlets_.size()));
		meshletBoundBuffer_ = MakePtr<ReadOnlyStructuredBuffer<MeshletBound>>(device, bindlessHeap, static_cast<Uint>(meshlets_.size()));
		vertexIndicesBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Uint32>>(device, bindlessHeap, static_cast<Uint>(vertexIndices_.size()));
		primitiveIndicesBuffer_ = MakePtr<ReadOnlyByteAddressBuffer>(device, bindlessHeap, alignedPrimitiveByteSize);

		return true;
	}

	void SoftbodyMesh::Update(const DynamicArray<Vector3>& simulatedProxyPositions)
	{
		if (proxyBindPoseVertices_.empty() || bindPoseVertices_.empty() || simulatedProxyPositions.size() != proxyBindPoseVertices_.size())
		{
			return;
		}

		for (Size proxyIndex = 0; proxyIndex < proxyBindPoseVertices_.size(); proxyIndex++)
		{
			scratchProxyDisplacements_[proxyIndex] = simulatedProxyPositions[proxyIndex] - proxyBindPoseVertices_[proxyIndex].position_;
		}

		for (Size renderIndex = 0; renderIndex < bindPoseVertices_.size(); renderIndex++)
		{
			const VertexBinding& binding = renderVertexBindings_[renderIndex];

			Vector3 blendedDisplacement(0.0f, 0.0f, 0.0f);
			for (Uint32 k = 0; k < bindMaxWeights_; k++)
			{
				blendedDisplacement += scratchProxyDisplacements_[binding.proxyIndex_[k]] * binding.weight_[k];
			}

			scratchDeformedPositions_[renderIndex] = bindPoseVertices_[renderIndex].position_ + blendedDisplacement;
		}

		Vector3 positionMax = scratchDeformedPositions_[0];
		positionMin_ = scratchDeformedPositions_[0];
		for (const Vector3& position : scratchDeformedPositions_)
		{
			positionMin_ = Vector3::Min(positionMin_, position);
			positionMax = Vector3::Max(positionMax, position);
		}
		positionExtent_ = Vector3::Max(positionMax - positionMin_, Vector3(1e-6f, 1e-6f, 1e-6f));

		for (Size vertexIndex = 0; vertexIndex < bindPoseVertices_.size(); vertexIndex++)
		{
			Vertex vertex = bindPoseVertices_[vertexIndex];
			vertex.position_ = scratchDeformedPositions_[vertexIndex];
			scratchVertices_[vertexIndex] = Crister::EncodeVertex(vertex, positionMin_, positionExtent_, texcoordMin_, texcoordExtent_);
		}

		/// [EN] One conservative bound (this frame's whole-mesh AABB)
		///      replicated to every meshlet — cheaper than per-meshlet
		///      bounds and still correct (each meshlet's geometry is a
		///      subset of the whole mesh, so the whole-mesh bound trivially
		///      contains it too). coneCutoff_ <= 0 disables ModelAS.hlsl's
		///      normal-cone culling (see its `bound.cone_cutoff_ > 0.0`
		///      guard) since per-meshlet cones aren't computed here.
		/// [JP] このフレームのメッシュ全体 AABB から得た1つの保守的な
		///      バウンドを、全メシュレットへ複製する — メシュレットごとの
		///      バウンドより安く、かつ正しい（各メシュレットのジオメトリは
		///      メッシュ全体の部分集合なので、メッシュ全体のバウンドは
		///      当然それも含む）。coneCutoff_ <= 0 は ModelAS.hlsl の
		///      法線コーンカリングを無効化する（`bound.cone_cutoff_ > 0.0`
		///      ガード参照）— メシュレットごとのコーンはここでは計算して
		///      いないため。
		MeshletBound bound;
		bound.center_ = (positionMin_ + positionMax) * 0.5f;
		bound.radius_ = (positionMax - positionMin_).Length() * 0.5f;
		bound.coneAxis_ = Vector3(0.0f, 0.0f, 1.0f);
		bound.coneCutoff_ = -1.0f;
		std::ranges::fill(scratchBounds_, bound);

		vertexBuffer_->Update(scratchVertices_.data(), static_cast<Uint>(scratchVertices_.size()));
		meshletBuffer_->Update(meshlets_.data(), static_cast<Uint>(meshlets_.size()));
		meshletBoundBuffer_->Update(scratchBounds_.data(), static_cast<Uint>(scratchBounds_.size()));
		vertexIndicesBuffer_->Update(vertexIndices_.data(), static_cast<Uint>(vertexIndices_.size()));
		primitiveIndicesBuffer_->Update(primitiveIndices_.data(), static_cast<Uint>(primitiveIndices_.size()));
	}

	Uint SoftbodyMesh::VertexBufferIndex()const
	{
		return vertexBuffer_->Index();
	}

	Uint SoftbodyMesh::MeshletBufferIndex()const
	{
		return meshletBuffer_->Index();
	}

	Uint SoftbodyMesh::MeshletBoundBufferIndex()const
	{
		return meshletBoundBuffer_->Index();
	}

	Uint SoftbodyMesh::VertexIndicesBufferIndex()const
	{
		return vertexIndicesBuffer_->Index();
	}

	Uint SoftbodyMesh::PrimitiveIndicesBufferIndex()const
	{
		return primitiveIndicesBuffer_->Index();
	}

	Uint32 SoftbodyMesh::MeshletCount()const
	{
		return static_cast<Uint32>(meshlets_.size());
	}

	Vector3 SoftbodyMesh::PositionMin()const
	{
		return positionMin_;
	}

	Vector3 SoftbodyMesh::PositionExtent()const
	{
		return positionExtent_;
	}

	Vector2 SoftbodyMesh::TexcoordMin()const
	{
		return texcoordMin_;
	}

	Vector2 SoftbodyMesh::TexcoordExtent()const
	{
		return texcoordExtent_;
	}
}
