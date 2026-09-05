#include <GraphicsEngine/Model/IK/IKPose.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Animation/Animator.h>

namespace SeedCore
{
	Int IKPose::ResolveChainRoot(const Crister& crister, Int endNodeIndex)
	{
		const DynamicArray<Node>& nodes = crister.Nodes();
		if (endNodeIndex < 0 || static_cast<Size>(endNodeIndex) >= nodes.size())
		{
			return -1;
		}

		Int current = nodes[endNodeIndex].parentIndex_;
		constexpr Size maxDepth = 256;
		for (Size step = 0; step < maxDepth; step++)
		{
			if (current < 0 || static_cast<Size>(current) >= nodes.size())
			{
				return -1;
			}

			Int parentIndex = nodes[current].parentIndex_;
			if (parentIndex < 0)
			{
				return current;
			}

			if (nodes[parentIndex].children_.size() > 1)
			{
				return current;
			}

			current = parentIndex;
		}

		return -1;
	}

	void IKPose::AutoDetectJointConstraints(const Crister& crister, Animator& animator)
	{
		constexpr Float defaultSwingAngle1 = 45.0f;
		constexpr Float defaultSwingAngle2 = 45.0f;

		const DynamicArray<Node>& nodes = crister.Nodes();

		for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
		{
			const Node& node = nodes[nodeIndex];
			if (node.parentIndex_ < 0 || node.children_.empty())
			{
				continue;
			}

			Int childIndex = node.children_[0];
			if (childIndex < 0 || static_cast<Size>(childIndex) >= nodes.size())
			{
				continue;
			}

			Vector3 thisPosition;
			Quaternion thisRotation;
			ExtractWorldTransform(node.globalTransform_, thisPosition, thisRotation);

			Vector3 childPosition;
			Quaternion childRotation;
			ExtractWorldTransform(nodes[static_cast<Size>(childIndex)].globalTransform_, childPosition, childRotation);

			Vector3 worldAxis = childPosition - thisPosition;
			if (worldAxis.LengthSquared() < 1e-8f)
			{
				continue;
			}
			worldAxis.Normalize();

			Vector3 parentPosition;
			Quaternion parentWorldRotation;
			ExtractWorldTransform(nodes[static_cast<Size>(node.parentIndex_)].globalTransform_, parentPosition, parentWorldRotation);

			Quaternion parentInverseRotation;
			parentWorldRotation.Inverse(parentInverseRotation);

			Vector3 worldSwingReference = Abs(worldAxis.Dot(Vector3::Right)) < 0.9f ? Vector3::Right : Vector3::Up;
			Vector3 worldSwingAxis = worldSwingReference - worldAxis * worldSwingReference.Dot(worldAxis);
			worldSwingAxis.Normalize();

			Vector3 localAxis = Vector3::Transform(worldAxis, parentInverseRotation);
			Vector3 localSwingAxis = Vector3::Transform(worldSwingAxis, parentInverseRotation);

			animator.SetJointConstraint(node.name_, localAxis, localSwingAxis, defaultSwingAngle1, defaultSwingAngle2);
		}
	}

	DynamicArray<Int> IKPose::ResolveChain(const Crister& crister, Int rootNodeIndex, Int endNodeIndex)
	{
		const DynamicArray<Node>& nodes = crister.Nodes();

		DynamicArray<Int> chain;
		Int current = endNodeIndex;
		constexpr Size maxDepth = 256;
		for (Size step = 0; step < maxDepth; step++)
		{
			if (current < 0 || static_cast<Size>(current) >= nodes.size())
			{
				return {};
			}

			chain.push_back(current);

			if (current == rootNodeIndex)
			{
				std::ranges::reverse(chain);
				return chain;
			}

			current = nodes[current].parentIndex_;
		}

		return {};
	}

	void IKPose::ExtractWorldTransform(const Matrix& globalTransform, Vector3& outPosition, Quaternion& outRotation)
	{
		Vector3 scale;
		Matrix copy = globalTransform;
		copy.Decompose(scale, outRotation, outPosition);
	}

	Quaternion IKPose::SwingRotation(const Vector3& fromDirection, const Vector3& toDirection)
	{
		if (fromDirection.LengthSquared() < 1e-12f || toDirection.LengthSquared() < 1e-12f)
		{
			return Quaternion::Identity;
		}

		Vector3 from = fromDirection;
		from.Normalize();
		Vector3 to = toDirection;
		to.Normalize();

		Float dot = Clamp(from.Dot(to), -1.0f, 1.0f);
		Vector3 axis = from.Cross(to);

		if (axis.LengthSquared() < 1e-12f)
		{
			if (dot > 0.0f)
			{
				return Quaternion::Identity;
			}

			axis = from.Cross(Vector3::Right);
			if (axis.LengthSquared() < 1e-6f)
			{
				axis = from.Cross(Vector3::Up);
			}
			axis.Normalize();
			return Quaternion::CreateFromAxisAngle(axis, DirectX::XM_PI);
		}

		axis.Normalize();
		Float angle = Acos(dot);
		return Quaternion::CreateFromAxisAngle(axis, angle);
	}

	Quaternion IKPose::AverageRotation(const DynamicArray<Quaternion>& rotations, const DynamicArray<Float>& weights)
	{
		if (rotations.empty())
		{
			return Quaternion::Identity;
		}

		Quaternion reference = rotations[0];

		Float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f, sumW = 0.0f;
		Float totalWeight = 0.0f;

		for (Size index = 0; index < rotations.size(); index++)
		{
			Quaternion rotation = rotations[index];
			if (rotation.Dot(reference) < 0.0f)
			{
				rotation = -rotation;
			}

			Float weight = weights[index];
			sumX += rotation.x * weight;
			sumY += rotation.y * weight;
			sumZ += rotation.z * weight;
			sumW += rotation.w * weight;
			totalWeight += weight;
		}

		if (totalWeight <= 0.0f)
		{
			return reference;
		}

		Quaternion averaged(sumX / totalWeight, sumY / totalWeight, sumZ / totalWeight, sumW / totalWeight);
		averaged.Normalize();
		return averaged;
	}

	Vector3 IKPose::SwingLimit(const Vector3& direction, const Vector3& axis, const Vector3& swingAxis, Float maxAngle1, Float maxAngle2)
	{
		if (direction.LengthSquared() < 1e-12f || axis.LengthSquared() < 1e-12f)
		{
			return direction;
		}

		Vector3 forward = axis;
		forward.Normalize();

		Vector3 reference1 = swingAxis - forward * swingAxis.Dot(forward);
		if (reference1.LengthSquared() < 1e-8f)
		{
			reference1 = Abs(forward.Dot(Vector3::Right)) < 0.9f ? Vector3::Right : Vector3::Up;
			reference1 = reference1 - forward * reference1.Dot(forward);
		}
		reference1.Normalize();

		Vector3 reference2 = forward.Cross(reference1);
		reference2.Normalize();

		Float length = direction.Length();
		Vector3 to = direction;
		to.Normalize();

		Float dot = Clamp(forward.Dot(to), -1.0f, 1.0f);
		Float theta = Acos(dot);

		Vector3 perpendicular = to - forward * dot;
		if (perpendicular.LengthSquared() < 1e-8f)
		{
			return direction;
		}
		perpendicular.Normalize();

		Float phi = Atan2(perpendicular.Dot(reference2), perpendicular.Dot(reference1));

		Float cosPhi = Cos(phi);
		Float sinPhi = Sin(phi);
		Float denominator = std::sqrt((maxAngle2 * cosPhi) * (maxAngle2 * cosPhi) + (maxAngle1 * sinPhi) * (maxAngle1 * sinPhi));
		Float allowedTheta = denominator > 1e-8f ? (maxAngle1 * maxAngle2) / denominator : 0.0f;

		if (theta <= allowedTheta)
		{
			return direction;
		}

		Vector3 clamped = forward * Cos(allowedTheta) + perpendicular * Sin(allowedTheta);
		clamped.Normalize();
		return clamped * length;
	}

	void IKPose::ApplyChainPose(const Crister& crister, const DynamicArray<Int>& chain, const std::unordered_map<Int, Quaternion>& newLocalRotations, DynamicArray<Matrix>& poseGlobalTransforms)
	{
		if (chain.empty())
		{
			return;
		}

		DynamicArray<Matrix> originalPoseGlobalTransforms = poseGlobalTransforms;

		Int rootNodeIndex = chain.front();
		const DynamicArray<Node>& nodes = crister.Nodes();
		const Node& rootNode = nodes[rootNodeIndex];

		Matrix parentGlobal = Matrix::Identity;
		if (rootNode.parentIndex_ >= 0 && static_cast<Size>(rootNode.parentIndex_) < originalPoseGlobalTransforms.size())
		{
			parentGlobal = originalPoseGlobalTransforms[rootNode.parentIndex_];
		}

		RecomputeSubtree(crister, rootNodeIndex, parentGlobal, newLocalRotations, originalPoseGlobalTransforms, poseGlobalTransforms);
	}

	void IKPose::RecomputeSubtree(const Crister& crister, Int nodeIndex, const Matrix& parentGlobal, const std::unordered_map<Int, Quaternion>& newLocalRotations, const DynamicArray<Matrix>& originalPoseGlobalTransforms, DynamicArray<Matrix>& poseGlobalTransforms)
	{
		const DynamicArray<Node>& nodes = crister.Nodes();
		const Node& node = nodes[nodeIndex];

		Matrix originalParentGlobal = Matrix::Identity;
		if (node.parentIndex_ >= 0 && static_cast<Size>(node.parentIndex_) < originalPoseGlobalTransforms.size())
		{
			originalParentGlobal = originalPoseGlobalTransforms[node.parentIndex_];
		}

		Matrix originalLocal = originalPoseGlobalTransforms[nodeIndex] * originalParentGlobal.Invert();

		Vector3 localScale;
		Quaternion localRotation;
		Vector3 localTranslation;
		originalLocal.Decompose(localScale, localRotation, localTranslation);

		auto overrideIt = newLocalRotations.find(nodeIndex);
		if (overrideIt != newLocalRotations.end())
		{
			localRotation = overrideIt->second;
		}

		Matrix scaleMatrix = Matrix::CreateScale(localScale.x, localScale.y, localScale.z);
		Matrix rotationMatrix = Matrix::CreateFromQuaternion(localRotation);
		Matrix translationMatrix = Matrix::CreateTranslation(localTranslation.x, localTranslation.y, localTranslation.z);
		Matrix localTransform = scaleMatrix * rotationMatrix * translationMatrix;

		Matrix global = localTransform * parentGlobal;
		poseGlobalTransforms[nodeIndex] = global;

		for (Int childIndex : node.children_)
		{
			RecomputeSubtree(crister, childIndex, global, newLocalRotations, originalPoseGlobalTransforms, poseGlobalTransforms);
		}
	}
}
