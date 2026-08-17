#include <GraphicsEngine/Model/IK/FABRIK.h>

namespace SeedCore
{
	void FABRIK::Solve(DynamicArray<Vector3>& jointPositions, const Vector3& targetPosition, Float tolerance, Int maxIterations)
	{
		Size jointCount = jointPositions.size();
		if (jointCount < 2)
		{
			return;
		}

		DynamicArray<Float> segmentLengths;
		segmentLengths.reserve(jointCount - 1);
		Float totalLength = 0.0f;
		for (Size jointIndex = 0; jointIndex + 1 < jointCount; jointIndex++)
		{
			Float segmentLength = Vector3::Distance(jointPositions[jointIndex], jointPositions[jointIndex + 1]);
			segmentLengths.push_back(segmentLength);
			totalLength += segmentLength;
		}

		Vector3 rootPosition = jointPositions[0];
		Float rootToTargetDistance = Vector3::Distance(rootPosition, targetPosition);

		if (rootToTargetDistance >= totalLength)
		{
			for (Size jointIndex = 0; jointIndex + 1 < jointCount; jointIndex++)
			{
				Vector3 direction = targetPosition - jointPositions[jointIndex];
				direction = direction.LengthSquared() > 1e-12f ? direction : Vector3::Up;
				direction.Normalize();
				jointPositions[jointIndex + 1] = jointPositions[jointIndex] + direction * segmentLengths[jointIndex];
			}
			return;
		}

		for (Int iteration = 0; iteration < maxIterations; iteration++)
		{
			if (Vector3::Distance(jointPositions[jointCount - 1], targetPosition) < tolerance)
			{
				break;
			}

			jointPositions[jointCount - 1] = targetPosition;
			for (Size jointIndex = jointCount - 1; jointIndex > 0; jointIndex--)
			{
				Vector3 direction = jointPositions[jointIndex - 1] - jointPositions[jointIndex];
				direction = direction.LengthSquared() > 1e-12f ? direction : Vector3::Up;
				direction.Normalize();
				jointPositions[jointIndex - 1] = jointPositions[jointIndex] + direction * segmentLengths[jointIndex - 1];
			}

			jointPositions[0] = rootPosition;
			for (Size jointIndex = 0; jointIndex + 1 < jointCount; jointIndex++)
			{
				Vector3 direction = jointPositions[jointIndex + 1] - jointPositions[jointIndex];
				direction = direction.LengthSquared() > 1e-12f ? direction : Vector3::Up;
				direction.Normalize();
				jointPositions[jointIndex + 1] = jointPositions[jointIndex] + direction * segmentLengths[jointIndex];
			}
		}
	}
}
