#include <GraphicsEngine/Renderer/RaytracingRenderer.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Log/DxFail.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Renderer/ModelRenderer.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	RaytracingRenderer::RaytracingRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, RaytracingStateObject& raytracingStateObject)
	{
		shadowRenderer_ = MakePtr<ShadowRenderer>(rootSignature, pipelineStateObject);
		ambientOcclusionRenderer_ = MakePtr<AmbientOcclusionRenderer>(rootSignature, pipelineStateObject);
		subsurfaceScatteringRenderer_ = MakePtr<SubsurfaceScatteringRenderer>(rootSignature, pipelineStateObject);
		reflectionRenderer_ = MakePtr<ReflectionRenderer>(rootSignature, raytracingStateObject, pipelineStateObject);
		refractionRenderer_ = MakePtr<RefractionRenderer>(rootSignature, raytracingStateObject);
		globalIlluminationRenderer_ = MakePtr<GlobalIlluminationRenderer>(rootSignature, raytracingStateObject, pipelineStateObject);
		volumetricCloudScapesRenderer_ = MakePtr<VolumetricCloudScapesRenderer>(rootSignature, pipelineStateObject);
		volumetricStarRenderer_ = MakePtr<VolumetricStarRenderer>(rootSignature, pipelineStateObject);
		weatherParticleRenderer_ = MakePtr<WeatherParticleRenderer>(rootSignature, pipelineStateObject);
		volumetricLightRenderer_ = MakePtr<VolumetricLightRenderer>(rootSignature, pipelineStateObject);
	}

	void RaytracingRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;

		for (Uint frame = 0; frame < FrameRing::frameCount; frame++)
		{
			tlasBindlessIndices_[frame] = bindlessHeap->AllocateIndex();
		}

		shadowRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		ambientOcclusionRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		subsurfaceScatteringRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		reflectionRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		refractionRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		globalIlluminationRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		volumetricCloudScapesRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		volumetricStarRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);
		weatherParticleRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem);
		volumetricLightRenderer_->Create(device, bindlessHeap, shaderCache, indicesSystem, width, height);

		skinnedPositionShader_.Create(shaderCache, device);
	}

	void RaytracingRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		shadowRenderer_->Resize(device, bindlessHeap, width, height);
		ambientOcclusionRenderer_->Resize(device, bindlessHeap, width, height);
		subsurfaceScatteringRenderer_->Resize(device, bindlessHeap, width, height);
		reflectionRenderer_->Resize(device, bindlessHeap, width, height);
		refractionRenderer_->Resize(device, bindlessHeap, width, height);
		globalIlluminationRenderer_->Resize(device, bindlessHeap, width, height);
		volumetricCloudScapesRenderer_->Resize(device, bindlessHeap, width, height);
		volumetricStarRenderer_->Resize(device, bindlessHeap, width, height);
	}

	void RaytracingRenderer::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, World& world, const ModelRenderer& modelRenderer)
	{
		pendingInstances_.clear();
		pendingBlasBuilds_.clear();

		Query<Read<Active>, Read<Mesh>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Mesh& mesh)
			{
				if (!active.active_)
				{
					return;
				}

				Handle<Crister> cristerHandle = modelResource.GetHandle(mesh.meshID_);
				if (cristerHandle.empty())
				{
					return;
				}

				Crister* crister = modelResource.Resolve(loaderSystem, cristerHandle);
				if (!crister || crister->FlatTriangleIndexCount() == 0)
				{
					return;
				}

				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				PendingInstance instance{};
				instance.crister_ = crister;
				instance.worldMatrix_ = actor->GetWorldMatrix();
				instance.entityID_ = entityID;

				const Animator* animator = actor->GetComponent<Animator>();
				if (animator && crister->HasSkinnedRTGeometry())
				{
					Uint32 boneOffset = 0;
					if (modelRenderer.TryGetAnimatedBoneOffset(entityID, boneOffset))
					{
						instance.hasSkeletalPose_ = true;
						instance.boneOffset_ = boneOffset;
					}
				}

				if (!instance.hasSkeletalPose_ && blasCache_.find(crister) == blasCache_.end())
				{
					Bool alreadyPending = false;
					for (const Crister* pending : pendingBlasBuilds_)
					{
						if (pending == crister)
						{
							alreadyPending = true;
							break;
						}
					}
					if (!alreadyPending)
					{
						pendingBlasBuilds_.push_back(crister);
					}
				}

				pendingInstances_.push_back(instance);
			});
	}

	/**
	* [EN]
	* Builds the two small per-Crister GPU tables Reflection.hlsli's
	* ResolveReflectionMaterial reads: the mesh's material list, and a
	* per-triangle index into it (from each SubMesh's materialIndex_ over its
	* own [indexOffset_/3, (indexOffset_+indexCount_)/3) triangle range).
	* Called once per unique Crister from the pendingBlasBuilds_ loop above -
	* same lifecycle as the BLAS, never rebuilt per frame.
	*
	* Both resources live on an UPLOAD heap, written directly via Map/memcpy:
	* they are small, built once, and read only once per ray hit, so the
	* DEFAULT-heap-plus-copy-command dance the vertex/index/BLAS buffers use
	* is not worth the extra code here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Reflection.hlsli の ResolveReflectionMaterial が読む、Crister ごとの
	* 小さな GPU テーブルを2つ構築する: メッシュのマテリアル一覧と、そこへの
	* 三角形ごとのインデックス(各 SubMesh の materialIndex_ を、その
	* [indexOffset_/3, (indexOffset_+indexCount_)/3) の三角形範囲へ書き込んで
	* 作る)。上の pendingBlasBuilds_ ループから、ユニークな Crister ごとに
	* 一度だけ呼ばれる — BLAS と同じライフサイクルで、毎フレーム再構築しない。
	*
	* どちらのリソースも UPLOAD ヒープに置き、Map/memcpy で直接書く: 小さく
	* 一度しか構築せず、レイが当たった時に1回読むだけなので、頂点/インデックス/
	* BLAS バッファが使う DEFAULT ヒープ+コピーコマンドの手間を掛ける価値が
	* ここには無い。
	*/
	void RaytracingRenderer::BuildReflectionMaterialTable(ID3D12Device* device, const Crister* crister)
	{
		HRESULT hr{ S_OK };

		D3D12_HEAP_PROPERTIES uploadHeapProperties{};
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		ReflectionMaterialTable table;

		// ---- マテリアル配列 ----
		{
			const DynamicArray<Material>& materials = crister->Materials();

			DynamicArray<ReflectionMaterialData> materialData;
			if (materials.empty())
			{
				materialData.push_back(ReflectionMaterialData{});
			}
			else
			{
				materialData.reserve(materials.size());
				for (const Material& material : materials)
				{
					ReflectionMaterialData entry{};
					entry.baseColor_[0] = material.baseColor_.R();
					entry.baseColor_[1] = material.baseColor_.G();
					entry.baseColor_[2] = material.baseColor_.B();
					entry.baseColorTextureIndex_ = static_cast<Uint32>(crister->TextureBindlessIndex(material.baseColorTextureIndex_));
					entry.ior_ = material.khr_.ior_.ior_;
					entry.transmissionFactor_ = material.khr_.transmission_.transmissionFactor_;
					entry.volumeAttenuationColor_[0] = material.khr_.volume_.attenuationColor_[0];
					entry.volumeAttenuationColor_[1] = material.khr_.volume_.attenuationColor_[1];
					entry.volumeAttenuationColor_[2] = material.khr_.volume_.attenuationColor_[2];
					entry.volumeAttenuationDistance_ = material.khr_.volume_.attenuationDistance_;
					materialData.push_back(entry);
				}
			}

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = static_cast<Uint64>(materialData.size()) * sizeof(ReflectionMaterialData);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&table.materialsResource_));
			SC_HR_CHECK(hr, "反射マテリアルテーブルの生成に失敗しました");

			void* mapped = nullptr;
			D3D12_RANGE noRead{ 0, 0 };
			hr = table.materialsResource_->Map(0, &noRead, &mapped);
			SC_HR_CHECK(hr, "反射マテリアルテーブルのMapに失敗しました");
			memcpy(mapped, materialData.data(), materialData.size() * sizeof(ReflectionMaterialData));
			table.materialsResource_->Unmap(0, nullptr);

			table.materialsShaderResourceViewIndex_ = bindlessHeap_->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Buffer.NumElements = static_cast<Uint32>(materialData.size());
			shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(ReflectionMaterialData);
			device->CreateShaderResourceView(table.materialsResource_.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(table.materialsShaderResourceViewIndex_));
		}

		// ---- 三角形→マテリアルインデックス ----
		{
			Uint32 triangleCount = std::max(crister->FlatTriangleIndexCount() / 3, 1u);

			/// [JP] SubMesh で埋まらない三角形(あり得ないはずだが)は 0
			///      (=先頭マテリアル)へフォールバックする。
			DynamicArray<Uint32> triangleMaterialIndices(triangleCount, 0);

			for (const SubMesh& subMesh : crister->SubMeshes())
			{
				Uint32 firstTriangle = subMesh.indexOffset_ / 3;
				Uint32 triangleSpan = subMesh.indexCount_ / 3;

				for (Uint32 offset = 0; offset < triangleSpan; offset++)
				{
					Uint32 triangleIndex = firstTriangle + offset;
					if (triangleIndex < triangleCount)
					{
						triangleMaterialIndices[triangleIndex] = subMesh.materialIndex_;
					}
				}
			}

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = static_cast<Uint64>(triangleMaterialIndices.size()) * sizeof(Uint32);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&table.triangleMaterialIndexResource_));
			SC_HR_CHECK(hr, "三角形マテリアルインデックステーブルの生成に失敗しました");

			void* mapped = nullptr;
			D3D12_RANGE noRead{ 0, 0 };
			hr = table.triangleMaterialIndexResource_->Map(0, &noRead, &mapped);
			SC_HR_CHECK(hr, "三角形マテリアルインデックステーブルのMapに失敗しました");
			memcpy(mapped, triangleMaterialIndices.data(), triangleMaterialIndices.size() * sizeof(Uint32));
			table.triangleMaterialIndexResource_->Unmap(0, nullptr);

			table.triangleMaterialIndexShaderResourceViewIndex_ = bindlessHeap_->AllocateIndex();
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Buffer.NumElements = static_cast<Uint32>(triangleMaterialIndices.size());
			shaderResourceViewDesc.Buffer.StructureByteStride = sizeof(Uint32);
			device->CreateShaderResourceView(table.triangleMaterialIndexResource_.Get(), &shaderResourceViewDesc, bindlessHeap_->CPUHandle(table.triangleMaterialIndexShaderResourceViewIndex_));
		}

		reflectionMaterialTableCache_.emplace(crister, std::move(table));
	}

	void RaytracingRenderer::Build(D3D12CommandList* cmdList, ID3D12Device* device, const ModelRenderer& modelRenderer, Float deltaTime, Float nightFactor, const Vector3& cameraPosition, Float totalTime, Float snowIntensity)
	{
		Vector3 wind = Vector3(volumetricCloudScapesSettings_.windSpeed_ * 10.0f, 0.0f, 0.0f);
		tlasBuiltThisFrame_ = false;

		/// [EN] Once the device is removed every D3D12 call fails, so building
		///      acceleration structures only produces a cascade of failed
		///      allocations. Skip the whole RT path and report the real removal
		///      reason once - without this the first visible symptom is the
		///      downstream "TLAS の構築に失敗しました", which points at the wrong
		///      subsystem. Nothing can actually render past this point; the goal
		///      is a single accurate log line, not recovery (device loss is
		///      process-wide and cannot be undone per-subsystem).
		/// [JP] デバイスが削除されると全ての D3D12 呼び出しが失敗するため、
		///      加速構造を構築しようとしても確保失敗が連鎖するだけ。RT 経路を
		///      丸ごとスキップし、本当の削除理由を 1 度だけ報告する — これが無いと
		///      最初に見える症状が下流の「TLAS の構築に失敗しました」になり、
		///      原因を別のサブシステムだと誤認させる。この時点で描画は継続不能で、
		///      狙いは復帰ではなく正確なログ 1 行(デバイス削除はプロセス全体の
		///      事象で、サブシステム単位では取り消せない)。
		HRESULT deviceRemovedReason = device->GetDeviceRemovedReason();
		if (FAILED(deviceRemovedReason))
		{
			if (!deviceRemovedLogged_)
			{
				_com_error removedError(deviceRemovedReason);
				SC_LOG_ERROR("GPU デバイスが削除されているため、レイトレーシングを停止しました。理由: {:#010x} ({})", static_cast<Uint32>(deviceRemovedReason), ConvertToCharString(removedError.ErrorMessage()));
				deviceRemovedLogged_ = true;
			}

			indicesSystem_->SetTLASIndex(TLASBindlessIndex());
			return;
		}

		/// [JP] 全 RT エフェクトが無効の間は BLAS/TLAS 構築を丸ごとスキップして
		///      GPU コストをゼロにする(TLASBindlessIndex() は 0xFFFFFFFF のまま=
		///      既存の「TLAS 無し」フォールバックと同じ経路 — 各 Dispatch が
		///      自動的に照射/開放(1.0)クリアへ倒れる)。pendingInstances_/
		///      pendingBlasBuilds_ は Gather() が積んだままなので、次に有効化
		///      された時にそのまま使える。
		/// [JP] 雲は TLAS を使わないため、この BLAS/TLAS スキップ判定には
		///      含めない(PrepareFrame は両経路で呼ぶ)。
		if (!shadowEnabled_ && !ambientOcclusionEnabled_ && !subsurfaceScatteringEnabled_ && !reflectionEnabled_ && !refractionEnabled_ && !globalIlluminationEnabled_ && !volumetricLightEnabled_)
		{
			indicesSystem_->SetTLASIndex(TLASBindlessIndex());
			shadowRenderer_->PrepareFrame(shadowSettings_);
			ambientOcclusionRenderer_->PrepareFrame(ambientOcclusionSettings_, dlssRayReconstructionEnabled_);
			subsurfaceScatteringRenderer_->PrepareFrame(subsurfaceScatteringSettings_);
			reflectionRenderer_->PrepareFrame(reflectionSettings_, dlssRayReconstructionEnabled_);
			refractionRenderer_->PrepareFrame(refractionSettings_);
			globalIlluminationRenderer_->PrepareFrame(globalIlluminationSettings_, dlssRayReconstructionEnabled_);
			volumetricCloudScapesRenderer_->PrepareFrame(volumetricCloudScapesSettings_, volumetricCloudScapesEnabled_);
			volumetricStarRenderer_->PrepareFrame(volumetricStarSettings_, volumetricStarEnabled_, deltaTime, nightFactor);
			weatherParticleRenderer_->PrepareFrame(cameraPosition, deltaTime, totalTime, wind, rainEnabled_, rainSettings_, volumetricCloudScapesSettings_.rain_, snowEnabled_, snowSettings_, snowIntensity);
			volumetricLightRenderer_->PrepareFrame(volumetricLightSettings_);
			return;
		}

		/// [EN] The device is always created as ID3D12Device5 (see D3D12Device),
		///      and the command list's underlying ID3D12GraphicsCommandList6
		///      already satisfies ID3D12GraphicsCommandList4 — both DXR AS-build
		///      APIs are available without any capability check here.
		/// [JP] デバイスは常に ID3D12Device5 として生成されており（D3D12Device
		///      参照）、コマンドリストの実体である ID3D12GraphicsCommandList6 も
		///      ID3D12GraphicsCommandList4 を満たす — ここで能力チェックせずに
		///      両方の DXR AS 構築 API を使える。
		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(device);
		ID3D12GraphicsCommandList4* commandList4 = cmdList->Get();

		for (const Crister* crister : pendingBlasBuilds_)
		{
			BottomLevelGeometryDesc geometryDesc{};
			geometryDesc.vertexBuffer_ = crister->VertexBufferGPUAddress();
			geometryDesc.vertexCount_ = crister->VertexCount();
			geometryDesc.vertexStride_ = sizeof(Vector3);
			geometryDesc.vertexFormat_ = DXGI_FORMAT_R32G32B32_FLOAT;
			geometryDesc.indexBuffer_ = crister->FlatTriangleIndexBufferGPUAddress();
			geometryDesc.indexCount_ = crister->FlatTriangleIndexCount();
			geometryDesc.indexFormat_ = DXGI_FORMAT_R32_UINT;
			geometryDesc.opaque_ = true;

			ResourcePtr<BottomLevelAccelerationStructure> blas = MakePtr<BottomLevelAccelerationStructure>();
			if (blas->Build(device5, commandList4, &geometryDesc, 1))
			{
				blasCache_.emplace(crister, std::move(blas));
			}
			else if (!blasBuildFailureLogged_)
			{
				SC_LOG_WARNING("BLAS の構築に失敗しました。DXR(レイトレーシング Tier 1.0 以上)非対応ハードウェアの可能性があります。影は常に照射(1.0)として扱われます。");
				blasBuildFailureLogged_ = true;
			}

			/// [JP] 反射/GI 用のマテリアルテーブルも、このメッシュを初めて見た
			///      このタイミングで一度だけ構築する(BLAS と同じライフサイクル)。
			BuildReflectionMaterialTable(device, crister);
		}
		pendingBlasBuilds_.clear();

		Uint frameIndex = FrameRing::Index();

		for (const PendingInstance& pending : pendingInstances_)
		{
			if (!pending.hasSkeletalPose_)
			{
				continue;
			}

			const Crister* crister = pending.crister_;
			Uint32 vertexCount = crister->VertexCount();

			SkinnedPositionBuffer& skinnedBuffer = skinnedPositionBuffers_[frameIndex][pending.entityID_];
			if (skinnedBuffer.capacity_ < vertexCount)
			{
				D3D12_HEAP_PROPERTIES heapProperties{};
				heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
				heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapProperties.CreationNodeMask = 1;
				heapProperties.VisibleNodeMask = 1;

				D3D12_RESOURCE_DESC resourceDesc{};
				resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resourceDesc.Width = sizeof(Vector3) * static_cast<Uint64>(vertexCount);
				resourceDesc.Height = 1;
				resourceDesc.DepthOrArraySize = 1;
				resourceDesc.MipLevels = 1;
				resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
				resourceDesc.SampleDesc.Count = 1;
				resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(skinnedBuffer.resource_.ReleaseAndGetAddressOf()));
				skinnedBuffer.resource_->SetName(L"SkinnedPositionBuffer");
				skinnedBuffer.capacity_ = vertexCount;
			}

			struct SkinnedPositionParams
			{
				Uint32 vertexCount_;
				Uint32 boneOffset_;
				Uint32 pad0_;
				Uint32 pad1_;
			};

			SkinnedPositionParams params{ vertexCount, pending.boneOffset_, 0, 0 };

			commandList4->SetPipelineState(skinnedPositionShader_.GetPipelineState());
			commandList4->SetComputeRootSignature(skinnedPositionShader_.GetRootSignature());
			commandList4->SetComputeRoot32BitConstants(0, 4, &params, 0);
			commandList4->SetComputeRootShaderResourceView(1, crister->RTPositionBufferGPUAddress());
			commandList4->SetComputeRootShaderResourceView(2, crister->RTSkinVertexBufferGPUAddress());
			commandList4->SetComputeRootShaderResourceView(3, modelRenderer.BoneMatrixBufferGPUAddress());
			commandList4->SetComputeRootUnorderedAccessView(4, skinnedBuffer.resource_->GetGPUVirtualAddress());
			commandList4->Dispatch((vertexCount + 63) / 64, 1, 1);

			D3D12_RESOURCE_BARRIER uavBarrier{};
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uavBarrier.UAV.pResource = skinnedBuffer.resource_.Get();
			commandList4->ResourceBarrier(1, &uavBarrier);

			BottomLevelGeometryDesc geometryDesc{};
			geometryDesc.vertexBuffer_ = skinnedBuffer.resource_->GetGPUVirtualAddress();
			geometryDesc.vertexCount_ = vertexCount;
			geometryDesc.vertexStride_ = sizeof(Vector3);
			geometryDesc.vertexFormat_ = DXGI_FORMAT_R32G32B32_FLOAT;
			geometryDesc.indexBuffer_ = crister->FlatTriangleIndexBufferGPUAddress();
			geometryDesc.indexCount_ = crister->FlatTriangleIndexCount();
			geometryDesc.indexFormat_ = DXGI_FORMAT_R32_UINT;
			geometryDesc.opaque_ = true;

			ResourcePtr<BottomLevelAccelerationStructure>& skinnedBlas = skinnedBlasCache_[frameIndex][pending.entityID_];
			if (!skinnedBlas)
			{
				skinnedBlas = MakePtr<BottomLevelAccelerationStructure>();
			}

			if (!skinnedBlas->Build(device5, commandList4, &geometryDesc, 1) && !skinnedBlasBuildFailureLogged_)
			{
				SC_LOG_WARNING("スキン付き BLAS の構築に失敗しました。対象アクターは TLAS から除外されます。");
				skinnedBlasBuildFailureLogged_ = true;
			}
		}

		if (!pendingInstances_.empty())
		{
			DynamicArray<TopLevelInstanceDesc> instanceDescs;
			instanceDescs.reserve(pendingInstances_.size());

			/// [JP] TLAS インスタンスと同じ順序で、closesthit が InstanceID() で
			///      引くインスタンステーブル(頂点/インデックスSRV+マテリアル
			///      近似色)も詰める。instanceID_ = instanceDescs 内の位置。
			DynamicArray<ReflectionInstanceData> reflectionInstances;
			reflectionInstances.reserve(pendingInstances_.size());

			for (const PendingInstance& pending : pendingInstances_)
			{
				/// [JP] スキン付きは今フレーム構築したスキン済み BLAS を優先。
				///      構築できなかったフレームは静的(バインドポーズ)BLAS へ
				///      フォールバックする。
				D3D12_GPU_VIRTUAL_ADDRESS bottomLevelAddress = 0;

				if (pending.hasSkeletalPose_)
				{
					auto found = skinnedBlasCache_[frameIndex].find(pending.entityID_);
					if (found == skinnedBlasCache_[frameIndex].end())
					{
						continue;
					}
					bottomLevelAddress = found->second->Address();
				}
				else
				{
					auto found = blasCache_.find(pending.crister_);
					if (found == blasCache_.end())
					{
						continue;
					}
					bottomLevelAddress = found->second->Address();
				}

				/// [EN] Final safety net: Address() is 0 whenever the BLAS behind it
				///      has never built successfully (first-ever build failure for
				///      this frame-ring slot, before any prior success to fall back
				///      on). Feeding a null AccelerationStructure into a TLAS instance
				///      is what corrupts/fails the TLAS build, so drop the instance
				///      instead.
				/// [JP] 最終安全弁: 背後の BLAS が一度も構築成功していない場合(この
				///      フレームリングスロットで初めての構築失敗で、フォールバック
				///      できる過去の成功ビルドも無い場合)、Address() は 0 になる。
				///      null の AccelerationStructure を TLAS インスタンスへ渡すと
				///      TLAS 構築が壊れる/失敗するため、そのインスタンスを除外する。
				if (bottomLevelAddress == 0)
				{
					continue;
				}

				TopLevelInstanceDesc instanceDesc{};
				instanceDesc.bottomLevelAddress_ = bottomLevelAddress;
				instanceDesc.instanceMask_ = 0xFF;
				instanceDesc.instanceID_ = static_cast<Uint32>(instanceDescs.size());
				instanceDesc.hitGroupIndex_ = 0;

				ReflectionInstanceData reflectionInstance{};
				reflectionInstance.vertexBufferIndex_ = pending.crister_->VertexBufferIndex();
				reflectionInstance.indexBufferIndex_ = pending.crister_->FlatTriangleIndexShaderResourceViewIndex();

				/// [JP] マテリアルは三角形ごとに BuildReflectionMaterialTable が
				///      焼いたテーブル経由で解決する(pendingBlasBuilds_ で
				///      この Crister が初出のときに一度だけ構築済みのはず — 万一
				///      未構築なら bindless インデックス 0 のまま渡り、
				///      closesthit 側で読む配列が存在しないため、そちらは表示
				///      すら出ない形で気付ける)。
				auto materialTable = reflectionMaterialTableCache_.find(pending.crister_);
				if (materialTable != reflectionMaterialTableCache_.end())
				{
					reflectionInstance.materialDataIndex_ = materialTable->second.materialsShaderResourceViewIndex_;
					reflectionInstance.triangleMaterialIndexBufferIndex_ = materialTable->second.triangleMaterialIndexShaderResourceViewIndex_;
				}

				/// [JP] UV の量子化 AABB。これが無いと closesthit が圧縮頂点の UV を
				///      復元できない(ラスタ経路が ModelRenderer 経由で渡しているのと
				///      同じ値)。
				Vector2 texcoordMin = pending.crister_->TexcoordMin();
				Vector2 texcoordExtent = pending.crister_->TexcoordExtent();
				reflectionInstance.texcoordMin_[0] = texcoordMin.x;
				reflectionInstance.texcoordMin_[1] = texcoordMin.y;
				reflectionInstance.texcoordExtent_[0] = texcoordExtent.x;
				reflectionInstance.texcoordExtent_[1] = texcoordExtent.y;

				reflectionInstances.push_back(reflectionInstance);

				/// [EN] Convert the engine's row-major, row-vector matrix (translation
				///      in row 3, p' = p * M) to DXR's column-vector 3x4 (translation
				///      in column 3, p' = M * p): transpose the upper-left 3x3, copy
				///      the translation row into the translation column.
				/// [JP] エンジンの行優先・行ベクトル行列（並進は行3、p' = p * M）を
				///      DXR の列ベクトル 3x4（並進は列3、p' = M * p）へ変換する:
				///      左上 3x3 を転置し、並進の行を並進の列へコピーする。
				for (Int row = 0; row < 3; row++)
				{
					for (Int col = 0; col < 3; col++)
					{
						instanceDesc.transform_[row][col] = pending.worldMatrix_.m[col][row];
					}
					instanceDesc.transform_[row][3] = pending.worldMatrix_.m[3][row];
				}

				instanceDescs.push_back(instanceDesc);
			}

			if (!instanceDescs.empty())
			{
				reflectionRenderer_->UpdateInstanceTable(reflectionInstances.data(), static_cast<Uint32>(reflectionInstances.size()));

				if (tlas_.Build(device5, commandList4, instanceDescs.data(), static_cast<Uint32>(instanceDescs.size())))
				{
					Uint frame = FrameRing::Index();
					D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
					shaderResourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
					shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
					shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					shaderResourceViewDesc.RaytracingAccelerationStructure.Location = tlas_.Address();

					/// [EN] Acceleration-structure SRVs are created with a null resource
					///      pointer — the location comes from the view desc, not a resource.
					/// [JP] アクセラレーション構造 SRV はリソースポインタに null を渡して作る
					///      — 位置情報はリソースではなく view desc 側から来る。
					device->CreateShaderResourceView(nullptr, &shaderResourceViewDesc, bindlessHeap_->CPUHandle(tlasBindlessIndices_[frame]));

					tlasBuiltThisFrame_ = true;
				}
				else if (!tlasBuildFailureLogged_)
				{
					SC_LOG_WARNING("TLAS の構築に失敗しました。影は常に照射(1.0)として扱われます。");
					tlasBuildFailureLogged_ = true;
				}
			}
		}

		/// [EN] Always runs, success or not, so IndicesSystem and ShadowRenderer
		///      see a consistent 0xFFFFFFFF (fallback) when nothing was built.
		/// [JP] 成否に関わらず必ず実行する。何も構築されなかった場合、
		///      IndicesSystem と ShadowRenderer には一貫して 0xFFFFFFFF
		///      （フォールバック）が見える。
		indicesSystem_->SetTLASIndex(TLASBindlessIndex());
		shadowRenderer_->PrepareFrame(shadowSettings_);
		ambientOcclusionRenderer_->PrepareFrame(ambientOcclusionSettings_, dlssRayReconstructionEnabled_);
		subsurfaceScatteringRenderer_->PrepareFrame(subsurfaceScatteringSettings_);
		reflectionRenderer_->PrepareFrame(reflectionSettings_, dlssRayReconstructionEnabled_);
		refractionRenderer_->PrepareFrame(refractionSettings_);
		globalIlluminationRenderer_->PrepareFrame(globalIlluminationSettings_, dlssRayReconstructionEnabled_);
		volumetricCloudScapesRenderer_->PrepareFrame(volumetricCloudScapesSettings_, volumetricCloudScapesEnabled_);
		volumetricStarRenderer_->PrepareFrame(volumetricStarSettings_, volumetricStarEnabled_, deltaTime, nightFactor);
		weatherParticleRenderer_->PrepareFrame(cameraPosition, deltaTime, totalTime, wind, rainEnabled_, rainSettings_, volumetricCloudScapesSettings_.rain_, snowEnabled_, snowSettings_, snowIntensity);
		volumetricLightRenderer_->PrepareFrame(volumetricLightSettings_);
	}

	void RaytracingRenderer::DispatchShadow(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view)
	{
		/// [JP] AO のみ有効で TLAS がある場合でも、影が無効なら影は全照射クリアに
		///      倒す(tlasValid && enabled で個別ゲート)。
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		shadowRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && shadowEnabled_, view);
	}

	void RaytracingRenderer::DispatchAmbientOcclusion(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view)
	{
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		ambientOcclusionRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && ambientOcclusionEnabled_, view, dlssRayReconstructionEnabled_);
	}

	void RaytracingRenderer::DispatchSubsurfaceScattering(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		subsurfaceScatteringRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && subsurfaceScatteringEnabled_);
	}

	void RaytracingRenderer::DispatchReflection(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view)
	{
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		reflectionRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && reflectionEnabled_, view, dlssRayReconstructionEnabled_);
	}

	void RaytracingRenderer::DispatchRefraction(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		refractionRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && refractionEnabled_);
	}

	void RaytracingRenderer::DispatchGlobalIllumination(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex, RaytracingView view)
	{
		Bool tlasValid = TLASBindlessIndex() != 0xFFFFFFFF;
		globalIlluminationRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, tlasValid && globalIlluminationEnabled_, view, dlssRayReconstructionEnabled_);
	}

	void RaytracingRenderer::DispatchVolumetricCloudScapes(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		volumetricCloudScapesRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, volumetricCloudScapesEnabled_);
	}

	void RaytracingRenderer::DispatchVolumetricStar(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		volumetricStarRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, volumetricStarEnabled_);
	}

	void RaytracingRenderer::SimulateWeatherParticles(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		weatherParticleRenderer_->Simulate(cmdList, heap, constantIndex, structuredIndex);
	}

	void RaytracingRenderer::DrawWeatherParticles(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		weatherParticleRenderer_->Draw(cmdList, frameBuffer, geometryBuffer, heap, constantIndex, structuredIndex);
	}

	void RaytracingRenderer::DispatchVolumetricLight(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		volumetricLightRenderer_->Dispatch(cmdList, heap, constantIndex, structuredIndex, volumetricLightEnabled_);
	}

	void RaytracingRenderer::SetRaytracingSettings(const RaytracingContext& settings)
	{
		shadowSettings_ = settings.shadow_;
		shadowEnabled_ = settings.shadowEnabled_;

		ambientOcclusionSettings_ = settings.ambientOcclusion_;
		ambientOcclusionEnabled_ = settings.ambientOcclusionEnabled_;

		subsurfaceScatteringSettings_ = settings.subsurfaceScattering_;
		subsurfaceScatteringEnabled_ = settings.subsurfaceScatteringEnabled_;

		reflectionSettings_ = settings.reflection_;
		reflectionEnabled_ = settings.reflectionEnabled_;
		refractionSettings_ = settings.refraction_;
		refractionEnabled_ = settings.refractionEnabled_;
		globalIlluminationSettings_ = settings.globalIllumination_;
		globalIlluminationEnabled_ = settings.globalIlluminationEnabled_;

		volumetricCloudScapesSettings_ = settings.volumetricCloudScapes_;
		volumetricCloudScapesEnabled_ = settings.volumetricCloudScapesEnabled_;

		/// [JP] SunLight 有効時は、そちらの太陽視半径をプロシージャル空の太陽
		///      ディスク描画(ProceduralSkyColor)へ流し込んで正とする。
		if (settings.sunLightEnabled_)
		{
			volumetricCloudScapesSettings_.sunSize_ = settings.sunLight_.angularRadius_;
		}

		volumetricStarSettings_ = settings.volumetricStar_;
		volumetricStarEnabled_ = settings.volumetricStarEnabled_;

		rainEnabled_ = settings.rainEnabled_;
		rainSettings_ = settings.rain_;
		snowEnabled_ = settings.snowEnabled_;
		snowSettings_ = settings.snow_;

		volumetricLightSettings_ = settings.volumetricLight_;
		volumetricLightEnabled_ = settings.volumetricLightEnabled_;

		dlssRayReconstructionEnabled_ = settings.dlssRayReconstructionEnabled_;

		/// [JP] Shadowは自前のデノイズモードフィールド(ShadowRayConstantBuffer::
		///      denoiseMode_)を持つ既存スキャフォールドがあるので、グローバル
		///      トグルをそこへ流し込む(AO/GIのように別引数にしない)。
		shadowSettings_.denoiseMode_ = dlssRayReconstructionEnabled_ ? static_cast<Uint32>(ShadowDenoiseMode::DlssRR) : static_cast<Uint32>(ShadowDenoiseMode::Temporal);
	}

	Uint32 RaytracingRenderer::TLASBindlessIndex()const
	{
		if (!tlasBuiltThisFrame_)
		{
			return 0xFFFFFFFF;
		}
		return tlasBindlessIndices_[FrameRing::Index()];
	}
}
