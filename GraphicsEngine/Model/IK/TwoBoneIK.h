#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class TwoBoneIK
	{
	public:
		static void Solve(const Vector3& rootPosition, const Vector3& midPosition, const Vector3& endPosition, const Vector3& targetPosition, const Vector3& poleVector, const Quaternion& rootWorldRotation, const Quaternion& midWorldRotation, Quaternion& outRootWorldRotation, Quaternion& outMidWorldRotation);

	private:
		static Float TriangleAngle(Float aLength, Float bLength, Float cLength);

		static Quaternion FromToRotation(const Vector3& from, const Vector3& to);
	};
}
