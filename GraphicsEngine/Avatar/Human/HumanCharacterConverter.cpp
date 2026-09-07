#include <GraphicsEngine/Avatar/Human/HumanCharacterConverter.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>

namespace SeedCore
{
	void HumanCharacterConverter::Convert(const HumanCharacterModel& model, const HumanCharacterEvaluator& evaluator, tinygltf::Model& outModel)
	{
		if (!model.IsLoaded() || evaluator.Positions().empty())
		{
			return;
		}

		Uint32 bodyVertexCount = model.BodyVertexCount();

		outModel.asset.version = "2.0";
		outModel.asset.generator = "SeedCore";

		tinygltf::Buffer& buffer = outModel.buffers.emplace_back();

		auto appendAccessor = [&](const void* data, Size byteSize, Int componentType, Int accessorType, Size elementCount, Int bufferViewTarget) -> Int
		{
			Size byteOffset = buffer.data.size();
			const unsigned char* begin = reinterpret_cast<const unsigned char*>(data);
			buffer.data.insert(buffer.data.end(), begin, begin + byteSize);
			while (buffer.data.size() % 4 != 0)
			{
				buffer.data.push_back(0);
			}

			tinygltf::BufferView& bufferView = outModel.bufferViews.emplace_back();
			bufferView.buffer = 0;
			bufferView.byteOffset = byteOffset;
			bufferView.byteLength = byteSize;
			if (bufferViewTarget != 0)
			{
				bufferView.target = bufferViewTarget;
			}

			tinygltf::Accessor& accessor = outModel.accessors.emplace_back();
			accessor.bufferView = static_cast<Int>(outModel.bufferViews.size()) - 1;
			accessor.byteOffset = 0;
			accessor.componentType = componentType;
			accessor.type = accessorType;
			accessor.count = elementCount;
			return static_cast<Int>(outModel.accessors.size()) - 1;
		};

		std::span<const Vector3> deformedPositions = evaluator.Positions();
		std::span<const Vector3> deformedNormals = evaluator.Normals();
		std::span<const Vector2> sourceTexcoords = model.Texcoords();
		std::span<const Float> sourceSkinWeights = model.SkinWeights();
		std::span<const Uint32> sourceSkinIndices = model.SkinIndices();

		DynamicArray<Vector3> positions(bodyVertexCount);
		DynamicArray<Vector3> normals(bodyVertexCount);
		DynamicArray<Vector2> texcoords(bodyVertexCount);
		DynamicArray<Uint16> joints(static_cast<Size>(bodyVertexCount) * 4);
		DynamicArray<Vector4> weights(bodyVertexCount);
		Vector3 positionMin = Vector3(-deformedPositions[0].x, deformedPositions[0].y, deformedPositions[0].z);
		Vector3 positionMax = positionMin;
		for (Uint32 vertexIndex = 0; vertexIndex < bodyVertexCount; vertexIndex++)
		{
			positions[vertexIndex] = Vector3(-deformedPositions[vertexIndex].x, deformedPositions[vertexIndex].y, deformedPositions[vertexIndex].z);
			normals[vertexIndex] = Vector3(-deformedNormals[vertexIndex].x, deformedNormals[vertexIndex].y, deformedNormals[vertexIndex].z);
			texcoords[vertexIndex] = sourceTexcoords[vertexIndex];
			joints[vertexIndex * 4 + 0] = static_cast<Uint16>(sourceSkinIndices[vertexIndex * 4 + 0]);
			joints[vertexIndex * 4 + 1] = static_cast<Uint16>(sourceSkinIndices[vertexIndex * 4 + 1]);
			joints[vertexIndex * 4 + 2] = static_cast<Uint16>(sourceSkinIndices[vertexIndex * 4 + 2]);
			joints[vertexIndex * 4 + 3] = static_cast<Uint16>(sourceSkinIndices[vertexIndex * 4 + 3]);
			weights[vertexIndex] = Vector4(sourceSkinWeights[vertexIndex * 4 + 0], sourceSkinWeights[vertexIndex * 4 + 1], sourceSkinWeights[vertexIndex * 4 + 2], sourceSkinWeights[vertexIndex * 4 + 3]);
			positionMin = Vector3::Min(positionMin, positions[vertexIndex]);
			positionMax = Vector3::Max(positionMax, positions[vertexIndex]);
		}

		tinygltf::Mesh& mesh = outModel.meshes.emplace_back();
		tinygltf::Primitive& primitive = mesh.primitives.emplace_back();
		primitive.mode = TINYGLTF_MODE_TRIANGLES;
		primitive.material = -1;

		Int positionAccessor = appendAccessor(positions.data(), positions.size() * sizeof(Vector3), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, bodyVertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
		outModel.accessors[positionAccessor].minValues = { positionMin.x, positionMin.y, positionMin.z };
		outModel.accessors[positionAccessor].maxValues = { positionMax.x, positionMax.y, positionMax.z };
		primitive.attributes["POSITION"] = positionAccessor;
		primitive.attributes["NORMAL"] = appendAccessor(normals.data(), normals.size() * sizeof(Vector3), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, bodyVertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
		primitive.attributes["TEXCOORD_0"] = appendAccessor(texcoords.data(), texcoords.size() * sizeof(Vector2), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2, bodyVertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
		primitive.attributes["JOINTS_0"] = appendAccessor(joints.data(), joints.size() * sizeof(Uint16), TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, TINYGLTF_TYPE_VEC4, bodyVertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);
		primitive.attributes["WEIGHTS_0"] = appendAccessor(weights.data(), weights.size() * sizeof(Vector4), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4, bodyVertexCount, TINYGLTF_TARGET_ARRAY_BUFFER);

		std::span<const Uint32> triangles = model.Triangles();
		DynamicArray<Uint32> indices(triangles.begin(), triangles.end());
		primitive.indices = appendAccessor(indices.data(), indices.size() * sizeof(Uint32), TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR, indices.size(), TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

		std::span<const Vector3> jointHeads = evaluator.JointHeads();
		std::span<const Char* const> boneNames = HumanCharacterModel::BoneNames();

		DynamicArray<Vector3> mirroredJointHeads(model.BoneCount());
		for (Uint32 boneIndex = 0; boneIndex < model.BoneCount(); boneIndex++)
		{
			mirroredJointHeads[boneIndex] = Vector3(-jointHeads[boneIndex].x, jointHeads[boneIndex].y, jointHeads[boneIndex].z);
		}

		DynamicArray<Float> inverseBindMatrices(static_cast<Size>(model.BoneCount()) * 16, 0.0f);
		Int rootNode = -1;
		for (Uint32 boneIndex = 0; boneIndex < model.BoneCount(); boneIndex++)
		{
			const HumanCharacterBone& bone = model.Bone(boneIndex);
			tinygltf::Node& boneNode = outModel.nodes.emplace_back();
			boneNode.name = boneIndex < boneNames.size() ? std::string(boneNames[boneIndex]) : ("bone_" + std::to_string(boneIndex));

			Vector3 parentHead = bone.parentIndex_ >= 0 ? mirroredJointHeads[bone.parentIndex_] : Vector3(0.0f, 0.0f, 0.0f);
			Vector3 localTranslation = mirroredJointHeads[boneIndex] - parentHead;
			boneNode.translation = { localTranslation.x, localTranslation.y, localTranslation.z };

			Size matrixBase = static_cast<Size>(boneIndex) * 16;
			inverseBindMatrices[matrixBase + 0] = 1.0f;
			inverseBindMatrices[matrixBase + 5] = 1.0f;
			inverseBindMatrices[matrixBase + 10] = 1.0f;
			inverseBindMatrices[matrixBase + 15] = 1.0f;
			inverseBindMatrices[matrixBase + 12] = -mirroredJointHeads[boneIndex].x;
			inverseBindMatrices[matrixBase + 13] = -mirroredJointHeads[boneIndex].y;
			inverseBindMatrices[matrixBase + 14] = -mirroredJointHeads[boneIndex].z;

			if (bone.parentIndex_ < 0)
			{
				rootNode = static_cast<Int>(boneIndex);
			}
		}
		for (Uint32 boneIndex = 0; boneIndex < model.BoneCount(); boneIndex++)
		{
			Int32 parentIndex = model.Bone(boneIndex).parentIndex_;
			if (parentIndex >= 0)
			{
				outModel.nodes[parentIndex].children.push_back(static_cast<Int>(boneIndex));
			}
		}

		tinygltf::Skin& skin = outModel.skins.emplace_back();
		for (Uint32 boneIndex = 0; boneIndex < model.BoneCount(); boneIndex++)
		{
			skin.joints.push_back(static_cast<Int>(boneIndex));
		}
		skin.inverseBindMatrices = appendAccessor(inverseBindMatrices.data(), inverseBindMatrices.size() * sizeof(Float), TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_MAT4, model.BoneCount(), 0);
		if (rootNode >= 0)
		{
			skin.skeleton = rootNode;
		}

		Int meshNode = static_cast<Int>(outModel.nodes.size());
		tinygltf::Node& gltfMeshNode = outModel.nodes.emplace_back();
		gltfMeshNode.name = "SeedHuman";
		gltfMeshNode.mesh = 0;
		gltfMeshNode.skin = 0;

		tinygltf::Scene& scene = outModel.scenes.emplace_back();
		outModel.defaultScene = 0;
		if (rootNode >= 0)
		{
			scene.nodes.push_back(rootNode);
		}
		scene.nodes.push_back(meshNode);
	}

	Bool HumanCharacterConverter::Bake(const HumanCharacterModel& model, const HumanCharacterEvaluator& evaluator, Bool binary, String filePath)
	{
		tinygltf::Model gltfModel;
		Convert(model, evaluator, gltfModel);

		tinygltf::TinyGLTF writer;
		return writer.WriteGltfSceneToFile(&gltfModel, std::string(filePath.c_str()), binary, binary, !binary, binary);
	}
}
