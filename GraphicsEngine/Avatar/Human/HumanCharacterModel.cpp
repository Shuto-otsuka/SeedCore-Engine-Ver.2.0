#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	namespace
	{
		const Char* const humanCharacterAxisNames[] =
		{
			"Gender", "Age", "Muscle", "Weight",
			"Height", "Measure_bust_circ", "Measure_underbust_circ", "Measure_waist_circ",
			"Measure_hips_circ", "Measure_neck_circ", "Measure_neck_height", "Measure_shoulder_dist",
			"Measure_upperarm_circ", "Measure_upperarm_length", "Measure_lowerarm_length", "Measure_frontchest_dist",
			"Measure_napetowaist_dist", "Measure_waisttohip_dist", "Measure_thigh_circ", "Measure_calf_circ",
			"Measure_knee_circ", "Measure_ankle_circ", "Measure_wrist_circ", "Measure_upperleg_height",
			"Measure_lowerleg_height", "Head_ScaleWide", "Head_ScaleTall", "Head_ScaleDeep",
			"Head_Age", "Head_Fat", "Nose_Size", "Mouth_Wide",
			"Chin_Prominent",
		};

		const Char* const humanCharacterAxisLabels[] =
		{
			"性別", "年齢", "筋肉", "体重",
			"身長", "バスト周囲", "アンダーバスト周囲", "ウエスト周囲",
			"ヒップ周囲", "首周囲", "首の高さ", "肩幅",
			"上腕周囲", "上腕の長さ", "前腕の長さ", "胸の厚み",
			"背面丈（首〜腰）", "腰〜ヒップ丈", "太もも周囲", "ふくらはぎ周囲",
			"膝周囲", "足首周囲", "手首周囲", "大腿長",
			"下腿長", "頭の幅", "頭の高さ", "頭の奥行き",
			"顔の年齢", "顔の脂肪", "鼻の大きさ", "口の広さ",
			"顎の突出",
		};

		const Char* const humanCharacterBoneNames[] =
		{
			"root", "spine05", "spine04", "spine03",
			"spine02", "breast.L", "breast.R", "spine01",
			"clavicle.L", "clavicle.R", "neck01", "neck02",
			"neck03", "head", "special06.L", "special05.L",
			"eye.L", "special06.R", "special05.R", "eye.R",
			"shoulder01.L", "upperarm01.L", "upperarm02.L", "lowerarm01.L",
			"lowerarm02.L", "wrist.L", "finger1-1.L", "shoulder01.R",
			"upperarm01.R", "upperarm02.R", "lowerarm01.R", "lowerarm02.R",
			"wrist.R", "finger1-1.R", "finger1-2.L", "finger1-2.R",
			"finger1-3.L", "finger1-3.R", "metacarpal1.L", "finger2-1.L",
			"metacarpal1.R", "finger2-1.R", "finger2-2.L", "finger2-2.R",
			"finger2-3.L", "finger2-3.R", "metacarpal2.L", "finger3-1.L",
			"metacarpal2.R", "finger3-1.R", "finger3-2.L", "finger3-2.R",
			"finger3-3.L", "finger3-3.R", "metacarpal3.L", "finger4-1.L",
			"metacarpal3.R", "finger4-1.R", "finger4-2.L", "finger4-2.R",
			"finger4-3.L", "finger4-3.R", "metacarpal4.L", "finger5-1.L",
			"metacarpal4.R", "finger5-1.R", "finger5-2.L", "finger5-2.R",
			"finger5-3.L", "finger5-3.R", "pelvis.L", "upperleg01.L",
			"upperleg02.L", "lowerleg01.L", "lowerleg02.L", "foot.L",
			"pelvis.R", "upperleg01.R", "upperleg02.R", "lowerleg01.R",
			"lowerleg02.R", "foot.R", "jaw", "levator02.L",
			"levator02.R", "levator03.L", "levator03.R", "levator04.L",
			"levator04.R", "levator05.L", "levator05.R", "special03",
			"levator06.L", "levator06.R", "temporalis01.L", "oculi02.L",
			"oculi01.L", "temporalis01.R", "oculi02.R", "oculi01.R",
			"orbicularis03.L", "orbicularis03.R", "orbicularis04.L", "orbicularis04.R",
			"special04", "oris02", "oris01", "special01",
			"oris04.L", "oris03.L", "oris04.R", "oris03.R",
			"oris06", "oris05", "oris06.L", "oris06.R",
			"oris07.L", "oris07.R", "temporalis02.L", "risorius02.L",
			"temporalis02.R", "risorius02.R", "risorius03.L", "risorius03.R",
			"toe1-1.L", "toe1-1.R", "toe1-2.L", "toe1-2.R",
			"toe2-1.L", "toe2-1.R", "toe2-2.L", "toe2-2.R",
			"toe2-3.L", "toe2-3.R", "toe3-1.L", "toe3-1.R",
			"toe3-2.L", "toe3-2.R", "toe3-3.L", "toe3-3.R",
			"toe4-1.L", "toe4-1.R", "toe4-2.L", "toe4-2.R",
			"toe4-3.L", "toe4-3.R", "toe5-1.L", "toe5-1.R",
			"toe5-2.L", "toe5-2.R", "toe5-3.L", "toe5-3.R",
			"tongue00", "tongue01", "tongue02", "tongue03",
			"tongue04", "tongue05.L", "tongue05.R", "tongue06.L",
			"tongue06.R", "tongue07.L", "tongue07.R",
		};
	}

	Bool HumanCharacterModel::Load(String filePath)
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
		if (blob_.size() < sizeof(Uint32) * 17)
		{
			blob_.clear();
			return false;
		}

		const Uint8* base = reinterpret_cast<const Uint8*>(blob_.data());
		const Uint32* header = reinterpret_cast<const Uint32*>(base);
		if (header[0] != humanCharacterMagic || header[1] != humanCharacterVersion)
		{
			blob_.clear();
			return false;
		}

		vertexCount_ = header[2];
		triangleCount_ = header[3];
		Uint32 axisCount = header[4];
		boneCount_ = header[5];
		const Uint32* blockOffsets = header + 6;

		positions_ = std::span<const Vector3>(reinterpret_cast<const Vector3*>(base + blockOffsets[0]), vertexCount_);
		normals_ = std::span<const Vector3>(reinterpret_cast<const Vector3*>(base + blockOffsets[1]), vertexCount_);
		texcoords_ = std::span<const Vector2>(reinterpret_cast<const Vector2*>(base + blockOffsets[2]), vertexCount_);
		triangles_ = std::span<const Uint32>(reinterpret_cast<const Uint32*>(base + blockOffsets[3]), static_cast<Size>(triangleCount_) * 3);
		regions_ = std::span<const Uint8>(base + blockOffsets[4], vertexCount_);
		skinWeights_ = std::span<const Float>(reinterpret_cast<const Float*>(base + blockOffsets[6]), static_cast<Size>(vertexCount_) * 4);
		skinIndices_ = std::span<const Uint32>(reinterpret_cast<const Uint32*>(base + blockOffsets[7]), static_cast<Size>(vertexCount_) * 4);
		bones_ = std::span<const HumanCharacterBone>(reinterpret_cast<const HumanCharacterBone*>(base + blockOffsets[8]), boneCount_);

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

		return true;
	}

	Bool HumanCharacterModel::IsLoaded()const
	{
		return !blob_.empty();
	}

	Uint32 HumanCharacterModel::VertexCount()const
	{
		return vertexCount_;
	}

	Uint32 HumanCharacterModel::BodyVertexCount()const
	{
		return bodyVertexCount_;
	}

	Uint32 HumanCharacterModel::TriangleCount()const
	{
		return triangleCount_;
	}

	Uint32 HumanCharacterModel::AxisCount()const
	{
		return static_cast<Uint32>(axes_.size());
	}

	Uint32 HumanCharacterModel::BoneCount()const
	{
		return boneCount_;
	}

	std::span<const Vector3> HumanCharacterModel::Positions()const
	{
		return positions_;
	}

	std::span<const Vector3> HumanCharacterModel::Normals()const
	{
		return normals_;
	}

	std::span<const Vector2> HumanCharacterModel::Texcoords()const
	{
		return texcoords_;
	}

	std::span<const Uint32> HumanCharacterModel::Triangles()const
	{
		return triangles_;
	}

	std::span<const Uint8> HumanCharacterModel::Regions()const
	{
		return regions_;
	}

	std::span<const Float> HumanCharacterModel::SkinWeights()const
	{
		return skinWeights_;
	}

	std::span<const Uint32> HumanCharacterModel::SkinIndices()const
	{
		return skinIndices_;
	}

	const HumanCharacterAxis& HumanCharacterModel::Axis(Uint32 index)const
	{
		return axes_[index];
	}

	const HumanCharacterBone& HumanCharacterModel::Bone(Uint32 index)const
	{
		return bones_[index];
	}

	std::span<const Uint32> HumanCharacterModel::JointGroup(Uint32 boneIndex)const
	{
		return jointGroups_[boneIndex];
	}

	std::span<const Char* const> HumanCharacterModel::AxisNames()
	{
		return std::span<const Char* const>(humanCharacterAxisNames, std::size(humanCharacterAxisNames));
	}

	std::span<const Char* const> HumanCharacterModel::AxisLabels()
	{
		return std::span<const Char* const>(humanCharacterAxisLabels, std::size(humanCharacterAxisLabels));
	}

	std::span<const Char* const> HumanCharacterModel::BoneNames()
	{
		return std::span<const Char* const>(humanCharacterBoneNames, std::size(humanCharacterBoneNames));
	}
}
