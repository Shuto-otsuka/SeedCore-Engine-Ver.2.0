#include <GraphicsEngine/Model/IK/TreeFABRIK.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	void TreeFABRIK::Solve(const Crister& crister, Int rootNodeIndex, const DynamicArray<Target>& targets, std::unordered_map<Int, Vector3>& jointPositions, Float tolerance, Int maxIterations)
	{
		if (targets.empty() || jointPositions.find(rootNodeIndex) == jointPositions.end())
		{
			return;
		}

		std::unordered_set<Int> activeNodes;
		std::unordered_map<Int, Float> downstreamWeight;
		std::unordered_map<Int, Vector3> targetPositions;
		std::unordered_map<Int, Float> segmentLengths;

		for (const Target& target : targets)
		{
			if (jointPositions.find(target.nodeIndex_) == jointPositions.end())
			{
				continue;
			}

			targetPositions[target.nodeIndex_] = target.targetPosition_;

			Int current = target.nodeIndex_;
			while (true)
			{
				activeNodes.insert(current);
				downstreamWeight[current] += target.weight_;

				if (current == rootNodeIndex)
				{
					break;
				}

				Int parentIndex = crister.Nodes()[current].parentIndex_;
				if (parentIndex < 0 || jointPositions.find(parentIndex) == jointPositions.end())
				{
					break;
				}

				if (segmentLengths.find(current) == segmentLengths.end())
				{
					segmentLengths[current] = Vector3::Distance(jointPositions[current], jointPositions[parentIndex]);
				}

				current = parentIndex;
			}
		}

		Vector3 anchorPosition = jointPositions[rootNodeIndex];

		for (Int iteration = 0; iteration < maxIterations; iteration++)
		{
			Float maxError = 0.0f;
			for (const auto& [nodeIndex, targetPosition] : targetPositions)
			{
				maxError = Max(maxError, Vector3::Distance(jointPositions[nodeIndex], targetPosition));
			}
			if (maxError < tolerance)
			{
				break;
			}

			BackwardPull(crister, rootNodeIndex, activeNodes, downstreamWeight, segmentLengths, targetPositions, jointPositions);
			jointPositions[rootNodeIndex] = anchorPosition;
			ForwardPush(crister, rootNodeIndex, anchorPosition, activeNodes, segmentLengths, jointPositions);
		}
	}

	Vector3 TreeFABRIK::BackwardPull(const Crister& crister, Int nodeIndex, const std::unordered_set<Int>& activeNodes, const std::unordered_map<Int, Float>& downstreamWeight, const std::unordered_map<Int, Float>& segmentLengths, const std::unordered_map<Int, Vector3>& targetPositions, std::unordered_map<Int, Vector3>& jointPositions)
	{
		auto targetIt = targetPositions.find(nodeIndex);
		if (targetIt != targetPositions.end())
		{
			jointPositions[nodeIndex] = targetIt->second;
			return targetIt->second;
		}

		Vector3 weightedPosition = Vector3::Zero;
		Float totalWeight = 0.0f;

		for (Int childIndex : crister.Nodes()[nodeIndex].children_)
		{
			if (activeNodes.find(childIndex) == activeNodes.end())
			{
				continue;
			}

			Vector3 childPulledPosition = BackwardPull(crister, childIndex, activeNodes, downstreamWeight, segmentLengths, targetPositions, jointPositions);

			Vector3 direction = jointPositions[nodeIndex] - childPulledPosition;
			direction = direction.LengthSquared() > 1e-12f ? direction : Vector3::Up;
			direction.Normalize();
			Float segmentLength = segmentLengths.at(childIndex);
			Vector3 candidatePosition = childPulledPosition + direction * segmentLength;

			Float childWeight = downstreamWeight.at(childIndex);
			weightedPosition += candidatePosition * childWeight;
			totalWeight += childWeight;
		}

		Vector3 pulledPosition = totalWeight > 0.0f ? weightedPosition / totalWeight : jointPositions[nodeIndex];
		jointPositions[nodeIndex] = pulledPosition;
		return pulledPosition;
	}

	void TreeFABRIK::ForwardPush(const Crister& crister, Int nodeIndex, const Vector3& parentPosition, const std::unordered_set<Int>& activeNodes, const std::unordered_map<Int, Float>& segmentLengths, std::unordered_map<Int, Vector3>& jointPositions)
	{
		jointPositions[nodeIndex] = parentPosition;

		for (Int childIndex : crister.Nodes()[nodeIndex].children_)
		{
			if (activeNodes.find(childIndex) == activeNodes.end())
			{
				continue;
			}

			Vector3 direction = jointPositions[childIndex] - parentPosition;
			direction = direction.LengthSquared() > 1e-12f ? direction : Vector3::Up;
			direction.Normalize();
			Float segmentLength = segmentLengths.at(childIndex);
			Vector3 childForwardPosition = parentPosition + direction * segmentLength;

			ForwardPush(crister, childIndex, childForwardPosition, activeNodes, segmentLengths, jointPositions);
		}
	}
}
