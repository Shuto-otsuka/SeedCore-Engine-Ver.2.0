#include <GraphicsEngine/Renderer/ColliderRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] Builds a subdivided-icosahedron ("icosphere") unit-sphere
		///      edge list, plus the subset of edges whose endpoints both lie
		///      in the y>=0 hemisphere — used for the sphere shape and for
		///      capsule end-caps respectively. Same construction JPH's own
		///      DrawWireSphere uses (recursive icosahedron subdivision), so
		///      the wireframe density matches at the same subdivision level.
		///      Faces are kept as flat Uint32 triples (faces[i*3+0..2]) rather
		///      than a named struct — there's no behavior attached to a
		///      "face", just index bookkeeping during subdivision.
		/// [JP] 細分割した正20面体（アイコスフィア）の単位球エッジ一覧と、
		///      両端が y>=0 半球に収まるものだけの部分集合（それぞれ球形状/
		///      カプセルの半球キャップ用）を構築する。JPH自身の
		///      DrawWireSphere と同じ構築法（正20面体の再帰的細分割）なので、
		///      同じ細分割レベルならワイヤーフレームの密度が一致する。
		///      面は名前付き struct ではなく、フラットな Uint32 の3つ組
		///      (faces[i*3+0..2]) のまま扱う — 「面」に紐づく振る舞いはなく、
		///      細分割中のインデックス管理でしかないため。
		void BuildIcosphereEdges(Uint subdivisionLevel, DynamicArray<Vector3>& outFullEdges, DynamicArray<Vector3>& outHemisphereEdges)
		{
			auto edgeKey = [](Uint32 a, Uint32 b) -> Uint64
			{
				Uint32 lo = a < b ? a : b;
				Uint32 hi = a < b ? b : a;
				return (static_cast<Uint64>(lo) << 32) | static_cast<Uint64>(hi);
			};

			const Float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

			DynamicArray<Vector3> vertices =
			{
				Vector3(-1.0f, t, 0.0f), Vector3(1.0f, t, 0.0f), Vector3(-1.0f, -t, 0.0f), Vector3(1.0f, -t, 0.0f),
				Vector3(0.0f, -1.0f, t), Vector3(0.0f, 1.0f, t), Vector3(0.0f, -1.0f, -t), Vector3(0.0f, 1.0f, -t),
				Vector3(t, 0.0f, -1.0f), Vector3(t, 0.0f, 1.0f), Vector3(-t, 0.0f, -1.0f), Vector3(-t, 0.0f, 1.0f),
			};
			for (Vector3& vertex : vertices)
			{
				vertex.Normalize();
			}

			DynamicArray<Uint32> faces =
			{
				0,11, 5,  0, 5, 1,  0, 1, 7,  0, 7,10,  0,10,11,
				1, 5, 9,  5,11, 4, 11,10, 2, 10, 7, 6,  7, 1, 8,
				3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
				4, 9, 5,  2, 4,11,  6, 2,10,  8, 6, 7,  9, 8, 1,
			};

			for (Uint level = 0; level < subdivisionLevel; level++)
			{
				std::unordered_map<Uint64, Uint32> midpointCache;

				auto getOrCreateMidpoint = [&vertices, &midpointCache, &edgeKey](Uint32 a, Uint32 b) -> Uint32
				{
					Uint64 key = edgeKey(a, b);
					if (auto it = midpointCache.find(key); it != midpointCache.end())
					{
						return it->second;
					}

					Vector3 midpoint = (vertices[a] + vertices[b]) * 0.5f;
					midpoint.Normalize();

					Uint32 index = static_cast<Uint32>(vertices.size());
					vertices.push_back(midpoint);
					midpointCache.emplace(key, index);
					return index;
				};

				DynamicArray<Uint32> subdividedFaces;

				for (Size faceIndex = 0; faceIndex < faces.size(); faceIndex += 3)
				{
					Uint32 a = faces[faceIndex + 0];
					Uint32 b = faces[faceIndex + 1];
					Uint32 c = faces[faceIndex + 2];

					Uint32 ab = getOrCreateMidpoint(a, b);
					Uint32 bc = getOrCreateMidpoint(b, c);
					Uint32 ca = getOrCreateMidpoint(c, a);

					subdividedFaces.insert(subdividedFaces.end(), { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
				}

				faces = std::move(subdividedFaces);
			}

			std::unordered_set<Uint64> seenEdges;
			for (Size faceIndex = 0; faceIndex < faces.size(); faceIndex += 3)
			{
				Uint32 corners[3] = { faces[faceIndex + 0], faces[faceIndex + 1], faces[faceIndex + 2] };

				for (Uint32 cornerIndex = 0; cornerIndex < 3; cornerIndex++)
				{
					Uint32 indexA = corners[cornerIndex];
					Uint32 indexB = corners[(cornerIndex + 1) % 3];

					if (!seenEdges.insert(edgeKey(indexA, indexB)).second)
					{
						continue;
					}

					const Vector3& va = vertices[indexA];
					const Vector3& vb = vertices[indexB];

					outFullEdges.push_back(va);
					outFullEdges.push_back(vb);

					if (va.y >= 0.0f && vb.y >= 0.0f)
					{
						outHemisphereEdges.push_back(va);
						outHemisphereEdges.push_back(vb);
					}
				}
			}
		}
	}

	void ColliderRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache)
	{
		bindlessHeap_ = bindlessHeap;

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ColliderInstance>>(device, bindlessHeap, maxInstanceCount_);
		instanceConstantsBuffer_ = MakePtr<ConstantBuffer<ColliderInstanceConstants>>(device, bindlessHeap);

		BuildIcosphereEdges(icosphereSubdivisionLevel_, sphereEdgeData_, hemisphereEdgeData_);
		sphereEdgeCount_ = static_cast<Uint>(sphereEdgeData_.size() / 2);
		hemisphereEdgeCount_ = static_cast<Uint>(hemisphereEdgeData_.size() / 2);

		sphereEdgeBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Vector3>>(device, bindlessHeap, static_cast<Uint>(sphereEdgeData_.size()));
		hemisphereEdgeBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Vector3>>(device, bindlessHeap, static_cast<Uint>(hemisphereEdgeData_.size()));

		Uint cylinderBodyLineCount = 2 * cylinderRingSegments_ + cylinderVerticalLineCount_;
		Uint capsuleLineCount = cylinderBodyLineCount + 2 * hemisphereEdgeCount_;
		Uint maxLinesPerInstance = std::max({ cylinderBodyLineCount, capsuleLineCount, sphereEdgeCount_, cylinderRingSegments_ });
		groupsPerInstance_ = (maxLinesPerInstance + threadsPerGroup_ - 1) / threadsPerGroup_;

		colliderLineShader_.Create(shaderCache, device, pipelineStateObject_);
	}

	void ColliderRenderer::Clear()
	{
		instances_.clear();
	}

	void ColliderRenderer::AddInstance(ColliderShapeKind shapeKind, const Vector3& position, const Quaternion& rotation, const Vector3& dimensions, const Color& color)
	{
		if (instances_.size() >= maxInstanceCount_)
		{
			return;
		}

		ColliderInstance instance{};
		instance.position_ = position;
		instance.shapeKind_ = static_cast<Uint32>(shapeKind);
		instance.rotation_ = rotation;
		instance.dimensions_ = dimensions;
		instance.color_ = color;

		instances_.push_back(instance);
	}

	void ColliderRenderer::Draw(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex)
	{
		if (instances_.empty())
		{
			return;
		}

		Uint instanceCount = static_cast<Uint>(instances_.size());
		if (instanceCount > maxInstanceCount_)
		{
			instanceCount = maxInstanceCount_;
		}

		instanceBuffer_->Update(instances_.data(), instanceCount);

		/// [JP] トポロジテーブルは Create() 以降不変だが、
		///      ReadOnlyStructuredBuffer はフレームリング式アップロード
		///      バッファなので、全インフライトスロットに行き渡るよう
		///      毎フレーム再アップロードする(ColliderRenderer.h 参照)。
		sphereEdgeBuffer_->Update(sphereEdgeData_.data(), static_cast<Uint>(sphereEdgeData_.size()));
		hemisphereEdgeBuffer_->Update(hemisphereEdgeData_.data(), static_cast<Uint>(hemisphereEdgeData_.size()));

		ColliderInstanceConstants constants{};
		constants.lineVertexBufferIndex_ = instanceBuffer_->Index();
		constants.instanceCount_ = instanceCount;
		constants.groupsPerInstance_ = groupsPerInstance_;
		constants.sphereEdgeBufferIndex_ = sphereEdgeBuffer_->Index();
		constants.sphereEdgeCount_ = sphereEdgeCount_;
		constants.hemisphereEdgeBufferIndex_ = hemisphereEdgeBuffer_->Index();
		constants.hemisphereEdgeCount_ = hemisphereEdgeCount_;
		instanceConstantsBuffer_->Update(constants);

		auto* cmd = cmdList->Get();

		/// [JP] 呼び出し側が渡した色 ＋ 深度（読み取りのみ）を bind する —
		///      ModelRenderer::DrawWireframe/DrawMeshlet と同じパターン。
		///      呼び出し側が depthStencilView の元となる深度リソースを
		///      既に読み取り可能な状態にしている前提。
		cmd->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(colliderLineShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(0, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(1, instanceConstantsBuffer_->Address());

		cmd->SetPipelineState(colliderLineShader_.GetPipelineState());

		/// [JP] 1インスタンスにつき groupsPerInstance_ 個のスレッドグループを
		///      割り当てる — Jolt本家相当密度の球/カプセルはもはや1グループ
		///      (threadsPerGroup_ スレッド)には収まらないため（ColliderLineMS.hlsl
		///      参照）。
		cmd->DispatchMesh(instanceCount * groupsPerInstance_, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
