#include <GraphicsEngine/Model/IK/FullBodyIK.h>
#include <GraphicsEngine/Model/IK/IKPose.h>
#include <GraphicsEngine/Model/IK/TwoBoneIK.h>
#include <GraphicsEngine/Model/IK/FABRIK.h>
#include <GraphicsEngine/Model/IK/TreeFABRIK.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Animation/Animator.h>

namespace SeedCore
{
	void FullBodyIK::Apply(const Crister& crister, const Animator& animator, DynamicArray<Matrix>& poseGlobalTransforms)
	{
		const std::unordered_map<std::string, JointConstraint>& jointConstraints = animator.GetJointConstraint();

		DynamicArray<std::pair<Int, const IKTarget*>> activeTargets;
		for (const auto& [effectorBoneName, target] : animator.GetIKTarget())
		{
			if (target.weight_ <= 0.0f)
			{
				continue;
			}

			Int endNodeIndex = crister.FindNodeIndex(effectorBoneName);
			if (endNodeIndex < 0)
			{
				continue;
			}

			activeTargets.push_back({ endNodeIndex, &target });
		}

		if (activeTargets.size() == 1)
		{
			SolveSingle(crister, activeTargets[0].first, *activeTargets[0].second, jointConstraints, poseGlobalTransforms);
		}
		else if (activeTargets.size() > 1)
		{
			SolveGroup(crister, activeTargets, jointConstraints, poseGlobalTransforms);
		}
	}

	void FullBodyIK::SolveSingle(const Crister& crister, Int endNodeIndex, const IKTarget& target, const std::unordered_map<std::string, JointConstraint>& jointConstraints, DynamicArray<Matrix>& poseGlobalTransforms)
	{
		constexpr Float fabrikTolerance = 0.001f;
		constexpr Int fabrikMaxIterations = 10;

		Int rootNodeIndex = IKPose::ResolveChainRoot(crister, endNodeIndex);
		if (rootNodeIndex < 0)
		{
			return;
		}

		DynamicArray<Int> chain = IKPose::ResolveChain(crister, rootNodeIndex, endNodeIndex);
		if (chain.size() < 3)
		{
			return;
		}

		DynamicArray<Vector3> originalPositions;
		DynamicArray<Quaternion> originalRotations;
		originalPositions.reserve(chain.size());
		originalRotations.reserve(chain.size());
		for (Int nodeIndex : chain)
		{
			Vector3 position;
			Quaternion rotation;
			IKPose::ExtractWorldTransform(poseGlobalTransforms[nodeIndex], position, rotation);
			originalPositions.push_back(position);
			originalRotations.push_back(rotation);
		}

		Float maxReach = 0.0f;
		for (Size index = 0; index + 1 < originalPositions.size(); index++)
		{
			maxReach += Vector3::Distance(originalPositions[index], originalPositions[index + 1]);
		}

		DynamicArray<Quaternion> pristineRotations = originalRotations;

		Bool hasLean = false;
		Int leanNodeIndex = -1;
		Quaternion leanParentWorldRotation = Quaternion::Identity;

		Int limbParentIndex = crister.Nodes()[rootNodeIndex].parentIndex_;
		if (limbParentIndex >= 0 && static_cast<Size>(limbParentIndex) < poseGlobalTransforms.size())
		{
			constexpr Float maxLeanAngle = DirectX::XM_PI / 6.0f;
			constexpr Float leanTransitionDistance = 0.1f;

			Float reachDeficit = Vector3::Distance(originalPositions[0], target.targetPosition_) - maxReach;
			Float leanBlend = Clamp(reachDeficit / leanTransitionDistance, 0.0f, 1.0f);

			if (leanBlend > 0.0f)
			{
				Vector3 leanPosition;
				Quaternion originalLeanWorldRotation;
				IKPose::ExtractWorldTransform(poseGlobalTransforms[limbParentIndex], leanPosition, originalLeanWorldRotation);

				Vector3 leanToRoot = originalPositions[0] - leanPosition;
				Vector3 leanToTarget = target.targetPosition_ - leanPosition;

				if (leanToRoot.LengthSquared() > 1e-8f && leanToTarget.LengthSquared() > 1e-8f)
				{
					leanToRoot.Normalize();
					leanToTarget.Normalize();

					Float leanAngle = Min(Acos(Clamp(leanToRoot.Dot(leanToTarget), -1.0f, 1.0f)), maxLeanAngle) * target.weight_ * leanBlend;

					Vector3 leanAxis = leanToRoot.Cross(leanToTarget);
					if (leanAngle > 0.0f && leanAxis.LengthSquared() > 1e-8f)
					{
						leanAxis.Normalize();
						Quaternion leanDelta = Quaternion::CreateFromAxisAngle(leanAxis, leanAngle);

						hasLean = true;
						leanNodeIndex = limbParentIndex;
						leanParentWorldRotation = originalLeanWorldRotation * leanDelta;

						for (Vector3& position : originalPositions)
						{
							position = leanPosition + Vector3::Transform(position - leanPosition, leanDelta);
						}
						for (Quaternion& rotation : originalRotations)
						{
							rotation = rotation * leanDelta;
						}
					}
				}
			}
		}

		DynamicArray<Quaternion> solvedWorldRotations = originalRotations;

		if (chain.size() == 3)
		{
			Vector3 poleVector;
			if (target.hasPoleVector_)
			{
				poleVector = target.poleVector_;
			}
			else
			{
				/// [EN] No pole was supplied, so keep the limb's current bend direction: project the mid joint's offset from the root-end line, then place the pole one bend-offset ahead of the mid joint along that same direction.
				/// [JP] ポール未指定の場合はチェーンの現在の曲がり方向を維持する: mid ジョイントの root-end 直線に対するはみ出し方向を求め、その方向へ mid ジョイントからさらに1つ分オフセットした位置をポールとする。
				Vector3 rootToEndAxis = originalPositions[2] - originalPositions[0];
				Vector3 rootToMidOffset = originalPositions[1] - originalPositions[0];
				Float rootToEndLengthSquared = rootToEndAxis.LengthSquared();
				Vector3 bendOffset = rootToEndLengthSquared > 1e-8f ? rootToMidOffset - rootToEndAxis * (rootToMidOffset.Dot(rootToEndAxis) / rootToEndLengthSquared) : rootToMidOffset;
				poleVector = originalPositions[1] + bendOffset;
			}

			Quaternion solvedRootWorldRotation, solvedMidWorldRotation;
			TwoBoneIK::Solve(originalPositions[0], originalPositions[1], originalPositions[2], target.targetPosition_, poleVector, originalRotations[0], originalRotations[1], solvedRootWorldRotation, solvedMidWorldRotation);
			solvedWorldRotations[0] = solvedRootWorldRotation;
			solvedWorldRotations[1] = solvedMidWorldRotation;
		}
		else
		{
			DynamicArray<Vector3> solvedPositions = originalPositions;
			FABRIK::Solve(solvedPositions, target.targetPosition_, fabrikTolerance, fabrikMaxIterations);

			for (Size jointIndex = 0; jointIndex + 1 < chain.size(); jointIndex++)
			{
				Vector3 oldDirection = originalPositions[jointIndex + 1] - originalPositions[jointIndex];
				Vector3 newDirection = solvedPositions[jointIndex + 1] - solvedPositions[jointIndex];
				Quaternion swing = IKPose::SwingRotation(oldDirection, newDirection);
				solvedWorldRotations[jointIndex] = solvedWorldRotations[jointIndex] * swing;
			}
		}

		std::unordered_map<Int, Quaternion> localRotationOverrides;

		for (Size jointIndex = 0; jointIndex + 1 < chain.size(); jointIndex++)
		{
			Int nodeIndex = chain[jointIndex];

			Quaternion originalParentWorldRotation = Quaternion::Identity;
			if (jointIndex == 0)
			{
				Int parentIndex = crister.Nodes()[nodeIndex].parentIndex_;
				if (parentIndex >= 0 && static_cast<Size>(parentIndex) < poseGlobalTransforms.size())
				{
					Vector3 parentPosition;
					IKPose::ExtractWorldTransform(poseGlobalTransforms[parentIndex], parentPosition, originalParentWorldRotation);
				}
			}
			else
			{
				originalParentWorldRotation = pristineRotations[jointIndex - 1];
			}

			Quaternion solvedParentWorldRotation = jointIndex == 0 ? (hasLean ? leanParentWorldRotation : originalParentWorldRotation) : solvedWorldRotations[jointIndex - 1];

			auto constraintIt = jointConstraints.find(crister.Nodes()[nodeIndex].name_);
			if (constraintIt != jointConstraints.end() && constraintIt->second.axis_.LengthSquared() > 1e-12f)
			{
				const JointConstraint& constraint = constraintIt->second;

				Vector3 originalDirection = originalPositions[jointIndex + 1] - originalPositions[jointIndex];

				Quaternion originalInverseRotation;
				originalRotations[jointIndex].Inverse(originalInverseRotation);
				Vector3 localDirection = Vector3::Transform(originalDirection, originalInverseRotation);
				Vector3 newDirection = Vector3::Transform(localDirection, solvedWorldRotations[jointIndex]);

				Vector3 worldAxis = Vector3::Transform(constraint.axis_, solvedParentWorldRotation);
				worldAxis.Normalize();
				Vector3 worldSwingAxis = constraint.swingAxis_.LengthSquared() > 1e-12f ? Vector3::Transform(constraint.swingAxis_, solvedParentWorldRotation) : Vector3::Right;
				worldSwingAxis.Normalize();

				Vector3 clampedDirection = IKPose::SwingLimit(newDirection, worldAxis, worldSwingAxis, ToRadians(constraint.swingAngle1_), ToRadians(constraint.swingAngle2_));

				Quaternion correction = IKPose::SwingRotation(newDirection, clampedDirection);
				solvedWorldRotations[jointIndex] = solvedWorldRotations[jointIndex] * correction;
			}

			Quaternion solvedParentInverseRotation;
			solvedParentWorldRotation.Inverse(solvedParentInverseRotation);

			Quaternion localRotation = solvedWorldRotations[jointIndex] * solvedParentInverseRotation;

			if (target.weight_ < 1.0f)
			{
				Quaternion originalParentInverseRotation;
				originalParentWorldRotation.Inverse(originalParentInverseRotation);
				Quaternion originalLocalRotation = pristineRotations[jointIndex] * originalParentInverseRotation;
				localRotation = Quaternion::Slerp(originalLocalRotation, localRotation, target.weight_);
			}

			solvedWorldRotations[jointIndex] = localRotation * solvedParentWorldRotation;
			localRotationOverrides[nodeIndex] = localRotation;
		}

		if (hasLean)
		{
			Quaternion leanGrandparentWorldRotation = Quaternion::Identity;
			Int leanGrandparentIndex = crister.Nodes()[leanNodeIndex].parentIndex_;
			if (leanGrandparentIndex >= 0 && static_cast<Size>(leanGrandparentIndex) < poseGlobalTransforms.size())
			{
				Vector3 leanGrandparentPosition;
				IKPose::ExtractWorldTransform(poseGlobalTransforms[leanGrandparentIndex], leanGrandparentPosition, leanGrandparentWorldRotation);
			}

			Quaternion leanGrandparentInverseRotation;
			leanGrandparentWorldRotation.Inverse(leanGrandparentInverseRotation);
			localRotationOverrides[leanNodeIndex] = leanParentWorldRotation * leanGrandparentInverseRotation;
		}

		if (target.hasRotation_)
		{
			Int effectorNodeIndex = chain.back();
			Quaternion solvedParentWorldRotation = solvedWorldRotations[chain.size() - 2];
			Quaternion solvedParentInverseRotation;
			solvedParentWorldRotation.Inverse(solvedParentInverseRotation);

			Quaternion localRotation = target.targetRotation_ * solvedParentInverseRotation;

			if (target.weight_ < 1.0f)
			{
				Quaternion originalParentInverseRotation;
				pristineRotations[chain.size() - 2].Inverse(originalParentInverseRotation);
				Quaternion originalLocalRotation = pristineRotations.back() * originalParentInverseRotation;
				localRotation = Quaternion::Slerp(originalLocalRotation, localRotation, target.weight_);
			}

			localRotationOverrides[effectorNodeIndex] = localRotation;
		}

		if (hasLean)
		{
			chain.insert(chain.begin(), leanNodeIndex);
		}

		IKPose::ApplyChainPose(crister, chain, localRotationOverrides, poseGlobalTransforms);
	}

	void FullBodyIK::SolveGroup(const Crister& crister, const DynamicArray<std::pair<Int, const IKTarget*>>& groupTargets, const std::unordered_map<std::string, JointConstraint>& jointConstraints, DynamicArray<Matrix>& poseGlobalTransforms)
	{
		constexpr Float fabrikTolerance = 0.001f;
		constexpr Int fabrikMaxIterations = 10;

		DynamicArray<Int> firstAncestors;
		{
			Int current = groupTargets[0].first;
			while (current >= 0)
			{
				firstAncestors.push_back(current);
				current = crister.Nodes()[current].parentIndex_;
			}
			std::ranges::reverse(firstAncestors);
		}

		DynamicArray<std::unordered_set<Int>> otherAncestorSets;
		for (Size targetIndex = 1; targetIndex < groupTargets.size(); targetIndex++)
		{
			std::unordered_set<Int> ancestorSet;
			Int current = groupTargets[targetIndex].first;
			while (current >= 0)
			{
				ancestorSet.insert(current);
				current = crister.Nodes()[current].parentIndex_;
			}
			otherAncestorSets.push_back(std::move(ancestorSet));
		}

		Int groupRootNodeIndex = -1;
		for (Int candidate : firstAncestors)
		{
			Bool allContain = std::ranges::all_of(otherAncestorSets, [candidate](const auto& ancestorSet) { return ancestorSet.contains(candidate); });

			if (!allContain)
			{
				break;
			}

			groupRootNodeIndex = candidate;
		}

		if (groupRootNodeIndex < 0)
		{
			return;
		}

		std::unordered_set<Int> unionNodes;
		std::unordered_map<Int, Vector3> originalPositions;
		std::unordered_map<Int, Quaternion> originalRotations;
		std::unordered_map<Int, Float> downstreamWeight;
		std::unordered_map<Int, Vector3> targetPositions;

		for (const auto& [endNodeIndex, target] : groupTargets)
		{
			targetPositions[endNodeIndex] = target->targetPosition_;

			Int current = endNodeIndex;
			while (true)
			{
				if (unionNodes.insert(current).second)
				{
					Vector3 position;
					Quaternion rotation;
					IKPose::ExtractWorldTransform(poseGlobalTransforms[current], position, rotation);
					originalPositions[current] = position;
					originalRotations[current] = rotation;
				}

				downstreamWeight[current] += target->weight_;

				if (current == groupRootNodeIndex)
				{
					break;
				}

				current = crister.Nodes()[current].parentIndex_;
			}
		}

		DynamicArray<TreeFABRIK::Target> treeTargets;
		treeTargets.reserve(groupTargets.size());
		for (const auto& [endNodeIndex, target] : groupTargets)
		{
			TreeFABRIK::Target treeTarget;
			treeTarget.nodeIndex_ = endNodeIndex;
			treeTarget.targetPosition_ = target->targetPosition_;
			treeTarget.weight_ = target->weight_;
			treeTargets.push_back(treeTarget);
		}

		std::unordered_map<Int, Vector3> solvedPositions = originalPositions;
		TreeFABRIK::Solve(crister, groupRootNodeIndex, treeTargets, solvedPositions, fabrikTolerance, fabrikMaxIterations);

		DynamicArray<Int> processOrder;
		{
			DynamicArray<Int> stack = { groupRootNodeIndex };
			while (!stack.empty())
			{
				Int nodeIndex = stack.back();
				stack.pop_back();
				processOrder.push_back(nodeIndex);
				std::ranges::copy_if(crister.Nodes()[nodeIndex].children_, std::back_inserter(stack), [&unionNodes](Int childIndex) { return unionNodes.contains(childIndex); });
			}
		}

		std::unordered_map<Int, Quaternion> solvedWorldRotations = originalRotations;

		for (Int nodeIndex : processOrder)
		{
			if (targetPositions.contains(nodeIndex))
			{
				continue;
			}

			DynamicArray<Quaternion> childRotations;
			DynamicArray<Float> childWeights;

			for (Int childIndex : crister.Nodes()[nodeIndex].children_)
			{
				if (!unionNodes.contains(childIndex))
				{
					continue;
				}

				Vector3 oldDirection = originalPositions[childIndex] - originalPositions[nodeIndex];
				Vector3 newDirection = solvedPositions[childIndex] - solvedPositions[nodeIndex];
				childRotations.push_back(IKPose::SwingRotation(oldDirection, newDirection));
				childWeights.push_back(downstreamWeight[childIndex]);
			}

			if (childRotations.empty())
			{
				continue;
			}

			Quaternion averagedSwing = IKPose::AverageRotation(childRotations, childWeights);
			solvedWorldRotations[nodeIndex] = solvedWorldRotations[nodeIndex] * averagedSwing;
		}

		std::unordered_map<Int, Quaternion> localRotationOverrides;

		for (Int nodeIndex : processOrder)
		{
			Bool isLeafTarget = targetPositions.contains(nodeIndex);
			if (isLeafTarget)
			{
				continue;
			}

			Quaternion originalParentWorldRotation = Quaternion::Identity;
			Quaternion solvedParentWorldRotation = Quaternion::Identity;
			if (nodeIndex == groupRootNodeIndex)
			{
				Int parentIndex = crister.Nodes()[nodeIndex].parentIndex_;
				if (parentIndex >= 0 && static_cast<Size>(parentIndex) < poseGlobalTransforms.size())
				{
					Vector3 parentPosition;
					IKPose::ExtractWorldTransform(poseGlobalTransforms[parentIndex], parentPosition, originalParentWorldRotation);
				}
				solvedParentWorldRotation = originalParentWorldRotation;
			}
			else
			{
				Int parentIndex = crister.Nodes()[nodeIndex].parentIndex_;
				originalParentWorldRotation = originalRotations[parentIndex];
				solvedParentWorldRotation = solvedWorldRotations[parentIndex];
			}

			auto constraintIt = jointConstraints.find(crister.Nodes()[nodeIndex].name_);
			if (constraintIt != jointConstraints.end() && constraintIt->second.axis_.LengthSquared() > 1e-12f)
			{
				const JointConstraint& constraint = constraintIt->second;

				Int primaryChild = -1;
				Float bestWeight = -1.0f;
				for (Int childIndex : crister.Nodes()[nodeIndex].children_)
				{
					if (!unionNodes.contains(childIndex))
					{
						continue;
					}

					Float weight = downstreamWeight[childIndex];
					if (weight > bestWeight)
					{
						bestWeight = weight;
						primaryChild = childIndex;
					}
				}

				if (primaryChild >= 0)
				{
					Vector3 originalDirection = originalPositions[primaryChild] - originalPositions[nodeIndex];

					Quaternion originalInverseRotation;
					originalRotations[nodeIndex].Inverse(originalInverseRotation);
					Vector3 localDirection = Vector3::Transform(originalDirection, originalInverseRotation);
					Vector3 newDirection = Vector3::Transform(localDirection, solvedWorldRotations[nodeIndex]);

					Vector3 worldAxis = Vector3::Transform(constraint.axis_, solvedParentWorldRotation);
					worldAxis.Normalize();
					Vector3 worldSwingAxis = constraint.swingAxis_.LengthSquared() > 1e-12f ? Vector3::Transform(constraint.swingAxis_, solvedParentWorldRotation) : Vector3::Right;
					worldSwingAxis.Normalize();

					Vector3 clampedDirection = IKPose::SwingLimit(newDirection, worldAxis, worldSwingAxis, ToRadians(constraint.swingAngle1_), ToRadians(constraint.swingAngle2_));

					Quaternion correction = IKPose::SwingRotation(newDirection, clampedDirection);
					solvedWorldRotations[nodeIndex] = solvedWorldRotations[nodeIndex] * correction;
				}
			}

			Quaternion solvedParentInverseRotation;
			solvedParentWorldRotation.Inverse(solvedParentInverseRotation);

			Quaternion localRotation = solvedWorldRotations[nodeIndex] * solvedParentInverseRotation;

			Float nodeWeight = downstreamWeight[nodeIndex] > 1.0f ? 1.0f : downstreamWeight[nodeIndex];
			if (nodeWeight < 1.0f)
			{
				Quaternion originalParentInverseRotation;
				originalParentWorldRotation.Inverse(originalParentInverseRotation);
				Quaternion originalLocalRotation = originalRotations[nodeIndex] * originalParentInverseRotation;
				localRotation = Quaternion::Slerp(originalLocalRotation, localRotation, nodeWeight);
			}

			solvedWorldRotations[nodeIndex] = localRotation * solvedParentWorldRotation;
			localRotationOverrides[nodeIndex] = localRotation;
		}

		for (const auto& [endNodeIndex, target] : groupTargets)
		{
			if (!target->hasRotation_)
			{
				continue;
			}

			Int parentIndex = crister.Nodes()[endNodeIndex].parentIndex_;

			Quaternion originalParentWorldRotation = Quaternion::Identity;
			Quaternion solvedParentWorldRotation = Quaternion::Identity;
			if (unionNodes.contains(parentIndex))
			{
				originalParentWorldRotation = originalRotations[parentIndex];
				solvedParentWorldRotation = solvedWorldRotations[parentIndex];
			}
			else if (parentIndex >= 0 && static_cast<Size>(parentIndex) < poseGlobalTransforms.size())
			{
				Vector3 parentPosition;
				IKPose::ExtractWorldTransform(poseGlobalTransforms[parentIndex], parentPosition, originalParentWorldRotation);
				solvedParentWorldRotation = originalParentWorldRotation;
			}

			Quaternion solvedParentInverseRotation;
			solvedParentWorldRotation.Inverse(solvedParentInverseRotation);

			Quaternion localRotation = target->targetRotation_ * solvedParentInverseRotation;

			if (target->weight_ < 1.0f)
			{
				Quaternion originalParentInverseRotation;
				originalParentWorldRotation.Inverse(originalParentInverseRotation);
				Quaternion originalLocalRotation = originalRotations[endNodeIndex] * originalParentInverseRotation;
				localRotation = Quaternion::Slerp(originalLocalRotation, localRotation, target->weight_);
			}

			localRotationOverrides[endNodeIndex] = localRotation;
		}

		DynamicArray<Int> chain = { groupRootNodeIndex };
		IKPose::ApplyChainPose(crister, chain, localRotationOverrides, poseGlobalTransforms);
	}
}
