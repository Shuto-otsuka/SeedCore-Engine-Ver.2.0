#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class Crister;

	class TreeFABRIK
	{
	public:
		struct Target
		{
			Int nodeIndex_ = -1;
			Vector3 targetPosition_ = Vector3::Zero;
			Float weight_ = 1.0f;
		};

		static void Solve(const Crister& crister, Int rootNodeIndex, const DynamicArray<Target>& targets, std::unordered_map<Int, Vector3>& jointPositions, Float tolerance, Int maxIterations);

	private:
		static Vector3 BackwardPull(const Crister& crister, Int nodeIndex, const std::unordered_set<Int>& activeNodes, const std::unordered_map<Int, Float>& downstreamWeight, const std::unordered_map<Int, Float>& segmentLengths, const std::unordered_map<Int, Vector3>& targetPositions, std::unordered_map<Int, Vector3>& jointPositions);

		static void ForwardPush(const Crister& crister, Int nodeIndex, const Vector3& parentPosition, const std::unordered_set<Int>& activeNodes, const std::unordered_map<Int, Float>& segmentLengths, std::unordered_map<Int, Vector3>& jointPositions);
	};
}
