#include <GraphicsEngine/Model/ModelExporter.h>
#include <GraphicsEngine/Model/Crister.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	ExportProfile ModelExporter::Preset(ExportPreset preset)
	{
		ExportProfile profile{};
		switch (preset)
		{
		case ExportPreset::Gltf:
		{
			profile = ExportProfile{ ModelFormat::Gltf, false, ExportAxis::GltfRightHandedYUp, 1.0f };
			break;
		}
		case ExportPreset::Glb:
		{
			profile = ExportProfile{ ModelFormat::Gltf, true, ExportAxis::GltfRightHandedYUp, 1.0f };
			break;
		}
		case ExportPreset::FbxMaya:
		{
			profile = ExportProfile{ ModelFormat::Fbx, true, ExportAxis::MayaYUp, 100.0f };
			break;
		}
		case ExportPreset::FbxUnreal:
		{
			profile = ExportProfile{ ModelFormat::Fbx, true, ExportAxis::UnrealZUpLeftHanded, 100.0f };
			break;
		}
		case ExportPreset::FbxUnity:
		{
			profile = ExportProfile{ ModelFormat::Fbx, true, ExportAxis::MayaYUp, 1.0f };
			break;
		}
		case ExportPreset::FbxNative:
		{
			profile = ExportProfile{ ModelFormat::Fbx, true, ExportAxis::EngineNativeDirectX, 1.0f };
			break;
		}
		}
		return profile;
	}

	Bool ModelExporter::Export(const Crister& crister, const ExportProfile& profile, String filePath)
	{
		std::filesystem::path outputPath(filePath.c_str());
		std::error_code directoryError;
		std::filesystem::create_directories(outputPath.parent_path(), directoryError);

		switch (profile.format_)
		{
		case ModelFormat::Gltf:
		{
			if (crister.vertices_.empty())
			{
				return false;
			}

			Bool binary = profile.binary_;

			tinygltf::Model model;
			model.asset.version = "2.0";
			model.asset.generator = "SeedCore";

			tinygltf::Buffer& buffer = model.buffers.emplace_back();
			if (!binary)
			{
				buffer.uri = outputPath.stem().string() + ".bin";
			}

			auto appendAccessor = [&](const void* data, Size byteSize, Int componentType, Int accessorType, Size elementCount, Int bufferViewTarget) -> Int
			{
				Size byteOffset = buffer.data.size();
				const unsigned char* begin = reinterpret_cast<const unsigned char*>(data);
				buffer.data.insert(buffer.data.end(), begin, begin + byteSize);
				while (buffer.data.size() % 4 != 0)
				{
					buffer.data.push_back(0);
				}

				tinygltf::BufferView& bufferView = model.bufferViews.emplace_back();
				bufferView.buffer = 0;
				bufferView.byteOffset = byteOffset;
				bufferView.byteLength = byteSize;
				if (bufferViewTarget != 0)
				{
					bufferView.target = bufferViewTarget;
				}

				tinygltf::Accessor& accessor = model.accessors.emplace_back();
				accessor.bufferView = static_cast<Int>(model.bufferViews.size()) - 1;
				accessor.byteOffset = 0;
				accessor.componentType = componentType;
				accessor.type = accessorType;
				accessor.count = elementCount;
				return static_cast<Int>(model.accessors.size()) - 1;
			};

			const DynamicArray<SubMesh>& subMeshes = crister.SubMeshes();
			DynamicArray<Int> meshGroupKeys;
			for (const SubMesh& subMesh : subMeshes)
			{
				Bool known = false;
				for (Int key : meshGroupKeys)
				{
					if (key == subMesh.meshIndex_)
					{
						known = true;
						break;
					}
				}
				if (!known)
				{
					meshGroupKeys.push_back(subMesh.meshIndex_);
				}
			}

			for (Int groupKey : meshGroupKeys)
			{
				tinygltf::Mesh& mesh = model.meshes.emplace_back();

				for (const SubMesh& subMesh : subMeshes)
				{
					if (subMesh.meshIndex_ != groupKey)
					{
						continue;
					}

					tinygltf::Primitive& primitive = mesh.primitives.emplace_back();
					primitive.mode = TINYGLTF_MODE_TRIANGLES;
					primitive.material = static_cast<Int>(subMesh.surfaceIndex_);

					Uint32 vertexBase = subMesh.vertexOffset_;
					Uint32 vertexCount = subMesh.vertexCount_;

					DynamicArray<Vector3> positions(vertexCount);
					DynamicArray<Vector3> normals(vertexCount);
					DynamicArray<Vector4> tangents(vertexCount);
					DynamicArray<Vector2> texcoords(vertexCount);
					for (Uint32 vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
					{
						const Vertex& vertex = crister.vertices_[vertexBase + vertexIndex];
						positions[vertexIndex] = Vector3(-vertex.position_.x, vertex.position_.y, vertex.position_.z);
						normals[vertexIndex] = Vector3(-vertex.normal_.x, vertex.normal_.y, vertex.normal_.z);
						tangents[vertexIndex] = Vector4(-vertex.tangent_.x, vertex.tangent_.y, vertex.tangent_.z, vertex.tangent_.w);
						texcoords[vertexIndex] = vertex.texcoord_;
					}

					Vector3 positionMin = positions[0];
					Vector3 positionMax = positions[0];
					for (const Vector3& position : positions)
					{
						positionMin = Vector3::Min(positionMin, position);
						positionMax = Vector3::Max(positionMax, position);
					}

					Int positionAccessor = appendAccessor(positions.data(), positions.size() * sizeof(Vector3), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
					model.accessors[positionAccessor].minValues = { positionMin.x, positionMin.y, positionMin.z };
					model.accessors[positionAccessor].maxValues = { positionMax.x, positionMax.y, positionMax.z };
					primitive.attributes["POSITION"] = positionAccessor;
					primitive.attributes["NORMAL"] = appendAccessor(normals.data(), normals.size() * sizeof(Vector3), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
					primitive.attributes["TANGENT"] = appendAccessor(tangents.data(), tangents.size() * sizeof(Vector4), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
					primitive.attributes["TEXCOORD_0"] = appendAccessor(texcoords.data(), texcoords.size() * sizeof(Vector2), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);

					if (subMesh.skinIndex_ >= 0)
					{
						DynamicArray<Uint16> joints(vertexCount * 4);
						DynamicArray<Vector4> weights(vertexCount);
						for (Uint32 vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
						{
							const Vertex& vertex = crister.vertices_[vertexBase + vertexIndex];
							joints[vertexIndex * 4 + 0] = static_cast<Uint16>(vertex.joints_.x);
							joints[vertexIndex * 4 + 1] = static_cast<Uint16>(vertex.joints_.y);
							joints[vertexIndex * 4 + 2] = static_cast<Uint16>(vertex.joints_.z);
							joints[vertexIndex * 4 + 3] = static_cast<Uint16>(vertex.joints_.w);
							weights[vertexIndex] = vertex.weights_;
						}
						primitive.attributes["JOINTS_0"] = appendAccessor(joints.data(), joints.size() * sizeof(Uint16), TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, TINYGLTF_TYPE_VEC4, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
						primitive.attributes["WEIGHTS_0"] = appendAccessor(weights.data(), weights.size() * sizeof(Vector4), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4, vertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
					}

					DynamicArray<Uint32> indices(subMesh.indexCount_);
					for (Uint32 index = 0; index < subMesh.indexCount_; index++)
					{
						indices[index] = crister.vertexIndices_[subMesh.indexOffset_ + index] - vertexBase;
					}
					primitive.indices = appendAccessor(indices.data(), indices.size() * sizeof(Uint32), TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR, subMesh.indexCount_, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

					for (const Morph& morph : subMesh.morphs_)
					{
						DynamicArray<Vector3> mirroredDeltas(morph.positionDeltas_.size());
						for (Size deltaIndex = 0; deltaIndex < morph.positionDeltas_.size(); deltaIndex++)
						{
							const Vector3& delta = morph.positionDeltas_[deltaIndex];
							mirroredDeltas[deltaIndex] = Vector3(-delta.x, delta.y, delta.z);
						}
						std::map<std::string, int> target;
						target["POSITION"] = appendAccessor(mirroredDeltas.data(), mirroredDeltas.size() * sizeof(Vector3), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, vertexCount, 0);
						primitive.targets.push_back(target);
						mesh.weights.push_back(0.0);
					}
				}
			}

			for (Size bitmapIndex = 0; bitmapIndex < crister.bitmaps_.size(); bitmapIndex++)
			{
				const Bitmap& bitmap = crister.bitmaps_[bitmapIndex];
				std::string pngName = outputPath.stem().string() + "_" + std::to_string(bitmapIndex) + ".png";
				std::filesystem::path pngPath = outputPath.parent_path() / pngName;

				tinygltf::Image& gltfImage = model.images.emplace_back();

				if (bitmap.width_ > 0 && bitmap.height_ > 0 && !bitmap.cacheData_.empty())
				{
					DynamicArray<Uchar> rgba;
					const Uchar* source = reinterpret_cast<const Uchar*>(bitmap.cacheData_.data());
					Size pixelCount = static_cast<Size>(bitmap.width_) * bitmap.height_;
					if (bitmap.component_ == 4)
					{
						rgba.assign(source, source + pixelCount * 4);
					}
					else
					{
						rgba.resize(pixelCount * 4, static_cast<Uchar>(255));
						Int sourceComponent = bitmap.component_ > 0 ? bitmap.component_ : 3;
						for (Size pixel = 0; pixel < pixelCount; pixel++)
						{
							for (Int channel = 0; channel < sourceComponent && channel < 4; channel++)
							{
								rgba[pixel * 4 + channel] = source[pixel * sourceComponent + channel];
							}
						}
					}

					DirectX::Image image{};
					image.width = static_cast<Size>(bitmap.width_);
					image.height = static_cast<Size>(bitmap.height_);
					image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
					image.rowPitch = static_cast<Size>(bitmap.width_) * 4;
					image.slicePitch = image.rowPitch * bitmap.height_;
					image.pixels = reinterpret_cast<uint8_t*>(rgba.data());

					if (binary)
					{
						DirectX::Blob pngBlob;
						if (SUCCEEDED(DirectX::SaveToWICMemory(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), pngBlob)))
						{
							const unsigned char* pngBegin = reinterpret_cast<const unsigned char*>(pngBlob.GetBufferPointer());
							Size pngOffset = buffer.data.size();
							buffer.data.insert(buffer.data.end(), pngBegin, pngBegin + pngBlob.GetBufferSize());
							while (buffer.data.size() % 4 != 0)
							{
								buffer.data.push_back(0);
							}

							tinygltf::BufferView& imageBufferView = model.bufferViews.emplace_back();
							imageBufferView.buffer = 0;
							imageBufferView.byteOffset = pngOffset;
							imageBufferView.byteLength = pngBlob.GetBufferSize();

							gltfImage.bufferView = static_cast<Int>(model.bufferViews.size()) - 1;
							gltfImage.mimeType = "image/png";
						}
					}
					else
					{
						DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), pngPath.wstring().c_str());
						gltfImage.uri = pngName;
					}
				}
				else if (!binary)
				{
					gltfImage.uri = pngName;
				}

				tinygltf::Texture& texture = model.textures.emplace_back();
				texture.source = static_cast<Int>(bitmapIndex);
				texture.sampler = 0;
			}
			if (!model.textures.empty())
			{
				model.samplers.emplace_back();
			}

			for (const Surface& surface : crister.Surfaces())
			{
				tinygltf::Material& material = model.materials.emplace_back();
				material.name = surface.name_;
				material.pbrMetallicRoughness.baseColorFactor = { surface.baseColor_.x, surface.baseColor_.y, surface.baseColor_.z, surface.baseColor_.w };
				material.pbrMetallicRoughness.metallicFactor = surface.metallic_;
				material.pbrMetallicRoughness.roughnessFactor = surface.roughness_;
				material.emissiveFactor = { surface.emissiveFactor_[0], surface.emissiveFactor_[1], surface.emissiveFactor_[2] };
				material.alphaMode = surface.alphaMode_ == 0 ? "OPAQUE" : surface.alphaMode_ == 1 ? "MASK" : "BLEND";
				material.alphaCutoff = surface.alphaCutoff_;
				material.doubleSided = surface.doubleSided_ != 0;
				if (surface.baseColorTextureIndex_ != 0xFFFFFFFF)
				{
					material.pbrMetallicRoughness.baseColorTexture.index = static_cast<Int>(surface.baseColorTextureIndex_);
				}
				if (surface.normalTextureIndex_ != 0xFFFFFFFF)
				{
					material.normalTexture.index = static_cast<Int>(surface.normalTextureIndex_);
				}
				if (surface.metallicRoughnessTextureIndex_ != 0xFFFFFFFF)
				{
					material.pbrMetallicRoughness.metallicRoughnessTexture.index = static_cast<Int>(surface.metallicRoughnessTextureIndex_);
				}
				if (surface.occlusionTextureIndex_ != 0xFFFFFFFF)
				{
					material.occlusionTexture.index = static_cast<Int>(surface.occlusionTextureIndex_);
				}
				if (surface.emissiveTextureIndex_ != 0xFFFFFFFF)
				{
					material.emissiveTexture.index = static_cast<Int>(surface.emissiveTextureIndex_);
				}
			}

			const DynamicArray<Skin>& skins = crister.Skins();
			for (const Skin& skin : skins)
			{
				tinygltf::Skin& gltfSkin = model.skins.emplace_back();
				for (Int joint : skin.joints_)
				{
					gltfSkin.joints.push_back(joint);
				}
				if (!skin.inverseBindMatrices_.empty())
				{
					DynamicArray<Matrix> mirroredMatrices(skin.inverseBindMatrices_.size());
					for (Size matrixIndex = 0; matrixIndex < skin.inverseBindMatrices_.size(); matrixIndex++)
					{
						Matrix matrix = skin.inverseBindMatrices_[matrixIndex];
						matrix.m[0][1] = -matrix.m[0][1];
						matrix.m[0][2] = -matrix.m[0][2];
						matrix.m[0][3] = -matrix.m[0][3];
						matrix.m[1][0] = -matrix.m[1][0];
						matrix.m[2][0] = -matrix.m[2][0];
						matrix.m[3][0] = -matrix.m[3][0];
						mirroredMatrices[matrixIndex] = matrix;
					}
					gltfSkin.inverseBindMatrices = appendAccessor(mirroredMatrices.data(), mirroredMatrices.size() * sizeof(Matrix), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_MAT4, mirroredMatrices.size(), 0);
				}
			}

			const DynamicArray<Node>& nodes = crister.Nodes();
			for (const Node& node : nodes)
			{
				tinygltf::Node& gltfNode = model.nodes.emplace_back();
				gltfNode.name = node.name_;
				gltfNode.rotation = { node.rotation_.x, -node.rotation_.y, -node.rotation_.z, node.rotation_.w };
				gltfNode.scale = { node.scale_.x, node.scale_.y, node.scale_.z };
				gltfNode.translation = { -node.translation_.x, node.translation_.y, node.translation_.z };
				for (Int child : node.children_)
				{
					gltfNode.children.push_back(child);
				}

				if (node.mesh_ >= 0)
				{
					for (Size groupIndex = 0; groupIndex < meshGroupKeys.size(); groupIndex++)
					{
						if (meshGroupKeys[groupIndex] == node.mesh_)
						{
							gltfNode.mesh = static_cast<Int>(groupIndex);
							break;
						}
					}
					for (const SubMesh& subMesh : subMeshes)
					{
						if (subMesh.meshIndex_ == node.mesh_ && subMesh.skinIndex_ >= 0)
						{
							gltfNode.skin = subMesh.skinIndex_;
							break;
						}
					}
				}
			}

			tinygltf::Scene& scene = model.scenes.emplace_back();
			model.defaultScene = 0;
			for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
			{
				if (nodes[nodeIndex].parentIndex_ < 0)
				{
					scene.nodes.push_back(static_cast<Int>(nodeIndex));
				}
			}

			tinygltf::TinyGLTF writer;
			return writer.WriteGltfSceneToFile(&model, outputPath.string(), binary, binary, !binary, binary);
		}
		case ModelFormat::Fbx:
		{
			if (crister.vertices_.empty())
			{
				return false;
			}

			Float unitScale = profile.unitScale_;

			FbxManager* manager = FbxManager::Create();
			manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));
			FbxScene* scene = FbxScene::Create(manager, outputPath.stem().string().c_str());
			scene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::DirectX);
			scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::m);

			const DynamicArray<Node>& nodes = crister.Nodes();
			const DynamicArray<Skin>& skins = crister.Skins();
			const DynamicArray<Surface>& surfaces = crister.Surfaces();
			const DynamicArray<SubMesh>& subMeshes = crister.SubMeshes();

			DynamicArray<Bool> isJoint(nodes.size(), false);
			for (const Skin& skin : skins)
			{
				for (Int joint : skin.joints_)
				{
					if (joint >= 0 && joint < static_cast<Int>(nodes.size()))
					{
						isJoint[joint] = true;
					}
				}
			}

			DynamicArray<FbxNode*> fbxNodes(nodes.size(), nullptr);
			for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
			{
				const Node& node = nodes[nodeIndex];
				std::string nodeName = node.name_.empty() ? ("node_" + std::to_string(nodeIndex)) : node.name_;
				FbxNode* fbxNode = FbxNode::Create(scene, nodeName.c_str());

				FbxQuaternion rotationQuaternion(node.rotation_.x, node.rotation_.y, node.rotation_.z, node.rotation_.w);
				FbxAMatrix rotationMatrix;
				rotationMatrix.SetQ(rotationQuaternion);
				FbxVector4 euler = rotationMatrix.GetR();
				fbxNode->LclTranslation.Set(FbxDouble3(node.translation_.x * unitScale, node.translation_.y * unitScale, node.translation_.z * unitScale));
				fbxNode->LclRotation.Set(FbxDouble3(euler[0], euler[1], euler[2]));
				fbxNode->LclScaling.Set(FbxDouble3(node.scale_.x, node.scale_.y, node.scale_.z));

				if (isJoint[nodeIndex])
				{
					FbxSkeleton* skeleton = FbxSkeleton::Create(scene, "");
					skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
					fbxNode->SetNodeAttribute(skeleton);
				}
				fbxNodes[nodeIndex] = fbxNode;
			}
			for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
			{
				for (Int child : nodes[nodeIndex].children_)
				{
					if (child >= 0 && child < static_cast<Int>(nodes.size()))
					{
						fbxNodes[nodeIndex]->AddChild(fbxNodes[child]);
					}
				}
			}
			for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
			{
				if (nodes[nodeIndex].parentIndex_ < 0)
				{
					scene->GetRootNode()->AddChild(fbxNodes[nodeIndex]);
				}
			}

			DynamicArray<FbxFileTexture*> fbxTextures(crister.bitmaps_.size(), nullptr);
			for (Size bitmapIndex = 0; bitmapIndex < crister.bitmaps_.size(); bitmapIndex++)
			{
				const Bitmap& bitmap = crister.bitmaps_[bitmapIndex];
				std::string pngName = outputPath.stem().string() + "_" + std::to_string(bitmapIndex) + ".png";
				std::filesystem::path pngPath = outputPath.parent_path() / pngName;

				if (bitmap.width_ > 0 && bitmap.height_ > 0 && !bitmap.cacheData_.empty())
				{
					DynamicArray<Uchar> rgba;
					const Uchar* source = reinterpret_cast<const Uchar*>(bitmap.cacheData_.data());
					Size pixelCount = static_cast<Size>(bitmap.width_) * bitmap.height_;
					if (bitmap.component_ == 4)
					{
						rgba.assign(source, source + pixelCount * 4);
					}
					else
					{
						rgba.resize(pixelCount * 4, static_cast<Uchar>(255));
						Int sourceComponent = bitmap.component_ > 0 ? bitmap.component_ : 3;
						for (Size pixel = 0; pixel < pixelCount; pixel++)
						{
							for (Int channel = 0; channel < sourceComponent && channel < 4; channel++)
							{
								rgba[pixel * 4 + channel] = source[pixel * sourceComponent + channel];
							}
						}
					}

					DirectX::Image image{};
					image.width = static_cast<Size>(bitmap.width_);
					image.height = static_cast<Size>(bitmap.height_);
					image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
					image.rowPitch = static_cast<Size>(bitmap.width_) * 4;
					image.slicePitch = image.rowPitch * bitmap.height_;
					image.pixels = reinterpret_cast<uint8_t*>(rgba.data());
					DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), pngPath.wstring().c_str());
				}

				FbxFileTexture* fbxTexture = FbxFileTexture::Create(scene, ("texture_" + std::to_string(bitmapIndex)).c_str());
				fbxTexture->SetFileName(pngPath.string().c_str());
				fbxTexture->SetTextureUse(FbxTexture::eStandard);
				fbxTexture->SetMappingType(FbxTexture::eUV);
				fbxTexture->SetMaterialUse(FbxFileTexture::eModelMaterial);
				fbxTextures[bitmapIndex] = fbxTexture;
			}

			DynamicArray<FbxSurfaceMaterial*> fbxMaterials(surfaces.size(), nullptr);
			for (Size surfaceIndex = 0; surfaceIndex < surfaces.size(); surfaceIndex++)
			{
				const Surface& surface = surfaces[surfaceIndex];
				FbxSurfacePhong* material = FbxSurfacePhong::Create(scene, surface.name_.c_str());
				material->Diffuse.Set(FbxDouble3(surface.baseColor_.x, surface.baseColor_.y, surface.baseColor_.z));
				material->Emissive.Set(FbxDouble3(surface.emissiveFactor_[0], surface.emissiveFactor_[1], surface.emissiveFactor_[2]));
				material->Shininess.Set((1.0 - surface.roughness_) * 100.0);
				if (surface.baseColorTextureIndex_ < fbxTextures.size() && fbxTextures[surface.baseColorTextureIndex_])
				{
					material->Diffuse.ConnectSrcObject(fbxTextures[surface.baseColorTextureIndex_]);
				}
				if (surface.normalTextureIndex_ < fbxTextures.size() && fbxTextures[surface.normalTextureIndex_])
				{
					material->NormalMap.ConnectSrcObject(fbxTextures[surface.normalTextureIndex_]);
				}
				fbxMaterials[surfaceIndex] = material;
			}

			DynamicArray<Int> meshGroupKeys;
			for (const SubMesh& subMesh : subMeshes)
			{
				Bool known = false;
				for (Int key : meshGroupKeys)
				{
					if (key == subMesh.meshIndex_)
					{
						known = true;
						break;
					}
				}
				if (!known)
				{
					meshGroupKeys.push_back(subMesh.meshIndex_);
				}
			}

			for (Int groupKey : meshGroupKeys)
			{
				DynamicArray<const SubMesh*> groupSubMeshes;
				Uint32 totalControlPoints = 0;
				for (const SubMesh& subMesh : subMeshes)
				{
					if (subMesh.meshIndex_ == groupKey)
					{
						groupSubMeshes.push_back(&subMesh);
						totalControlPoints += subMesh.vertexCount_;
					}
				}

				FbxMesh* mesh = FbxMesh::Create(scene, "");
				mesh->InitControlPoints(static_cast<int>(totalControlPoints));

				FbxGeometryElementNormal* normalElement = mesh->CreateElementNormal();
				normalElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
				normalElement->SetReferenceMode(FbxGeometryElement::eDirect);
				FbxGeometryElementUV* uvElement = mesh->CreateElementUV("uv");
				uvElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
				uvElement->SetReferenceMode(FbxGeometryElement::eDirect);
				FbxGeometryElementMaterial* materialElement = mesh->CreateElementMaterial();
				materialElement->SetMappingMode(FbxGeometryElement::eByPolygon);
				materialElement->SetReferenceMode(FbxGeometryElement::eIndexToDirect);

				Uint32 controlPointCursor = 0;
				for (const SubMesh* subMesh : groupSubMeshes)
				{
					for (Uint32 vertexIndex = 0; vertexIndex < subMesh->vertexCount_; vertexIndex++)
					{
						const Vertex& vertex = crister.vertices_[subMesh->vertexOffset_ + vertexIndex];
						mesh->SetControlPointAt(FbxVector4(vertex.position_.x * unitScale, vertex.position_.y * unitScale, vertex.position_.z * unitScale), static_cast<int>(controlPointCursor + vertexIndex));
						normalElement->GetDirectArray().Add(FbxVector4(vertex.normal_.x, vertex.normal_.y, vertex.normal_.z));
						uvElement->GetDirectArray().Add(FbxVector2(vertex.texcoord_.x, 1.0 - vertex.texcoord_.y));
					}
					controlPointCursor += subMesh->vertexCount_;
				}

				DynamicArray<Uint32> groupSurfaces;
				for (const SubMesh* subMesh : groupSubMeshes)
				{
					Bool known = false;
					for (Uint32 surfaceIndex : groupSurfaces)
					{
						if (surfaceIndex == subMesh->surfaceIndex_)
						{
							known = true;
							break;
						}
					}
					if (!known)
					{
						groupSurfaces.push_back(subMesh->surfaceIndex_);
					}
				}

				FbxNode* meshNode = nullptr;
				for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
				{
					if (nodes[nodeIndex].mesh_ == groupKey)
					{
						meshNode = fbxNodes[nodeIndex];
						break;
					}
				}
				if (!meshNode)
				{
					meshNode = FbxNode::Create(scene, ("mesh_" + std::to_string(groupKey)).c_str());
					scene->GetRootNode()->AddChild(meshNode);
				}
				meshNode->SetNodeAttribute(mesh);
				for (Uint32 surfaceIndex : groupSurfaces)
				{
					if (surfaceIndex < fbxMaterials.size() && fbxMaterials[surfaceIndex])
					{
						meshNode->AddMaterial(fbxMaterials[surfaceIndex]);
					}
				}

				controlPointCursor = 0;
				for (const SubMesh* subMesh : groupSubMeshes)
				{
					int nodeMaterialIndex = 0;
					for (Size slot = 0; slot < groupSurfaces.size(); slot++)
					{
						if (groupSurfaces[slot] == subMesh->surfaceIndex_)
						{
							nodeMaterialIndex = static_cast<int>(slot);
							break;
						}
					}
					for (Uint32 triangleStart = 0; triangleStart + 2 < subMesh->indexCount_; triangleStart += 3)
					{
						mesh->BeginPolygon(nodeMaterialIndex);
						for (Uint32 corner = 0; corner < 3; corner++)
						{
							Uint32 globalIndex = crister.vertexIndices_[subMesh->indexOffset_ + triangleStart + corner];
							mesh->AddPolygon(static_cast<int>(controlPointCursor + (globalIndex - subMesh->vertexOffset_)));
						}
						mesh->EndPolygon();
					}
					controlPointCursor += subMesh->vertexCount_;
				}

				Int skinIndex = -1;
				for (const SubMesh* subMesh : groupSubMeshes)
				{
					if (subMesh->skinIndex_ >= 0)
					{
						skinIndex = subMesh->skinIndex_;
						break;
					}
				}
				if (skinIndex >= 0 && skinIndex < static_cast<Int>(skins.size()))
				{
					const Skin& skin = skins[skinIndex];
					FbxSkin* fbxSkin = FbxSkin::Create(scene, "");
					DynamicArray<FbxCluster*> clusters(skin.joints_.size(), nullptr);
					for (Size jointIndex = 0; jointIndex < skin.joints_.size(); jointIndex++)
					{
						Int nodeIndex = skin.joints_[jointIndex];
						if (nodeIndex < 0 || nodeIndex >= static_cast<Int>(nodes.size()))
						{
							continue;
						}
						FbxCluster* cluster = FbxCluster::Create(scene, "");
						cluster->SetLink(fbxNodes[nodeIndex]);
						cluster->SetLinkMode(FbxCluster::eTotalOne);
						cluster->SetTransformMatrix(meshNode->EvaluateGlobalTransform());
						cluster->SetTransformLinkMatrix(fbxNodes[nodeIndex]->EvaluateGlobalTransform());
						clusters[jointIndex] = cluster;
						fbxSkin->AddCluster(cluster);
					}

					controlPointCursor = 0;
					for (const SubMesh* subMesh : groupSubMeshes)
					{
						for (Uint32 vertexIndex = 0; vertexIndex < subMesh->vertexCount_; vertexIndex++)
						{
							const Vertex& vertex = crister.vertices_[subMesh->vertexOffset_ + vertexIndex];
							Uint32 vertexJoints[4] = { vertex.joints_.x, vertex.joints_.y, vertex.joints_.z, vertex.joints_.w };
							Float vertexWeights[4] = { vertex.weights_.x, vertex.weights_.y, vertex.weights_.z, vertex.weights_.w };
							for (Int influence = 0; influence < 4; influence++)
							{
								if (vertexWeights[influence] > 0.0f && vertexJoints[influence] < clusters.size() && clusters[vertexJoints[influence]])
								{
									clusters[vertexJoints[influence]]->AddControlPointIndex(static_cast<int>(controlPointCursor + vertexIndex), vertexWeights[influence]);
								}
							}
						}
						controlPointCursor += subMesh->vertexCount_;
					}
					mesh->AddDeformer(fbxSkin);
				}
			}

			scene->GetGlobalSettings().SetSystemUnit(unitScale >= 99.0f ? FbxSystemUnit::cm : FbxSystemUnit::m);

			switch (profile.axis_)
			{
			case ExportAxis::MayaYUp:
			{
				FbxAxisSystem mayaAxisSystem(FbxAxisSystem::MayaYUp);
				mayaAxisSystem.DeepConvertScene(scene);
				break;
			}
			case ExportAxis::UnrealZUpLeftHanded:
			{
				FbxAxisSystem unrealAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
				unrealAxisSystem.DeepConvertScene(scene);
				break;
			}
			case ExportAxis::EngineNativeDirectX:
				[[fallthrough]];
			case ExportAxis::GltfRightHandedYUp:
			{
				break;
			}
			}

			FbxExporter* exporter = FbxExporter::Create(manager, "");
			Bool result = exporter->Initialize(outputPath.string().c_str(), -1, manager->GetIOSettings());
			if (result)
			{
				result = exporter->Export(scene);
			}
			exporter->Destroy();
			manager->Destroy();
			return result;
		}
		}

		return false;
	}

	Bool ModelExporter::Export(const tinygltf::Model& model, const ExportProfile& profile, String filePath)
	{
		std::filesystem::path outputPath(filePath.c_str());
		std::error_code directoryError;
		std::filesystem::create_directories(outputPath.parent_path(), directoryError);

		if (profile.format_ == ModelFormat::Gltf)
		{
			tinygltf::TinyGLTF writer;
			return writer.WriteGltfSceneToFile(&model, outputPath.string(), profile.binary_, profile.binary_, !profile.binary_, profile.binary_);
		}

		if (model.meshes.empty())
		{
			return false;
		}

		Float unitScale = profile.unitScale_;

		auto readFloats = [&](Int accessorIndex, Int components)
		{
			DynamicArray<Float> values;
			if (accessorIndex < 0 || accessorIndex >= static_cast<Int>(model.accessors.size()))
			{
				return values;
			}
			const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
			const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
			Size stride = bufferView.byteStride != 0 ? bufferView.byteStride : static_cast<Size>(components) * sizeof(Float);
			const unsigned char* base = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
			values.resize(accessor.count * static_cast<Size>(components));
			for (Size element = 0; element < accessor.count; element++)
			{
				const Float* source = reinterpret_cast<const Float*>(base + element * stride);
				for (Int component = 0; component < components; component++)
				{
					values[element * static_cast<Size>(components) + component] = source[component];
				}
			}
			return values;
		};

		auto readUints = [&](Int accessorIndex, Int components)
		{
			DynamicArray<Uint32> values;
			if (accessorIndex < 0 || accessorIndex >= static_cast<Int>(model.accessors.size()))
			{
				return values;
			}
			const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
			const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
			Size componentSize = accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? 4 : accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 1;
			Size stride = bufferView.byteStride != 0 ? bufferView.byteStride : componentSize * static_cast<Size>(components);
			const unsigned char* base = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
			values.resize(accessor.count * static_cast<Size>(components));
			for (Size element = 0; element < accessor.count; element++)
			{
				const unsigned char* source = base + element * stride;
				for (Int component = 0; component < components; component++)
				{
					const unsigned char* value = source + component * componentSize;
					if (componentSize == 4)
					{
						values[element * static_cast<Size>(components) + component] = *reinterpret_cast<const Uint32*>(value);
					}
					else if (componentSize == 2)
					{
						values[element * static_cast<Size>(components) + component] = *reinterpret_cast<const Uint16*>(value);
					}
					else
					{
						values[element * static_cast<Size>(components) + component] = *value;
					}
				}
			}
			return values;
		};

		FbxManager* manager = FbxManager::Create();
		manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));
		FbxScene* scene = FbxScene::Create(manager, outputPath.stem().string().c_str());
		scene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::OpenGL);
		scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::m);

		DynamicArray<Bool> isJoint(model.nodes.size(), false);
		for (const tinygltf::Skin& skin : model.skins)
		{
			for (Int joint : skin.joints)
			{
				if (joint >= 0 && joint < static_cast<Int>(model.nodes.size()))
				{
					isJoint[joint] = true;
				}
			}
		}

		DynamicArray<FbxNode*> fbxNodes(model.nodes.size(), nullptr);
		for (Size nodeIndex = 0; nodeIndex < model.nodes.size(); nodeIndex++)
		{
			const tinygltf::Node& node = model.nodes[nodeIndex];
			std::string nodeName = node.name.empty() ? ("node_" + std::to_string(nodeIndex)) : node.name;
			FbxNode* fbxNode = FbxNode::Create(scene, nodeName.c_str());

			Vector3 translation(0.0f, 0.0f, 0.0f);
			Quaternion rotation = Quaternion::Identity;
			Vector3 scale(1.0f, 1.0f, 1.0f);
			if (node.translation.size() == 3)
			{
				translation = Vector3(static_cast<Float>(node.translation[0]), static_cast<Float>(node.translation[1]), static_cast<Float>(node.translation[2]));
			}
			if (node.rotation.size() == 4)
			{
				rotation = Quaternion(static_cast<Float>(node.rotation[0]), static_cast<Float>(node.rotation[1]), static_cast<Float>(node.rotation[2]), static_cast<Float>(node.rotation[3]));
			}
			if (node.scale.size() == 3)
			{
				scale = Vector3(static_cast<Float>(node.scale[0]), static_cast<Float>(node.scale[1]), static_cast<Float>(node.scale[2]));
			}

			FbxQuaternion rotationQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);
			FbxAMatrix rotationMatrix;
			rotationMatrix.SetQ(rotationQuaternion);
			FbxVector4 euler = rotationMatrix.GetR();
			fbxNode->LclTranslation.Set(FbxDouble3(translation.x * unitScale, translation.y * unitScale, translation.z * unitScale));
			fbxNode->LclRotation.Set(FbxDouble3(euler[0], euler[1], euler[2]));
			fbxNode->LclScaling.Set(FbxDouble3(scale.x, scale.y, scale.z));

			if (isJoint[nodeIndex])
			{
				FbxSkeleton* skeleton = FbxSkeleton::Create(scene, "");
				skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
				fbxNode->SetNodeAttribute(skeleton);
			}
			fbxNodes[nodeIndex] = fbxNode;
		}

		DynamicArray<Bool> isChild(model.nodes.size(), false);
		for (Size nodeIndex = 0; nodeIndex < model.nodes.size(); nodeIndex++)
		{
			for (Int child : model.nodes[nodeIndex].children)
			{
				if (child >= 0 && child < static_cast<Int>(model.nodes.size()))
				{
					fbxNodes[nodeIndex]->AddChild(fbxNodes[child]);
					isChild[child] = true;
				}
			}
		}
		for (Size nodeIndex = 0; nodeIndex < model.nodes.size(); nodeIndex++)
		{
			if (!isChild[nodeIndex])
			{
				scene->GetRootNode()->AddChild(fbxNodes[nodeIndex]);
			}
		}

		DynamicArray<FbxSurfaceMaterial*> fbxMaterials(model.materials.size(), nullptr);
		for (Size materialIndex = 0; materialIndex < model.materials.size(); materialIndex++)
		{
			const tinygltf::Material& material = model.materials[materialIndex];
			std::string materialName = material.name.empty() ? ("material_" + std::to_string(materialIndex)) : material.name;
			FbxSurfacePhong* fbxMaterial = FbxSurfacePhong::Create(scene, materialName.c_str());
			const std::vector<double>& baseColor = material.pbrMetallicRoughness.baseColorFactor;
			if (baseColor.size() == 4)
			{
				fbxMaterial->Diffuse.Set(FbxDouble3(baseColor[0], baseColor[1], baseColor[2]));
			}
			if (material.emissiveFactor.size() == 3)
			{
				fbxMaterial->Emissive.Set(FbxDouble3(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]));
			}
			fbxMaterial->Shininess.Set((1.0 - material.pbrMetallicRoughness.roughnessFactor) * 100.0);
			fbxMaterials[materialIndex] = fbxMaterial;
		}

		for (Size meshIndex = 0; meshIndex < model.meshes.size(); meshIndex++)
		{
			const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

			Int ownerNode = -1;
			for (Size nodeIndex = 0; nodeIndex < model.nodes.size(); nodeIndex++)
			{
				if (model.nodes[nodeIndex].mesh == static_cast<Int>(meshIndex))
				{
					ownerNode = static_cast<Int>(nodeIndex);
					break;
				}
			}

			Size totalControlPoints = 0;
			for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
			{
				auto found = primitive.attributes.find("POSITION");
				if (found != primitive.attributes.end())
				{
					totalControlPoints += model.accessors[found->second].count;
				}
			}
			if (totalControlPoints == 0)
			{
				continue;
			}

			FbxMesh* fbxMesh = FbxMesh::Create(scene, "");
			fbxMesh->InitControlPoints(static_cast<int>(totalControlPoints));
			FbxGeometryElementNormal* normalElement = fbxMesh->CreateElementNormal();
			normalElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
			normalElement->SetReferenceMode(FbxGeometryElement::eDirect);
			FbxGeometryElementUV* uvElement = fbxMesh->CreateElementUV("uv");
			uvElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
			uvElement->SetReferenceMode(FbxGeometryElement::eDirect);
			FbxGeometryElementMaterial* materialElement = fbxMesh->CreateElementMaterial();
			materialElement->SetMappingMode(FbxGeometryElement::eByPolygon);
			materialElement->SetReferenceMode(FbxGeometryElement::eIndexToDirect);

			FbxNode* meshNode = ownerNode >= 0 ? fbxNodes[ownerNode] : FbxNode::Create(scene, ("mesh_" + std::to_string(meshIndex)).c_str());
			if (ownerNode < 0)
			{
				scene->GetRootNode()->AddChild(meshNode);
			}
			meshNode->SetNodeAttribute(fbxMesh);

			DynamicArray<Int> usedMaterials;
			for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
			{
				Bool known = false;
				for (Int used : usedMaterials)
				{
					if (used == primitive.material)
					{
						known = true;
						break;
					}
				}
				if (!known)
				{
					usedMaterials.push_back(primitive.material);
				}
			}
			for (Int used : usedMaterials)
			{
				if (used >= 0 && used < static_cast<Int>(fbxMaterials.size()) && fbxMaterials[used])
				{
					meshNode->AddMaterial(fbxMaterials[used]);
				}
			}

			Int skinIndex = ownerNode >= 0 ? model.nodes[ownerNode].skin : -1;

			Size controlPointCursor = 0;
			DynamicArray<Uint32> allJoints;
			DynamicArray<Float> allWeights;
			for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
			{
				auto positionIt = primitive.attributes.find("POSITION");
				auto normalIt = primitive.attributes.find("NORMAL");
				auto texcoordIt = primitive.attributes.find("TEXCOORD_0");
				auto jointsIt = primitive.attributes.find("JOINTS_0");
				auto weightsIt = primitive.attributes.find("WEIGHTS_0");
				if (positionIt == primitive.attributes.end())
				{
					continue;
				}

				DynamicArray<Float> positions = readFloats(positionIt->second, 3);
				DynamicArray<Float> normals = normalIt != primitive.attributes.end() ? readFloats(normalIt->second, 3) : DynamicArray<Float>();
				DynamicArray<Float> texcoords = texcoordIt != primitive.attributes.end() ? readFloats(texcoordIt->second, 2) : DynamicArray<Float>();
				DynamicArray<Uint32> primitiveJoints = jointsIt != primitive.attributes.end() ? readUints(jointsIt->second, 4) : DynamicArray<Uint32>();
				DynamicArray<Float> primitiveWeights = weightsIt != primitive.attributes.end() ? readFloats(weightsIt->second, 4) : DynamicArray<Float>();

				Size vertexCount = positions.size() / 3;
				for (Size vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
				{
					fbxMesh->SetControlPointAt(FbxVector4(positions[vertexIndex * 3 + 0] * unitScale, positions[vertexIndex * 3 + 1] * unitScale, positions[vertexIndex * 3 + 2] * unitScale), static_cast<int>(controlPointCursor + vertexIndex));
					if (normals.size() >= (vertexIndex + 1) * 3)
					{
						normalElement->GetDirectArray().Add(FbxVector4(normals[vertexIndex * 3 + 0], normals[vertexIndex * 3 + 1], normals[vertexIndex * 3 + 2]));
					}
					else
					{
						normalElement->GetDirectArray().Add(FbxVector4(0.0, 1.0, 0.0));
					}
					if (texcoords.size() >= (vertexIndex + 1) * 2)
					{
						uvElement->GetDirectArray().Add(FbxVector2(texcoords[vertexIndex * 2 + 0], 1.0 - texcoords[vertexIndex * 2 + 1]));
					}
					else
					{
						uvElement->GetDirectArray().Add(FbxVector2(0.0, 0.0));
					}

					for (Int influence = 0; influence < 4; influence++)
					{
						allJoints.push_back(primitiveJoints.size() >= (vertexIndex + 1) * 4 ? primitiveJoints[vertexIndex * 4 + influence] : 0);
						allWeights.push_back(primitiveWeights.size() >= (vertexIndex + 1) * 4 ? primitiveWeights[vertexIndex * 4 + influence] : 0.0f);
					}
				}

				Int nodeMaterialSlot = 0;
				for (Size slot = 0; slot < usedMaterials.size(); slot++)
				{
					if (usedMaterials[slot] == primitive.material)
					{
						nodeMaterialSlot = static_cast<Int>(slot);
						break;
					}
				}

				DynamicArray<Uint32> indices = readUints(primitive.indices, 1);
				for (Size triangleStart = 0; triangleStart + 2 < indices.size(); triangleStart += 3)
				{
					fbxMesh->BeginPolygon(nodeMaterialSlot);
					for (Uint32 corner = 0; corner < 3; corner++)
					{
						fbxMesh->AddPolygon(static_cast<int>(controlPointCursor + indices[triangleStart + corner]));
					}
					fbxMesh->EndPolygon();
				}

				controlPointCursor += vertexCount;
			}

			if (skinIndex >= 0 && skinIndex < static_cast<Int>(model.skins.size()))
			{
				const tinygltf::Skin& skin = model.skins[skinIndex];
				DynamicArray<Float> inverseBindMatrices = readFloats(skin.inverseBindMatrices, 16);

				FbxSkin* fbxSkin = FbxSkin::Create(scene, "");
				DynamicArray<FbxCluster*> clusters(skin.joints.size(), nullptr);
				for (Size jointIndex = 0; jointIndex < skin.joints.size(); jointIndex++)
				{
					Int nodeIndex = skin.joints[jointIndex];
					if (nodeIndex < 0 || nodeIndex >= static_cast<Int>(model.nodes.size()))
					{
						continue;
					}
					FbxCluster* cluster = FbxCluster::Create(scene, "");
					cluster->SetLink(fbxNodes[nodeIndex]);
					cluster->SetLinkMode(FbxCluster::eTotalOne);
					cluster->SetTransformMatrix(meshNode->EvaluateGlobalTransform());
					cluster->SetTransformLinkMatrix(fbxNodes[nodeIndex]->EvaluateGlobalTransform());
					clusters[jointIndex] = cluster;
					fbxSkin->AddCluster(cluster);
				}

				for (Size vertexIndex = 0; vertexIndex < controlPointCursor; vertexIndex++)
				{
					for (Int influence = 0; influence < 4; influence++)
					{
						Uint32 joint = allJoints[vertexIndex * 4 + influence];
						Float weight = allWeights[vertexIndex * 4 + influence];
						if (weight > 0.0f && joint < clusters.size() && clusters[joint])
						{
							clusters[joint]->AddControlPointIndex(static_cast<int>(vertexIndex), weight);
						}
					}
				}
				fbxMesh->AddDeformer(fbxSkin);
			}
		}

		scene->GetGlobalSettings().SetSystemUnit(unitScale >= 99.0f ? FbxSystemUnit::cm : FbxSystemUnit::m);

		switch (profile.axis_)
		{
		case ExportAxis::MayaYUp:
		{
			FbxAxisSystem mayaAxisSystem(FbxAxisSystem::MayaYUp);
			mayaAxisSystem.DeepConvertScene(scene);
			break;
		}
		case ExportAxis::UnrealZUpLeftHanded:
		{
			FbxAxisSystem unrealAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
			unrealAxisSystem.DeepConvertScene(scene);
			break;
		}
		case ExportAxis::EngineNativeDirectX:
		{
			FbxAxisSystem directXAxisSystem(FbxAxisSystem::DirectX);
			directXAxisSystem.DeepConvertScene(scene);
			break;
		}
		case ExportAxis::GltfRightHandedYUp:
		{
			break;
		}
		}

		FbxExporter* exporter = FbxExporter::Create(manager, "");
		Bool result = exporter->Initialize(outputPath.string().c_str(), -1, manager->GetIOSettings());
		if (result)
		{
			result = exporter->Export(scene);
		}
		exporter->Destroy();
		manager->Destroy();
		return result;
	}
}
