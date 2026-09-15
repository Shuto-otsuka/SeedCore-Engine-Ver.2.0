#include <GraphicsEngine/Avatar/Animal/AnimalCharacterEvaluator.h>
#include <GraphicsEngine/Avatar/Animal/AnimalCharacterModel.h>

namespace SeedCore
{
	void AnimalCharacterEvaluator::SetModel(const AnimalCharacterModel& model)
	{
		model_ = &model;
		axisWeights_.assign(model.AxisCount(), 0.0f);
		for (Uint32 axisIndex = 0; axisIndex < model.AxisCount(); axisIndex++)
		{
			axisWeights_[axisIndex] = model.Axis(axisIndex).defaultValue_;
		}
	}

	void AnimalCharacterEvaluator::SetAxisWeight(Uint32 axisIndex, Float weight)
	{
		if (axisIndex >= axisWeights_.size())
		{
			return;
		}
		const AnimalCharacterAxis& axis = model_->Axis(axisIndex);
		axisWeights_[axisIndex] = std::clamp(weight, axis.minValue_, axis.maxValue_);
	}

	void AnimalCharacterEvaluator::ResetAxisWeights()
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

	Float AnimalCharacterEvaluator::AxisWeight(Uint32 axisIndex)const
	{
		return axisIndex < axisWeights_.size() ? axisWeights_[axisIndex] : 0.0f;
	}

	void AnimalCharacterEvaluator::Evaluate()
	{
		if (!model_ || !model_->IsLoaded())
		{
			return;
		}

		std::span<const Vector3> basePositions = model_->Positions();
		positions_.assign(basePositions.begin(), basePositions.end());

		Uint32 blendAxisCount = model_->BlendAxisCount();
		Float blendSum = 0.0f;
		Float featureSum = 0.0f;
		for (Uint32 axisIndex = 0; axisIndex < model_->AxisCount(); axisIndex++)
		{
			if (axisIndex < blendAxisCount)
			{
				blendSum += std::abs(axisWeights_[axisIndex]);
			}
			else
			{
				featureSum += std::abs(axisWeights_[axisIndex]);
			}
		}
		Float blendScale = blendSum > 1.0f ? 1.0f / blendSum : 1.0f;
		Float featureScale = featureSum > featureBudget_ ? featureBudget_ / featureSum : 1.0f;

		for (Uint32 axisIndex = 0; axisIndex < model_->AxisCount(); axisIndex++)
		{
			Float weight = axisWeights_[axisIndex] * (axisIndex < blendAxisCount ? blendScale : featureScale);
			if (weight == 0.0f)
			{
				continue;
			}
			const AnimalCharacterAxis& axis = model_->Axis(axisIndex);
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
				const AnimalCharacterBone& bone = model_->Bone(boneIndex);
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

	std::span<const Vector3> AnimalCharacterEvaluator::Positions()const
	{
		return positions_;
	}

	std::span<const Vector3> AnimalCharacterEvaluator::Normals()const
	{
		return normals_;
	}

	std::span<const Vector3> AnimalCharacterEvaluator::JointHeads()const
	{
		return jointHeads_;
	}
}
