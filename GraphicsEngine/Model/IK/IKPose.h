#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class Crister;
	class Animator;

	class IKPose
	{
	public:
		static Int ResolveChainRoot(const Crister& crister, Int endNodeIndex);

		static void AutoDetectJointConstraints(const Crister& crister, Animator& animator);

		static DynamicArray<Int> ResolveChain(const Crister& crister, Int rootNodeIndex, Int endNodeIndex);

		static void ExtractWorldTransform(const Matrix& globalTransform, Vector3& outPosition, Quaternion& outRotation);

		static Quaternion SwingRotation(const Vector3& fromDirection, const Vector3& toDirection);

		static Quaternion AverageRotation(const DynamicArray<Quaternion>& rotations, const DynamicArray<Float>& weights);

		static Vector3 SwingLimit(const Vector3& direction, const Vector3& axis, const Vector3& swingAxis, Float maxAngle1, Float maxAngle2);

		static void ApplyChainPose(const Crister& crister, const DynamicArray<Int>& chain, const std::unordered_map<Int, Quaternion>& newLocalRotations, DynamicArray<Matrix>& poseGlobalTransforms);

	private:
		static void RecomputeSubtree(const Crister& crister, Int nodeIndex, const Matrix& parentGlobal, const std::unordered_map<Int, Quaternion>& newLocalRotations, const DynamicArray<Matrix>& originalPoseGlobalTransforms, DynamicArray<Matrix>& poseGlobalTransforms);
	};
}
