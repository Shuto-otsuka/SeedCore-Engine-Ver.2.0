#include <GraphicsEngine/Model/ModelExporter.h>
#include <GraphicsEngine/Model/Crister.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	Bool ModelExporter::Export(const Crister& crister, ModelFormat format, String filePath)
	{
		std::filesystem::path outputPath(filePath.c_str());
		std::error_code directoryError;
		std::filesystem::create_directories(outputPath.parent_path(), directoryError);

		switch (format)
		{
		case ModelFormat::Gltf:
		{
			if (crister.vertices_.empty())
			{
				return false;
			}

			std::string outputExtension = outputPath.extension().string();
			std::ranges::transform(outputExtension, outputExtension.begin(), [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
			Bool binary = outputExtension == ".glb";

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
				fbxNode->LclTranslation.Set(FbxDouble3(node.translation_.x, node.translation_.y, node.translation_.z));
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
						mesh->SetControlPointAt(FbxVector4(vertex.position_.x, vertex.position_.y, vertex.position_.z), static_cast<int>(controlPointCursor + vertexIndex));
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
}
