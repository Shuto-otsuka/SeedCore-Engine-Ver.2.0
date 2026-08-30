#include <GraphicsEngine/Renderer/MaterialRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Material/MaterialResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

namespace SeedCore
{
	MaterialRenderer::MaterialRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : modelShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	MaterialRenderer::~MaterialRenderer() = default;

	void MaterialRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		maxInstanceCount_ = 2048;
		maxBoneCount_ = 2048;

		modelShader_.Create(shaderCache, device);

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelInstanceData>>(device, bindlessHeap, maxInstanceCount_);
		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_ = MakePtr<FrameBuffer>(device, &renderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &depthStencilViewHeap_, 0.45f, 0.65f, 0.9f, 1.0f);

		sceneSystem_ = MakePtr<SceneSystem>(device, bindlessHeap);

		constantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap);
		structuredIndicesBuffer_ = MakePtr<ConstantBuffer<StructuredIndices>>(device, bindlessHeap);
	}

	void MaterialRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_->Resize(device, bindlessHeap, width, height);
	}

	void MaterialRenderer::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, MaterialResource& materialResource, Uint32 meshAssetId, Uint32 surfaceAssetId, const Matrix& worldMatrix)
	{
		opaqueInstances_.clear();
		transparentInstances_.clear();
		boneMatrices_.clear();
		hasSkinnedOpaque_ = false;
		uploaded_ = false;

		Handle<Crister> cristerHandle = modelResource.GetHandle(meshAssetId);
		if (cristerHandle.empty())
		{
			return;
		}

		Crister* crister = modelResource.Resolve(loaderSystem, cristerHandle);
		if (!crister)
		{
			return;
		}

		Matrix inverseTransposeWorld = worldMatrix.Invert().Transpose();

		const auto& subMeshes = crister->SubMeshes();
		const auto& surfaces = crister->Surfaces();
		const auto& clusters = crister->Clusters();
		const auto& skins = crister->Skins();
		const auto& nodes = crister->Nodes();

		/// [EN] The one Surface the viewer selected, applied to every submesh.
		///      Null when surfaceAssetId is 0 (or not loaded) - each submesh
		///      then falls back to the mesh's own embedded surface.
		/// [JP] ビューアが選択した1つの Surface。全 submesh へ適用する。
		///      surfaceAssetId が 0(または未ロード)なら null - 各 submesh は
		///      メッシュ内蔵の Surface へフォールバックする。
		Surface* previewSurface = surfaceAssetId != 0 ? materialResource.Resolve(loaderSystem, materialResource.GetHandle(surfaceAssetId)) : nullptr;

		Uint boneBase = 0;
		Bool boneOverflow = false;
		if (!skins.empty())
		{
			Size totalJoints = 0;
			for (const Skin& skin : skins)
			{
				totalJoints += skin.joints_.size();
			}

			if (totalJoints > maxBoneCount_)
			{
				boneOverflow = true;
			}
			else
			{
				for (const Skin& skin : skins)
				{
					for (Size joint = 0; joint < skin.joints_.size(); joint++)
					{
						const Matrix& globalTransform = nodes[skin.joints_[joint]].globalTransform_;

						if (joint < skin.inverseBindMatrices_.size())
						{
							boneMatrices_.push_back(skin.inverseBindMatrices_[joint] * globalTransform);
						}
						else
						{
							boneMatrices_.push_back(globalTransform);
						}
					}
				}
			}
		}

		for (const SubMesh& subMesh : subMeshes)
		{
			const Surface& material = previewSurface ? *previewSurface : surfaces[subMesh.surfaceIndex_];

			Bool skinned = subMesh.skinIndex_ >= 0 && subMesh.skinIndex_ < static_cast<Int>(skins.size()) && !boneOverflow;

			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			DynamicArray<Uint32> residentClusters;
			if (skinned)
			{
				residentClusters.push_back(subMesh.clusterOffset_);
			}
			else
			{
				Uint32 desired = subMesh.clusterCount_ - 1;
				if (!crister->IsClusterResident(subMesh.clusterOffset_ + desired))
				{
					streamingRequests_.push_back({ crister, subMesh.clusterOffset_ + desired });
				}

				for (Uint32 c = 0; c < subMesh.clusterCount_; ++c)
				{
					if (crister->IsClusterResident(subMesh.clusterOffset_ + c))
					{
						residentClusters.push_back(subMesh.clusterOffset_ + c);
					}
				}
			}

			for (Size residentIndex = 0; residentIndex < residentClusters.size(); residentIndex++)
			{
				Uint32 clusterIndex = residentClusters[residentIndex];
				const Cluster& cluster = clusters[clusterIndex];
				crister->TouchCluster(clusterIndex, streamingFrame_);

				Float lodErrorNext = FLT_MAX;
				if (!skinned && residentIndex + 1 < residentClusters.size())
				{
					lodErrorNext = clusters[residentClusters[residentIndex + 1]].lodError_;
				}

				constexpr Uint32 maxMeshletsPerDispatch = 32;
				Uint32 remaining = cluster.meshletCount_;
				Uint32 offset = cluster.meshletOffset_;

				while (remaining > 0)
				{
					Uint32 count = (remaining > maxMeshletsPerDispatch) ? maxMeshletsPerDispatch : remaining;

					ModelInstanceData instanceData{};
					instanceData.world_ = worldMatrix;
					instanceData.inverseTransposeWorld_ = inverseTransposeWorld;

					instanceData.baseColor_ = material.baseColor_;
					instanceData.metallic_ = material.metallic_;
					instanceData.roughness_ = material.roughness_;
					instanceData.alphaCutoff_ = material.alphaMode_ == 1 ? material.alphaCutoff_ : (material.alphaMode_ == 2 ? 0.01f : 0.0f);
					instanceData.emissive_ = Vector3(material.emissiveFactor_[0], material.emissiveFactor_[1], material.emissiveFactor_[2]);

					instanceData.ior_ = material.khr_.ior_.ior_;
					instanceData.emissiveStrength_ = material.khr_.emissiveStrength_.emissiveStrength_;
					instanceData.specularFactor_ = material.khr_.specular_.specularFactor_;
					instanceData.specularColor_ = Vector3(material.khr_.specular_.specularColorFactor_[0], material.khr_.specular_.specularColorFactor_[1], material.khr_.specular_.specularColorFactor_[2]);
					instanceData.clearCoatFactor_ = material.khr_.clearCoat_.clearCoatFactor_;
					instanceData.clearCoatRoughness_ = material.khr_.clearCoat_.clearCoatRoughnessFactor_;
					instanceData.anisotropy_ = material.khr_.anisotropy_.anisotropyStrength_;
					instanceData.transmissionFactor_ = material.khr_.transmission_.transmissionFactor_;
					instanceData.volumeThicknessFactor_ = material.khr_.volume_.thicknessFactor_;
					instanceData.volumeAttenuationDistance_ = material.khr_.volume_.attenuationDistance_;
					instanceData.volumeAttenuationColor_ = Vector3(material.khr_.volume_.attenuationColor_[0], material.khr_.volume_.attenuationColor_[1], material.khr_.volume_.attenuationColor_[2]);
					instanceData.sheenColor_ = Vector3(material.khr_.sheen_.sheenColorFactor_[0], material.khr_.sheen_.sheenColorFactor_[1], material.khr_.sheen_.sheenColorFactor_[2]);
					instanceData.sheenRoughness_ = material.khr_.sheen_.sheenRoughnessFactor_;
					instanceData.iridescenceFactor_ = material.khr_.iridescence_.iridescenceFactor_;
					instanceData.iridescenceIor_ = material.khr_.iridescence_.iridescenceIor_;
					instanceData.iridescenceThickness_ = (material.khr_.iridescence_.iridescenceThicknessMinimum_ + material.khr_.iridescence_.iridescenceThicknessMaximum_) * 0.5f;
					instanceData.unlit_ = material.khr_.unlit_.unlit_ != 0 ? 1.0f : 0.0f;
					instanceData.shadingModel_ = material.khr_.unlit_.unlit_ != 0 ? static_cast<Uint>(ShadingModel::Unlit) : static_cast<Uint>(material.shadingModel_);

					instanceData.baseColorTextureIndex_ = crister->TextureBindlessIndex(material.baseColorTextureIndex_);
					instanceData.normalTextureIndex_ = crister->TextureBindlessIndex(material.normalTextureIndex_);
					instanceData.metallicRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.metallicRoughnessTextureIndex_);
					instanceData.emissiveTextureIndex_ = crister->TextureBindlessIndex(material.emissiveTextureIndex_);
					instanceData.occlusionTextureIndex_ = crister->TextureBindlessIndex(material.occlusionTextureIndex_);
					instanceData.specularTextureIndex_ = crister->TextureBindlessIndex(material.khr_.specular_.specularTextureIndex_);
					instanceData.specularColorTextureIndex_ = crister->TextureBindlessIndex(material.khr_.specular_.specularColorTextureIndex_);
					instanceData.clearCoatTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatTextureIndex_);
					instanceData.clearCoatRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatRoughnessTextureIndex_);
					instanceData.clearCoatNormalTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatNormalTextureIndex_);
					instanceData.transmissionTextureIndex_ = crister->TextureBindlessIndex(material.khr_.transmission_.transmissionTextureIndex_);
					instanceData.thicknessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.volume_.thicknessTextureIndex_);
					instanceData.sheenColorTextureIndex_ = crister->TextureBindlessIndex(material.khr_.sheen_.sheenColorTextureIndex_);
					instanceData.sheenRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.sheen_.sheenRoughnessTextureIndex_);
					instanceData.iridescenceTextureIndex_ = crister->TextureBindlessIndex(material.khr_.iridescence_.iridescenceTextureIndex_);
					instanceData.iridescenceThicknessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.iridescence_.iridescenceThicknessTextureIndex_);
					instanceData.anisotropyTextureIndex_ = crister->TextureBindlessIndex(material.khr_.anisotropy_.anisotropyTextureIndex_);
					instanceData.anisotropyRotation_ = material.khr_.anisotropy_.anisotropyRotation_;

					instanceData.vertexBufferIndex_ = crister->ClusterVertexBufferIndex(clusterIndex);
					instanceData.skinVertexBufferIndex_ = crister->SkinVertexBufferIndex();
					instanceData.positionMin_ = crister->PositionMin();
					instanceData.positionExtent_ = crister->PositionExtent();
					instanceData.texcoordMinU_ = crister->TexcoordMin().x;
					instanceData.texcoordMinV_ = crister->TexcoordMin().y;
					instanceData.texcoordExtent_ = crister->TexcoordExtent();
					instanceData.meshletBufferIndex_ = crister->ClusterMeshletBufferIndex(clusterIndex);
					instanceData.meshletBoundBufferIndex_ = crister->ClusterMeshletBoundBufferIndex(clusterIndex);
					instanceData.vertexIndicesBufferIndex_ = crister->ClusterVertexIndicesBufferIndex(clusterIndex);
					instanceData.primitiveIndicesBufferIndex_ = crister->ClusterPrimitiveIndicesBufferIndex(clusterIndex);

					instanceData.meshletOffset_ = offset - cluster.meshletOffset_;
					instanceData.meshletCount_ = count;

					instanceData.lodError_ = cluster.lodError_;
					instanceData.lodErrorNext_ = lodErrorNext;

					if (skinned)
					{
						Uint skinBoneOffset = boneBase;
						for (Int skinIndex = 0; skinIndex < subMesh.skinIndex_; skinIndex++)
						{
							skinBoneOffset += static_cast<Uint>(skins[skinIndex].joints_.size());
						}
						instanceData.skinIndex_ = static_cast<Uint>(subMesh.skinIndex_);
						instanceData.boneOffset_ = skinBoneOffset;
					}
					else
					{
						instanceData.skinIndex_ = 0xFFFFFFFF;
						instanceData.boneOffset_ = 0;
					}

					instanceData.doubleSided_ = material.doubleSided_ ? 1 : 0;
					instanceData.blend_ = material.alphaMode_ == 2 ? 1 : 0;
					instanceData.selected_ = 0;

					if (material.alphaMode_ != 2)
					{
						opaqueInstances_.push_back(instanceData);
						hasSkinnedOpaque_ = hasSkinnedOpaque_ || instanceData.skinIndex_ != 0xFFFFFFFF;
					}
					else
					{
						transparentInstances_.push_back(instanceData);
					}

					offset += count;
					remaining -= count;
				}
			}
		}
	}

	void MaterialRenderer::Upload()
	{
		if (uploaded_)
		{
			return;
		}
		uploaded_ = true;

		structuredIndices_.model_.instanceIndex_ = instanceBuffer_->Index();
		structuredIndices_.model_.boneMatrixIndex_ = boneBuffer_->Index();

		if (!opaqueInstances_.empty() || !transparentInstances_.empty())
		{
			DynamicArray<ModelInstanceData> allInstances;
			allInstances.reserve(opaqueInstances_.size() + transparentInstances_.size());
			allInstances.insert(allInstances.end(), opaqueInstances_.begin(), opaqueInstances_.end());
			allInstances.insert(allInstances.end(), transparentInstances_.begin(), transparentInstances_.end());

			instanceBuffer_->Update(allInstances.data(), static_cast<Uint>(allInstances.size()));
		}

		if (!boneMatrices_.empty())
		{
			boneBuffer_->Update(boneMatrices_.data(), static_cast<Uint>(boneMatrices_.size()));
		}
	}

	void MaterialRenderer::Begin(D3D12CommandList* cmdList)
	{
		frameBuffer_->Begin(cmdList);
		frameBuffer_->Clear(cmdList, 0.45f, 0.65f, 0.9f, 1.0f);
	}

	void MaterialRenderer::Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene)
	{
		sceneSystem_->Upload(scene);

		constantIndices_.sceneIndex_ = sceneSystem_->GetIndex();
		constantIndicesBuffer_->Update(constantIndices_);
		structuredIndicesBuffer_->Update(structuredIndices_);

		if (opaqueInstances_.empty())
		{
			return;
		}

		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = constantIndicesBuffer_->Address();
		D3D12_GPU_VIRTUAL_ADDRESS structuredAddr = structuredIndicesBuffer_->Address();

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantAddr);
		cmd->SetGraphicsRootConstantBufferView(3, structuredAddr);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewStatic());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		if (hasSkinnedOpaque_)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewSkeletal());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
	}

	void MaterialRenderer::End(D3D12CommandList* cmdList)
	{
		frameBuffer_->End(cmdList);
	}

	void MaterialRenderer::RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap)
	{
		Bool alreadyRegistered = imguiHeap_ != nullptr;
		imguiHeap_ = imguiHeap;

		if (!alreadyRegistered)
		{
			imguiShaderResourceViewIndex_ = imguiHeap->AllocateIndex();
		}

		D3D12_RESOURCE_DESC desc = frameBuffer_->ColorResource()->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription{};
		shaderResourceViewDescription.Format = desc.Format;
		shaderResourceViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDescription.Texture2D.MipLevels = 1;

		device->CreateShaderResourceView(frameBuffer_->ColorResource(), &shaderResourceViewDescription, imguiHeap->CPUHandle(imguiShaderResourceViewIndex_));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE MaterialRenderer::ImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(imguiShaderResourceViewIndex_);
	}
}
