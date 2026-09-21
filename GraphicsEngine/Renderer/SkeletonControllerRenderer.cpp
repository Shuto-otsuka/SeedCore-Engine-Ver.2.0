#include <GraphicsEngine/Renderer/SkeletonControllerRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>

namespace SeedCore
{
	namespace
	{
		void BuildIcosphereEdges(Uint subdivisionLevel, DynamicArray<Vector3>& outFullEdges)
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

					outFullEdges.push_back(vertices[indexA]);
					outFullEdges.push_back(vertices[indexB]);
				}
			}
		}
	}

	SkeletonControllerRenderer::SkeletonControllerRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : modelShader_(rootSignature, pipelineStateObject), boneLineShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	SkeletonControllerRenderer::~SkeletonControllerRenderer() = default;

	void SkeletonControllerRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height)
	{
		bindlessHeap_ = bindlessHeap;
		maxInstanceCount_ = 2048;
		maxBoneCount_ = 2048;

		modelShader_.Create(shaderCache, device);

		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelStructuredBuffer>>(device, bindlessHeap, maxInstanceCount_);
		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);

		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_ = MakePtr<FrameBuffer>(device, &renderTargetViewHeap_, bindlessHeap, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, &depthStencilViewHeap_, 0.45f, 0.65f, 0.9f, 1.0f);

		sceneSystem_ = MakePtr<SceneSystem>(device, bindlessHeap);

		constantIndicesBuffer_ = MakePtr<ConstantBuffer<ConstantIndices>>(device, bindlessHeap);
		shaderResourceIndicesBuffer_ = MakePtr<ConstantBuffer<ShaderResourceIndices>>(device, bindlessHeap);

		if (D3D12Check::GetLevel() != D3D12Level::D12_2)
		{
			modelCullingBuffer_.Create(device, bindlessHeap);
		}

		boneLineShader_.Create(shaderCache, device, DepthStencilStateType::DepthOff);

		boneInstanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ColliderStructuredBuffer>>(device, bindlessHeap, maxBoneInstanceCount_);
		boneInstanceConstantsBuffer_ = MakePtr<ConstantBuffer<ColliderConstantBuffer>>(device, bindlessHeap);

		BuildIcosphereEdges(icosphereSubdivisionLevel_, sphereEdgeData_);
		sphereEdgeCount_ = static_cast<Uint>(sphereEdgeData_.size() / 2);

		sphereEdgeBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Vector3>>(device, bindlessHeap, static_cast<Uint>(sphereEdgeData_.size()));

		Uint coneLineCount = boneConeRingSegments_ + boneConeVerticalLineCount_;
		Uint maxLinesPerBoneInstance = std::max(coneLineCount, sphereEdgeCount_);
		groupsPerBoneInstance_ = (maxLinesPerBoneInstance + threadsPerGroup_ - 1) / threadsPerGroup_;
	}

	void SkeletonControllerRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, Uint32 width, Uint32 height)
	{
		renderTargetViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		depthStencilViewHeap_.Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		frameBuffer_->Resize(device, bindlessHeap, width, height);
	}

	void SkeletonControllerRenderer::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix, Int selectedNodeIndex)
	{
		opaqueInstances_.clear();
		transparentInstances_.clear();
		boneMatrices_.clear();
		boneInstances_.clear();
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

		const DynamicArray<SubMesh>& subMeshes = crister->SubMeshes();
		const DynamicArray<Surface>& surfaces = crister->Surfaces();
		const DynamicArray<Cluster>& clusters = crister->Clusters();
		const DynamicArray<Skin>& skins = crister->Skins();
		const DynamicArray<Node>& nodes = crister->Nodes();

		DynamicArray<Matrix> poseGlobalTransforms;
		Bool hasPose = false;

		if (!nodes.empty() && animationAssetId != 0)
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

			DynamicArray<Uint32> residentClusters;
			if (skinned)
			{
				residentClusters.push_back(subMesh.clusterOffset_);
			}
			else
			{
				Uint32 desired = subMesh.clusterCount_ - 1;
				if (!crister->ClusterResident(subMesh.clusterOffset_ + desired))
				{
					streamingRequests_.push_back({ crister, subMesh.clusterOffset_ + desired });
				}

				for (Uint32 c = 0; c < subMesh.clusterCount_; ++c)
				{
					if (crister->ClusterResident(subMesh.clusterOffset_ + c))
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

		constexpr Float boneRadiusFactor = 0.12f;
		constexpr Float boneRadiusFallback = 0.02f;
		Color boneGizmoColor(1.0f, 0.8f, 0.2f, 1.0f);
		Color boneGizmoSelectedColor(0.2f, 1.0f, 1.0f, 1.0f);

		std::unordered_set<Int> jointNodeSet;
		for (const Skin& skin : skins)
		{
			for (Int jointIndex : skin.joints_)
			{
				jointNodeSet.insert(jointIndex);
			}
		}

		DynamicArray<Vector3> worldNodePositions(nodes.size(), Vector3::Zero);
		for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
		{
			const Matrix& nodeGlobal = hasPose ? poseGlobalTransforms[nodeIndex] : nodes[nodeIndex].globalTransform_;
			worldNodePositions[nodeIndex] = Vector3::Transform(nodeGlobal.Translation(), worldMatrix);
		}

		std::unordered_map<Int, Int> jointParentJoint;
		for (Int jointIndex : jointNodeSet)
		{
			Int current = nodes[static_cast<Size>(jointIndex)].parentIndex_;
			while (current >= 0 && !jointNodeSet.contains(current))
			{
				current = nodes[static_cast<Size>(current)].parentIndex_;
			}
			jointParentJoint[jointIndex] = current;
		}

		DynamicArray<Float> nodeRadius(nodes.size(), boneRadiusFallback);
		for (Int jointIndex : jointNodeSet)
		{
			Int parentJoint = jointParentJoint[jointIndex];
			if (parentJoint < 0)
			{
				continue;
			}
			Float edgeLength = Vector3::Distance(worldNodePositions[static_cast<Size>(parentJoint)], worldNodePositions[static_cast<Size>(jointIndex)]);
			nodeRadius[static_cast<Size>(jointIndex)] = Max(edgeLength * boneRadiusFactor, 0.001f);
		}
		for (Int jointIndex : jointNodeSet)
		{
			if (jointParentJoint[jointIndex] >= 0)
			{
				continue;
			}

			Float childRadiusSum = 0.0f;
			Int childJointCount = 0;
			for (Int otherJointIndex : jointNodeSet)
			{
				if (jointParentJoint[otherJointIndex] == jointIndex)
				{
					childRadiusSum += nodeRadius[static_cast<Size>(otherJointIndex)];
					childJointCount++;
				}
			}
			if (childJointCount > 0)
			{
				nodeRadius[static_cast<Size>(jointIndex)] = childRadiusSum / static_cast<Float>(childJointCount);
			}
		}

		for (Int jointIndex : jointNodeSet)
		{
			if (boneInstances_.size() >= maxBoneInstanceCount_)
			{
				break;
			}

			ColliderStructuredBuffer sphereInstance{};
			sphereInstance.position_ = worldNodePositions[static_cast<Size>(jointIndex)];
			sphereInstance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Sphere);
			sphereInstance.rotation_ = Quaternion::Identity;
			sphereInstance.dimensions_ = Vector3(nodeRadius[static_cast<Size>(jointIndex)], 0.0f, 0.0f);
			sphereInstance.color_ = jointIndex == selectedNodeIndex ? boneGizmoSelectedColor : boneGizmoColor;
			boneInstances_.push_back(sphereInstance);

			Int parentJoint = jointParentJoint[jointIndex];
			if (parentJoint < 0 || boneInstances_.size() >= maxBoneInstanceCount_)
			{
				continue;
			}

			Vector3 parentPosition = worldNodePositions[static_cast<Size>(parentJoint)];
			Vector3 childPosition = worldNodePositions[static_cast<Size>(jointIndex)];
			Vector3 direction = childPosition - parentPosition;
			Float boneLength = direction.Length();
			if (boneLength < 1e-6f)
			{
				continue;
			}
			direction /= boneLength;

			Float dot = Clamp(Vector3::Up.Dot(direction), -1.0f, 1.0f);
			Vector3 axis = Vector3::Up.Cross(direction);
			Quaternion coneRotation;
			if (axis.LengthSquared() < 1e-8f)
			{
				coneRotation = dot > 0.0f ? Quaternion::Identity : Quaternion::CreateFromAxisAngle(Vector3::Right, DirectX::XM_PI);
			}
			else
			{
				axis.Normalize();
				coneRotation = Quaternion::CreateFromAxisAngle(axis, Acos(dot));
			}

			ColliderStructuredBuffer coneInstance{};
			coneInstance.position_ = parentPosition + direction * (boneLength * 0.5f);
			coneInstance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Cone);
			coneInstance.rotation_ = coneRotation;
			coneInstance.dimensions_ = Vector3(nodeRadius[static_cast<Size>(jointIndex)], boneLength * 0.5f, 0.0f);
			coneInstance.color_ = jointIndex == selectedNodeIndex ? boneGizmoSelectedColor : boneGizmoColor;
			boneInstances_.push_back(coneInstance);
		}
	}

	void SkeletonControllerRenderer::Upload()
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

		if (D3D12Check::GetLevel() != D3D12Level::D12_2)
		{
			modelCullingBuffer_.Reserve(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()) * 32);
		}
	}

	void SkeletonControllerRenderer::Begin(D3D12CommandList* cmdList)
	{
		frameBuffer_->Begin(cmdList);
		frameBuffer_->Clear(cmdList, 0.45f, 0.65f, 0.9f, 1.0f);
	}

	void SkeletonControllerRenderer::Draw(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, const SceneConstantBuffer& scene)
	{
		sceneSystem_->Upload(scene);

		constantIndices_.sceneIndex_ = sceneSystem_->GetIndex();
		constantIndices_.colliderIndex_ = boneInstanceConstantsBuffer_->GetIndex();
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

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
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
		else
		{
			modelCullingBuffer_.Begin(cmd);

			cmd->SetComputeRootSignature(modelShader_.GetRootSignature());
			cmd->SetComputeRootConstantBufferView(2, constantAddr);
			cmd->SetComputeRootConstantBufferView(0, shaderResourceAddr);
			Uint cullingIndex = modelCullingBuffer_.GetConstantBufferIndex();
			cmd->SetComputeRoot32BitConstants(3, 1, &cullingIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStateModelCulling());
			cmd->Dispatch(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);

			modelCullingBuffer_.Barrier(cmd);

			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			Uint singleSidedIndex = modelCullingBuffer_.GetSingleSidedShaderResourceViewIndex();
			Uint doubleSidedIndex = modelCullingBuffer_.GetDoubleSidedShaderResourceViewIndex();

			cmd->SetGraphicsRoot32BitConstants(3, 1, &singleSidedIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewStatic());
			cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), 0, nullptr, 0);
			ProfilerStats::AddDrawCall();

			cmd->SetGraphicsRoot32BitConstants(3, 1, &doubleSidedIndex, 0);
			cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewStaticDoubleSided());
			cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), sizeof(D3D12_DRAW_ARGUMENTS), nullptr, 0);
			ProfilerStats::AddDrawCall();

			if (hasSkinnedOpaque_)
			{
				cmd->SetGraphicsRoot32BitConstants(3, 1, &singleSidedIndex, 0);
				cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewSkeletal());
				cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), 0, nullptr, 0);
				ProfilerStats::AddDrawCall();

				cmd->SetGraphicsRoot32BitConstants(3, 1, &doubleSidedIndex, 0);
				cmd->SetPipelineState(modelShader_.GetPipelineStatePreviewSkeletalDoubleSided());
				cmd->ExecuteIndirect(modelCullingBuffer_.GetCommandSignature(), 1, modelCullingBuffer_.GetArgumentBuffer(), sizeof(D3D12_DRAW_ARGUMENTS), nullptr, 0);
				ProfilerStats::AddDrawCall();
			}

			modelCullingBuffer_.End(cmd);
		}
	}

	void SkeletonControllerRenderer::DrawBones(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap)
	{
		if (boneInstances_.empty())
		{
			return;
		}

		Uint instanceCount = static_cast<Uint>(boneInstances_.size());
		if (instanceCount > maxBoneInstanceCount_)
		{
			instanceCount = maxBoneInstanceCount_;
		}

		boneInstanceBuffer_->Update(boneInstances_.data(), instanceCount);
		sphereEdgeBuffer_->Update(sphereEdgeData_.data(), static_cast<Uint>(sphereEdgeData_.size()));

		ColliderConstantBuffer constants{};
		constants.instanceBufferIndex_ = boneInstanceBuffer_->Index();
		constants.instanceCount_ = instanceCount;
		constants.groupsPerInstance_ = groupsPerBoneInstance_;
		constants.sphereEdgeBufferIndex_ = sphereEdgeBuffer_->Index();
		constants.sphereEdgeCount_ = sphereEdgeCount_;
		constants.hemisphereEdgeBufferIndex_ = 0;
		constants.hemisphereEdgeCount_ = 0;
		boneInstanceConstantsBuffer_->Update(constants);

		auto* cmd = cmdList->Get();

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = frameBuffer_->RenderTargetViewHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = frameBuffer_->DepthStencilViewHandle();
		D3D12_VIEWPORT viewport = frameBuffer_->GetViewport();

		cmd->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);
		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(boneLineShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndicesBuffer_->Address());

		cmd->SetPipelineState(boneLineShader_.GetPipelineState());

		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			cmd->DispatchMesh(instanceCount * groupsPerBoneInstance_, 1, 1);
		}
		else
		{
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmd->DrawInstanced(groupsPerBoneInstance_ * threadsPerGroup_ * 2, instanceCount, 0, 0);
		}
		ProfilerStats::AddDrawCall();
	}

	void SkeletonControllerRenderer::End(D3D12CommandList* cmdList)
	{
		frameBuffer_->End(cmdList);
	}

	void SkeletonControllerRenderer::RegisterImGuiShaderResourceView(ID3D12Device* device, DescriptorHeap* imguiHeap)
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

	D3D12_GPU_DESCRIPTOR_HANDLE SkeletonControllerRenderer::ImGuiGPUHandle()const
	{
		return imguiHeap_->GPUHandle(imguiShaderResourceViewIndex_);
	}
}
