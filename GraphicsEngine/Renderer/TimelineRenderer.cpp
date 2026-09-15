#include <GraphicsEngine/Renderer/TimelineRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

namespace SeedCore
{
	TimelineRenderer::TimelineRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : modelShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	TimelineRenderer::~TimelineRenderer() = default;

	void TimelineRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		maxInstanceCount_ = 2048;
		maxBoneCount_ = 2048;

		modelShader_.Create(shaderCache, device);

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelStructuredBuffer>>(device, bindlessHeap, maxInstanceCount_);
		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		/// [EN] optimizedClearColor* must match every Clear() call's color —
		///      D3D12 bakes this into the resource at creation for a fast
		///      clear path; a mismatch (e.g. this sky-blue vs. the default
		///      gray) still clears correctly but falls back to a slower path
		///      and emits a debug-layer warning.
		/// [JP] optimizedClearColor*は毎回のClear()呼び出しの色と一致させる
		///      必要がある — D3D12はリソース作成時にこれを焼き込んで高速な
		///      クリアパスを使う。不一致(例: このスカイブルーとデフォルトの
		///      グレー)でも正しくクリアはされるが、遅いパスにフォールバック
		///      しデバッグレイヤーの警告が出る。
		frameBuffer_ = MakePtr<FrameBuffer>(device, &renderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &depthStencilViewHeap_, 0.45f, 0.65f, 0.9f, 1.0f);

		sceneSystem_ = MakePtr<SceneSystem>(device, bindlessHeap);

		constantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap);
		shaderResourceIndicesBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, bindlessHeap);
	}

	void TimelineRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_->Resize(device, bindlessHeap, width, height);
	}

	void TimelineRenderer::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix)
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

		const auto& subMeshes = crister->SubMeshes();
		const auto& surfaces = crister->Surfaces();
		const auto& clusters = crister->Clusters();
		const auto& skins = crister->Skins();
		const auto& nodes = crister->Nodes();

		DynamicArray<Matrix> poseGlobalTransforms;
		Bool hasPose = false;

		if (!skins.empty() && animationAssetId != 0)
		{
			Handle<Animation> animationHandle = animationResource.GetHandle(animationAssetId);
			Animation* animation = animationHandle.empty() ? nullptr : animationResource.Resolve(loaderSystem, animationHandle);

			if (animation)
			{
				Float duration = animation->Duration();
				Float sampleTime = duration > 0.0f ? std::fmod(time, duration) : time;

				std::unordered_map<Int, Vector3> translationOverrides;
				std::unordered_map<Int, Quaternion> rotationOverrides;
				std::unordered_map<Int, Vector3> scaleOverrides;
				animation->SamplePose(sampleTime, translationOverrides, rotationOverrides, scaleOverrides);

				poseGlobalTransforms.resize(nodes.size(), Matrix::Identity);

				std::function<void(Int, const Matrix&)> traverse = [&](Int nodeIndex, const Matrix& parentGlobal)
					{
						const Node& node = nodes[nodeIndex];

						auto translationIt = translationOverrides.find(nodeIndex);
						Vector3 translation = (translationIt != translationOverrides.end()) ? translationIt->second : node.translation_;

						auto rotationIt = rotationOverrides.find(nodeIndex);
						Quaternion rotation = (rotationIt != rotationOverrides.end()) ? rotationIt->second : node.rotation_;

						auto scaleIt = scaleOverrides.find(nodeIndex);
						Vector3 scale = (scaleIt != scaleOverrides.end()) ? scaleIt->second : node.scale_;

						Matrix scaleMatrix = Matrix::CreateScale(scale.x, scale.y, scale.z);
						Matrix rotationMatrix = Matrix::CreateFromQuaternion(rotation);
						Matrix translationMatrix = Matrix::CreateTranslation(translation.x, translation.y, translation.z);
						Matrix localTransform = scaleMatrix * rotationMatrix * translationMatrix;

						Matrix global = localTransform * parentGlobal;
						poseGlobalTransforms[nodeIndex] = global;

						for (Int childIndex : node.children_)
						{
							traverse(childIndex, global);
						}
					};

				for (Int rootNodeIndex : crister->Stages()[crister->DefaultStage()].nodes_)
				{
					traverse(rootNodeIndex, Matrix::Identity);
				}

				hasPose = true;
			}
		}

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
				boneBase = 0;
				for (const Skin& skin : skins)
				{
					for (Size joint = 0; joint < skin.joints_.size(); joint++)
					{
						const Matrix& globalTransform = hasPose ? poseGlobalTransforms[skin.joints_[joint]] : nodes[skin.joints_[joint]].globalTransform_;

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

		for (Size subMeshIndex = 0; subMeshIndex < subMeshes.size(); subMeshIndex++)
		{
			const SubMesh& subMesh = subMeshes[subMeshIndex];

			const Surface& material = surfaces[subMesh.surfaceIndex_];

			Bool skinned = subMesh.skinIndex_ >= 0 && subMesh.skinIndex_ < static_cast<Int>(skins.size()) && !boneOverflow;

			if (subMesh.clusterCount_ == 0)
			{
				continue;
			}

			/// [EN] No CPU-side camera exists for the preview, so LOD selection
			///      simply asks for the finest cluster and renders whatever of
			///      the chain happens to already be resident.
			/// [JP] プレビューにはCPU側カメラが存在しないため、LOD選択は単純に
			///      最も精細なクラスタを要求し、チェーンのうち既に常駐している
			///      ものだけを描画する。
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

				for (const Matrix& placement : crister->SubMeshPlacement(subMeshIndex))
				{
					Matrix placedWorldMatrix = placement * worldMatrix;
					Matrix placedInverseTransposeWorld = placedWorldMatrix.Invert().Transpose();

					Uint32 remaining = cluster.meshletCount_;
					Uint32 offset = cluster.meshletOffset_;

					while (remaining > 0)
					{
						Uint32 count = (remaining > maxMeshletsPerDispatch) ? maxMeshletsPerDispatch : remaining;

						ModelStructuredBuffer instanceData{};
						instanceData.transform_.world_ = placedWorldMatrix;
						instanceData.transform_.inverseTransposeWorld_ = placedInverseTransposeWorld;

						instanceData.texture_.baseColor_ = material.baseColor_;
						instanceData.texture_.metallic_ = material.metallic_;
						instanceData.texture_.roughness_ = material.roughness_;
						instanceData.texture_.alphaCutoff_ = material.alphaMode_ == 1 ? material.alphaCutoff_ : (material.alphaMode_ == 2 ? 0.01f : 0.0f);
						instanceData.texture_.emissive_ = Vector3(material.emissiveFactor_[0], material.emissiveFactor_[1], material.emissiveFactor_[2]);

						instanceData.texture_.ior_ = material.khr_.ior_.ior_;
						instanceData.texture_.emissiveStrength_ = material.khr_.emissiveStrength_.emissiveStrength_;
						instanceData.extension_.specularFactor_ = material.khr_.specular_.specularFactor_;
						instanceData.extension_.specularColor_ = Vector3(material.khr_.specular_.specularColorFactor_[0], material.khr_.specular_.specularColorFactor_[1], material.khr_.specular_.specularColorFactor_[2]);
						instanceData.extension_.clearCoatFactor_ = material.khr_.clearCoat_.clearCoatFactor_;
						instanceData.extension_.clearCoatRoughness_ = material.khr_.clearCoat_.clearCoatRoughnessFactor_;
						instanceData.extension_.anisotropy_ = material.khr_.anisotropy_.anisotropyStrength_;
						instanceData.extension_.transmissionFactor_ = material.khr_.transmission_.transmissionFactor_;
						instanceData.extension_.volumeThicknessFactor_ = material.khr_.volume_.thicknessFactor_;
						instanceData.extension_.volumeAttenuationDistance_ = material.khr_.volume_.attenuationDistance_;
						instanceData.extension_.volumeAttenuationColor_ = Vector3(material.khr_.volume_.attenuationColor_[0], material.khr_.volume_.attenuationColor_[1], material.khr_.volume_.attenuationColor_[2]);
						instanceData.extension_.sheenColor_ = Vector3(material.khr_.sheen_.sheenColorFactor_[0], material.khr_.sheen_.sheenColorFactor_[1], material.khr_.sheen_.sheenColorFactor_[2]);
						instanceData.extension_.sheenRoughness_ = material.khr_.sheen_.sheenRoughnessFactor_;
						instanceData.extension_.iridescenceFactor_ = material.khr_.iridescence_.iridescenceFactor_;
						instanceData.extension_.iridescenceIor_ = material.khr_.iridescence_.iridescenceIor_;
						instanceData.extension_.iridescenceThickness_ = (material.khr_.iridescence_.iridescenceThicknessMinimum_ + material.khr_.iridescence_.iridescenceThicknessMaximum_) * 0.5f;
						instanceData.extension_.unlit_ = material.khr_.unlit_.unlit_ != 0 ? 1.0f : 0.0f;
						instanceData.shading_.shadingModel_ = material.khr_.unlit_.unlit_ != 0 ? static_cast<Uint>(ShadingModel::Unlit) : static_cast<Uint>(material.shadingModel_);

						instanceData.texture_.baseColorTextureIndex_ = crister->TextureBindlessIndex(material.baseColorTextureIndex_);
						instanceData.texture_.normalTextureIndex_ = crister->TextureBindlessIndex(material.normalTextureIndex_);
						instanceData.texture_.metallicRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.metallicRoughnessTextureIndex_);
						instanceData.texture_.emissiveTextureIndex_ = crister->TextureBindlessIndex(material.emissiveTextureIndex_);
						instanceData.texture_.occlusionTextureIndex_ = crister->TextureBindlessIndex(material.occlusionTextureIndex_);
						instanceData.extension_.specularTextureIndex_ = crister->TextureBindlessIndex(material.khr_.specular_.specularTextureIndex_);
						instanceData.extension_.specularColorTextureIndex_ = crister->TextureBindlessIndex(material.khr_.specular_.specularColorTextureIndex_);
						instanceData.extension_.clearCoatTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatTextureIndex_);
						instanceData.extension_.clearCoatRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatRoughnessTextureIndex_);
						instanceData.extension_.clearCoatNormalTextureIndex_ = crister->TextureBindlessIndex(material.khr_.clearCoat_.clearCoatNormalTextureIndex_);
						instanceData.extension_.transmissionTextureIndex_ = crister->TextureBindlessIndex(material.khr_.transmission_.transmissionTextureIndex_);
						instanceData.extension_.thicknessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.volume_.thicknessTextureIndex_);
						instanceData.extension_.sheenColorTextureIndex_ = crister->TextureBindlessIndex(material.khr_.sheen_.sheenColorTextureIndex_);
						instanceData.extension_.sheenRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.sheen_.sheenRoughnessTextureIndex_);
						instanceData.extension_.iridescenceTextureIndex_ = crister->TextureBindlessIndex(material.khr_.iridescence_.iridescenceTextureIndex_);
						instanceData.extension_.iridescenceThicknessTextureIndex_ = crister->TextureBindlessIndex(material.khr_.iridescence_.iridescenceThicknessTextureIndex_);
						instanceData.extension_.anisotropyTextureIndex_ = crister->TextureBindlessIndex(material.khr_.anisotropy_.anisotropyTextureIndex_);
						instanceData.extension_.anisotropyRotation_ = material.khr_.anisotropy_.anisotropyRotation_;

						instanceData.geometry_.vertexBufferIndex_ = crister->ClusterVertexBufferIndex(clusterIndex);
						instanceData.skining_.skinVertexBufferIndex_ = crister->SkinVertexBufferIndex();
						instanceData.streaming_.positionMin_ = crister->PositionMin();
						instanceData.streaming_.positionExtent_ = crister->PositionExtent();
						instanceData.streaming_.texcoordMinU_ = crister->TexcoordMin().x;
						instanceData.streaming_.texcoordMinV_ = crister->TexcoordMin().y;
						instanceData.streaming_.texcoordExtent_ = crister->TexcoordExtent();
						instanceData.geometry_.meshletBufferIndex_ = crister->ClusterMeshletBufferIndex(clusterIndex);
						instanceData.geometry_.meshletBoundBufferIndex_ = crister->ClusterMeshletBoundBufferIndex(clusterIndex);
						instanceData.geometry_.vertexIndicesBufferIndex_ = crister->ClusterVertexIndicesBufferIndex(clusterIndex);
						instanceData.geometry_.primitiveIndicesBufferIndex_ = crister->ClusterPrimitiveIndicesBufferIndex(clusterIndex);

						instanceData.geometry_.meshletOffset_ = offset - cluster.meshletOffset_;
						instanceData.geometry_.meshletCount_ = count;

						instanceData.streaming_.lodError_ = cluster.lodError_;
						instanceData.streaming_.lodErrorNext_ = lodErrorNext;

						if (skinned)
						{
							Uint skinBoneOffset = boneBase;
							for (Int skinIndex = 0; skinIndex < subMesh.skinIndex_; skinIndex++)
							{
								skinBoneOffset += static_cast<Uint>(skins[skinIndex].joints_.size());
							}
							instanceData.skining_.skinIndex_ = static_cast<Uint>(subMesh.skinIndex_);
							instanceData.skining_.boneOffset_ = skinBoneOffset;
						}
						else
						{
							instanceData.skining_.skinIndex_ = 0xFFFFFFFF;
							instanceData.skining_.boneOffset_ = 0;
						}

						instanceData.shading_.doubleSided_ = material.doubleSided_ ? 1 : 0;
						instanceData.shading_.blend_ = material.alphaMode_ == 2 ? 1 : 0;
						instanceData.shading_.selected_ = 0;

						if (material.alphaMode_ != 2)
						{
							opaqueInstances_.push_back(instanceData);
							hasSkinnedOpaque_ = hasSkinnedOpaque_ || instanceData.skining_.skinIndex_ != 0xFFFFFFFF;
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
	}

	void TimelineRenderer::Upload()
	{
		if (uploaded_)
		{
			return;
		}
		uploaded_ = true;

		shaderResourceIndices_.model_.instanceIndex_ = instanceBuffer_->Index();
		shaderResourceIndices_.model_.boneMatrixIndex_ = boneBuffer_->Index();
		shaderResourceIndices_.model_.previousBoneMatrixIndex_ = boneBuffer_->Index();

		if (!opaqueInstances_.empty() || !transparentInstances_.empty())
		{
			DynamicArray<ModelStructuredBuffer> allInstances;
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

	void TimelineRenderer::Begin(D3D12CommandList* cmdList)
	{
		/// [EN] Sky-blue clear instead of the default flat gray, since the
		///      preview has no skybox/background of its own.
		/// [JP] プレビュー自体には空/背景が無いため、デフォルトのフラット
		///      グレーではなく空色でクリアする。
		frameBuffer_->Begin(cmdList);
		frameBuffer_->Clear(cmdList, 0.45f, 0.65f, 0.9f, 1.0f);
	}

	void TimelineRenderer::Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene)
	{
		sceneSystem_->Upload(scene);

		constantIndices_.sceneIndex_ = sceneSystem_->GetIndex();
		constantIndicesBuffer_->Update(constantIndices_);
		shaderResourceIndicesBuffer_->Update(shaderResourceIndices_);

		if (opaqueInstances_.empty())
		{
			return;
		}

		D3D12_GPU_VIRTUAL_ADDRESS constantAddr = constantIndicesBuffer_->Address();
		D3D12_GPU_VIRTUAL_ADDRESS shaderResourceAddr = shaderResourceIndicesBuffer_->Address();

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantAddr);
		cmd->SetGraphicsRootConstantBufferView(0, shaderResourceAddr);

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

	void TimelineRenderer::End(D3D12CommandList* cmdList)
	{
		frameBuffer_->End(cmdList);
	}

	void TimelineRenderer::RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap)
	{
		/// [EN] Called again on every resize - DescriptorHeap is a bump
		///      allocator with no FreeIndex, so only allocate once and
		///      reuse the same slot on every later call (see
		///      Renderer::RegisterImGuiShaderResourceViews for the same
		///      pattern/rationale).
		/// [JP] リサイズのたびに再度呼ばれる - DescriptorHeap は FreeIndex を
		///      持たない増加専用アロケータなので、最初の1回だけ確保し、以降は
		///      同じスロットを使い回す(同じパターン/理由は
		///      Renderer::RegisterImGuiShaderResourceViews 参照)。
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

	D3D12_GPU_DESCRIPTOR_HANDLE TimelineRenderer::ImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(imguiShaderResourceViewIndex_);
	}
}
