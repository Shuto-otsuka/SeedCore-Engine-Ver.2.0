#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>

namespace SeedCore
{
	void HumanCharacterEvaluator::SetModel(const HumanCharacterModel& model)
	{
		model_ = &model;
		axisWeights_.assign(model.AxisCount(), 0.0f);
		for (Uint32 axisIndex = 0; axisIndex < model.AxisCount(); axisIndex++)
		{
			axisWeights_[axisIndex] = model.Axis(axisIndex).defaultValue_;
		}
	}

	void HumanCharacterEvaluator::SetAxisWeight(Uint32 axisIndex, Float weight)
	{
		if (axisIndex >= axisWeights_.size())
		{
			return;
		}
		const HumanCharacterAxis& axis = model_->Axis(axisIndex);
		axisWeights_[axisIndex] = std::clamp(weight, axis.minValue_, axis.maxValue_);
	}

	void HumanCharacterEvaluator::ResetAxisWeights()
	{
		if (!model_)
		{
			return;
		}
		for (Uint32 axisIndex = 0; axisIndex < model_->AxisCount(); axisIndex++)
		{
			axisWeights_[axisIndex] = model_->Axis(axisIndex).defaultValue_;
		}
	}

	Float HumanCharacterEvaluator::AxisWeight(Uint32 axisIndex)const
	{
		return axisIndex < axisWeights_.size() ? axisWeights_[axisIndex] : 0.0f;
	}

	void HumanCharacterEvaluator::Evaluate()
	{
		if (!model_ || !model_->IsLoaded())
		{
			return;
		}

		std::span<const Vector3> basePositions = model_->Positions();
		positions_.assign(basePositions.begin(), basePositions.end());

		for (Uint32 axisIndex = 0; axisIndex < model_->AxisCount(); axisIndex++)
		{
			Float weight = axisWeights_[axisIndex];
			if (weight == 0.0f)
			{
				continue;
			}
			const HumanCharacterAxis& axis = model_->Axis(axisIndex);
			if (weight > 0.0f)
			{
				for (Size entryIndex = 0; entryIndex < axis.vertexIndices_.size(); entryIndex++)
				{
					positions_[axis.vertexIndices_[entryIndex]] += axis.deltas_[entryIndex] * weight;
				}
			}
			else
			{
				for (Size entryIndex = 0; entryIndex < axis.negativeVertexIndices_.size(); entryIndex++)
				{
					positions_[axis.negativeVertexIndices_[entryIndex]] += axis.negativeDeltas_[entryIndex] * (-weight);
				}
			}
		}

		jointHeads_.assign(model_->BoneCount(), Vector3(0.0f, 0.0f, 0.0f));
		for (Uint32 boneIndex = 0; boneIndex < model_->BoneCount(); boneIndex++)
		{
			std::span<const Uint32> group = model_->JointGroup(boneIndex);
			if (group.empty())
			{
				const HumanCharacterBone& bone = model_->Bone(boneIndex);
				jointHeads_[boneIndex] = Vector3(bone.restHead_[0], bone.restHead_[1], bone.restHead_[2]);
				continue;
			}
			Vector3 sum(0.0f, 0.0f, 0.0f);
			for (Uint32 vertexIndex : group)
			{
				sum += positions_[vertexIndex];
			}
			jointHeads_[boneIndex] = sum / static_cast<Float>(group.size());
		}

		normals_.assign(positions_.size(), Vector3(0.0f, 0.0f, 0.0f));
		std::span<const Uint32> triangles = model_->Triangles();
		for (Size triangleStart = 0; triangleStart + 2 < triangles.size(); triangleStart += 3)
		{
			Uint32 index0 = triangles[triangleStart];
			Uint32 index1 = triangles[triangleStart + 1];
			Uint32 index2 = triangles[triangleStart + 2];
			Vector3 faceNormal = (positions_[index1] - positions_[index0]).Cross(positions_[index2] - positions_[index0]);
			normals_[index0] += faceNormal;
			normals_[index1] += faceNormal;
			normals_[index2] += faceNormal;
		}
		for (Vector3& normal : normals_)
		{
			normal.Normalize();
		}
	}

	std::span<const Vector3> HumanCharacterEvaluator::Positions()const
	{
		return positions_;
	}

	std::span<const Vector3> HumanCharacterEvaluator::Normals()const
	{
		return normals_;
	}

	std::span<const Vector3> HumanCharacterEvaluator::JointHeads()const
	{
		return jointHeads_;
	}
}
