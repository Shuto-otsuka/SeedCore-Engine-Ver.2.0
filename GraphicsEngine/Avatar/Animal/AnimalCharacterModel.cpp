#include <GraphicsEngine/Avatar/Animal/AnimalCharacterModel.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	namespace
	{
		const Char* const animalCharacterGroupNames[] =
		{
			"犬系", "有蹄系", "ネコ系",
		};

		const Char* const animalCharacterRegionNames[] =
		{
			"body", "face", "belly", "legs",
		};

		const Char* const animalCharacterRegionLabels[] =
		{
			"体", "顔", "腹", "脚",
		};

		const Char* const animalCharacterFeatureLabels[] =
		{
			"鼻の長さ", "耳の大きさ", "首の長さ", "脚の長さ", "しっぽの長さ", "ずんぐり",
		};

		const Char* const animalCharacterCanidLabels[] =
		{
			"キツネ / オオカミ",
			"鼻の長さ", "耳の大きさ", "首の長さ", "脚の長さ", "しっぽの長さ", "ずんぐり",
		};

		const Char* const animalCharacterUngulateLabels[] =
		{
			"シカ / ウマ",
			"鼻の長さ", "耳の大きさ", "首の長さ", "脚の長さ", "しっぽの長さ", "ずんぐり",
		};

		const Char* const animalCharacterBoneNames[] =
		{
			"root", "Body", "Back", "BackShoulder.L", "BackLeg.L",
			"BackShoulder.R", "BackLeg.R", "BackUpperLeg.L", "BackLowerLeg.L", "BackUpperLeg.R",
			"BackLowerLeg.R", "Torso", "Torso2", "Torso3", "Neck1",
			"Neck2", "Neck3", "Ear1.L", "Ear1.R", "Ear2.L",
			"Ear2.R", "Ear3.L", "Ear3.R", "Ear4.L", "Ear4.R",
			"FrontShoulder.L", "FrontUpperLeg.L", "FrontLowerLeg.L", "FrontShoulder.R", "FrontUpperLeg.R",
			"FrontLowerLeg.R", "Head", "Tail1", "Tail2", "Tail3",
			"Tail4", "Tail5", "Tail6", "Tail7", "Tail8",
		};
	}

	Bool AnimalCharacterModel::Load(String filePath)
	{
		std::ifstream stream(filePath.c_str(), std::ios::binary);
		if (!stream.is_open())
		{
			return false;
		}
		stream.seekg(0, std::ios::end);
		Size fileSize = static_cast<Size>(stream.tellg());
		stream.seekg(0, std::ios::beg);
		if (fileSize < 16)
		{
			return false;
		}

		DynamicArray<Byte> rawData(fileSize);
		if (!stream.read(rawData.data(), fileSize))
		{
			return false;
		}

		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));
		DynamicArray<Byte> iv(rawData.begin(), rawData.begin() + 16);
		DynamicArray<Byte> ciphertext(rawData.begin() + 16, rawData.end());
		blob_ = Aes256::Decrypt(key, iv, ciphertext);
		if (blob_.size() < sizeof(Uint32) * 4)
		{
			blob_.clear();
			return false;
		}

		const Uint8* base = reinterpret_cast<const Uint8*>(blob_.data());
		const Uint32* header = reinterpret_cast<const Uint32*>(base);
		if (header[0] != animalCharacterMagic || header[1] != animalCharacterVersion)
		{
			blob_.clear();
			return false;
		}

		Uint32 groupCount = header[2];
		groups_.clear();
		groups_.resize(groupCount);
		for (Uint32 groupIndex = 0; groupIndex < groupCount; groupIndex++)
		{
			const Uint32* record = header + 4 + groupIndex * 16;
			GroupHeader& group = groups_[groupIndex];
			group.vertexCount_ = record[0];
			group.triangleCount_ = record[1];
			group.axisCount_ = record[2];
			group.boneCount_ = record[3];
			group.blendAxisCount_ = record[4];
			for (Uint32 offsetIndex = 0; offsetIndex < 11; offsetIndex++)
			{
				group.blockOffsets_[offsetIndex] = record[5 + offsetIndex];
			}
		}

		activeGroup_ = 0;
		SetGroup(0);
		return true;
	}

	void AnimalCharacterModel::SetGroup(Uint32 groupIndex)
	{
		if (groups_.empty() || groupIndex >= groups_.size())
		{
			return;
		}
		activeGroup_ = groupIndex;

		const Uint8* base = reinterpret_cast<const Uint8*>(blob_.data());
		const GroupHeader& group = groups_[groupIndex];
		const Uint32* blockOffsets = group.blockOffsets_;

		vertexCount_ = group.vertexCount_;
		triangleCount_ = group.triangleCount_;
		boneCount_ = group.boneCount_;
		blendAxisCount_ = group.blendAxisCount_;
		Uint32 axisCount = group.axisCount_;

		positions_ = std::span<const Vector3>(reinterpret_cast<const Vector3*>(base + blockOffsets[0]), vertexCount_);
		normals_ = std::span<const Vector3>(reinterpret_cast<const Vector3*>(base + blockOffsets[1]), vertexCount_);
		texcoords_ = std::span<const Vector2>(reinterpret_cast<const Vector2*>(base + blockOffsets[2]), vertexCount_);
		triangles_ = std::span<const Uint32>(reinterpret_cast<const Uint32*>(base + blockOffsets[3]), static_cast<Size>(triangleCount_) * 3);
		regions_ = std::span<const Uint8>(base + blockOffsets[4], vertexCount_);
		skinWeights_ = std::span<const Float>(reinterpret_cast<const Float*>(base + blockOffsets[6]), static_cast<Size>(vertexCount_) * 4);
		skinIndices_ = std::span<const Uint32>(reinterpret_cast<const Uint32*>(base + blockOffsets[7]), static_cast<Size>(vertexCount_) * 4);
		bones_ = std::span<const AnimalCharacterBone>(reinterpret_cast<const AnimalCharacterBone*>(base + blockOffsets[8]), boneCount_);

		bodyVertexCount_ = 0;
		for (Uint32 index : triangles_)
		{
			if (index + 1 > bodyVertexCount_)
			{
				bodyVertexCount_ = index + 1;
			}
		}

		Size cursor = blockOffsets[5];
		axes_.clear();
		axes_.resize(axisCount);
		for (Uint32 axisIndex = 0; axisIndex < axisCount; axisIndex++)
		{
			const Float* meta = reinterpret_cast<const Float*>(base + cursor + axisIndex * 3 * sizeof(Float));
			axes_[axisIndex].defaultValue_ = meta[0];
			axes_[axisIndex].minValue_ = meta[1];
			axes_[axisIndex].maxValue_ = meta[2];
		}
		cursor += static_cast<Size>(axisCount) * 3 * sizeof(Float);
		for (Uint32 axisIndex = 0; axisIndex < axisCount; axisIndex++)
		{
			Uint32 positiveCount = *reinterpret_cast<const Uint32*>(base + cursor);
			cursor += sizeof(Uint32);
			const Uint32* positiveIndices = reinterpret_cast<const Uint32*>(base + cursor);
			cursor += static_cast<Size>(positiveCount) * sizeof(Uint32);
			const Vector3* positiveDeltas = reinterpret_cast<const Vector3*>(base + cursor);
			cursor += static_cast<Size>(positiveCount) * sizeof(Vector3);

			Uint32 negativeCount = *reinterpret_cast<const Uint32*>(base + cursor);
			cursor += sizeof(Uint32);
			const Uint32* negativeIndices = reinterpret_cast<const Uint32*>(base + cursor);
			cursor += static_cast<Size>(negativeCount) * sizeof(Uint32);
			const Vector3* negativeDeltas = reinterpret_cast<const Vector3*>(base + cursor);
			cursor += static_cast<Size>(negativeCount) * sizeof(Vector3);

			axes_[axisIndex].vertexIndices_.assign(positiveIndices, positiveIndices + positiveCount);
			axes_[axisIndex].deltas_.assign(positiveDeltas, positiveDeltas + positiveCount);
			axes_[axisIndex].negativeVertexIndices_.assign(negativeIndices, negativeIndices + negativeCount);
			axes_[axisIndex].negativeDeltas_.assign(negativeDeltas, negativeDeltas + negativeCount);
		}

		cursor = blockOffsets[9];
		jointGroups_.clear();
		jointGroups_.resize(boneCount_);
		for (Uint32 boneIndex = 0; boneIndex < boneCount_; boneIndex++)
		{
			Uint32 count = *reinterpret_cast<const Uint32*>(base + cursor);
			cursor += sizeof(Uint32);
			const Uint32* indices = reinterpret_cast<const Uint32*>(base + cursor);
			cursor += static_cast<Size>(count) * sizeof(Uint32);
			jointGroups_[boneIndex].assign(indices, indices + count);
		}

		const Uint32* regionData = reinterpret_cast<const Uint32*>(base + blockOffsets[10]);
		for (Uint32 regionIndex = 0; regionIndex < animalCharacterRegionCount; regionIndex++)
		{
			regionRanges_[regionIndex].firstTriangle_ = regionData[regionIndex * 2 + 0];
			regionRanges_[regionIndex].triangleCount_ = regionData[regionIndex * 2 + 1];
		}
	}

	const AnimalCharacterRegionRange& AnimalCharacterModel::RegionTriangleRange(Uint32 regionIndex)const
	{
		return regionRanges_[regionIndex < animalCharacterRegionCount ? regionIndex : 0];
	}

	Uint32 AnimalCharacterModel::GroupCount()const
	{
		return static_cast<Uint32>(groups_.size());
	}

	Uint32 AnimalCharacterModel::ActiveGroup()const
	{
		return activeGroup_;
	}

	Uint32 AnimalCharacterModel::BlendAxisCount()const
	{
		return blendAxisCount_;
	}

	Bool AnimalCharacterModel::IsLoaded()const
	{
		return !blob_.empty();
	}

	Uint32 AnimalCharacterModel::VertexCount()const
	{
		return vertexCount_;
	}

	Uint32 AnimalCharacterModel::BodyVertexCount()const
	{
		return bodyVertexCount_;
	}

	Uint32 AnimalCharacterModel::TriangleCount()const
	{
		return triangleCount_;
	}

	Uint32 AnimalCharacterModel::AxisCount()const
	{
		return static_cast<Uint32>(axes_.size());
	}

	Uint32 AnimalCharacterModel::BoneCount()const
	{
		return boneCount_;
	}

	std::span<const Vector3> AnimalCharacterModel::Positions()const
	{
		return positions_;
	}

	std::span<const Vector3> AnimalCharacterModel::Normals()const
	{
		return normals_;
	}

	std::span<const Vector2> AnimalCharacterModel::Texcoords()const
	{
		return texcoords_;
	}

	std::span<const Uint32> AnimalCharacterModel::Triangles()const
	{
		return triangles_;
	}

	std::span<const Uint8> AnimalCharacterModel::Regions()const
	{
		return regions_;
	}

	std::span<const Float> AnimalCharacterModel::SkinWeights()const
	{
		return skinWeights_;
	}

	std::span<const Uint32> AnimalCharacterModel::SkinIndices()const
	{
		return skinIndices_;
	}

	const AnimalCharacterAxis& AnimalCharacterModel::Axis(Uint32 index)const
	{
		return axes_[index];
	}

	const AnimalCharacterBone& AnimalCharacterModel::Bone(Uint32 index)const
	{
		return bones_[index];
	}

	std::span<const Uint32> AnimalCharacterModel::JointGroup(Uint32 boneIndex)const
	{
		return jointGroups_[boneIndex];
	}

	std::span<const Char* const> AnimalCharacterModel::GroupNames()
	{
		return std::span<const Char* const>(animalCharacterGroupNames, std::size(animalCharacterGroupNames));
	}

	std::span<const Char* const> AnimalCharacterModel::RegionNames()
	{
		return std::span<const Char* const>(animalCharacterRegionNames, std::size(animalCharacterRegionNames));
	}

	std::span<const Char* const> AnimalCharacterModel::RegionLabels()
	{
		return std::span<const Char* const>(animalCharacterRegionLabels, std::size(animalCharacterRegionLabels));
	}

	std::span<const Char* const> AnimalCharacterModel::AxisLabels(Uint32 groupIndex)
	{
		if (groupIndex == 0)
		{
			return std::span<const Char* const>(animalCharacterCanidLabels, std::size(animalCharacterCanidLabels));
		}
		if (groupIndex == 1)
		{
			return std::span<const Char* const>(animalCharacterUngulateLabels, std::size(animalCharacterUngulateLabels));
		}
		return std::span<const Char* const>(animalCharacterFeatureLabels, std::size(animalCharacterFeatureLabels));
	}

	std::span<const Char* const> AnimalCharacterModel::BoneNames()
	{
		return std::span<const Char* const>(animalCharacterBoneNames, std::size(animalCharacterBoneNames));
	}
}
