#include <GraphicsEngine/Model/IK/TwoBoneIK.h>

namespace SeedCore
{
	Float TwoBoneIK::TriangleAngle(Float aLength, Float bLength, Float cLength)
	{
		Float cosAngle = (aLength * aLength + bLength * bLength - cLength * cLength) / (2.0f * aLength * bLength);
		return Acos(Clamp(cosAngle, -1.0f, 1.0f));
	}

	Quaternion TwoBoneIK::FromToRotation(const Vector3& from, const Vector3& to)
	{
		if (from.LengthSquared() < 1e-12f || to.LengthSquared() < 1e-12f)
		{
			return Quaternion::Identity;
		}

		Vector3 f = from;
		f.Normalize();
		Vector3 t = to;
		t.Normalize();

		Float dot = Clamp(f.Dot(t), -1.0f, 1.0f);
		Vector3 axis = f.Cross(t);

		if (axis.LengthSquared() < 1e-12f)
		{
			if (dot > 0.0f)
			{
				return Quaternion::Identity;
			}

			axis = f.Cross(Vector3::Right);
			if (axis.LengthSquared() < 1e-6f)
			{
				axis = f.Cross(Vector3::Up);
			}
			axis.Normalize();
			return Quaternion::CreateFromAxisAngle(axis, DirectX::XM_PI);
		}

		axis.Normalize();
		Float angle = Acos(dot);
		return Quaternion::CreateFromAxisAngle(axis, angle);
	}

	void TwoBoneIK::Solve(const Vector3& rootPosition, const Vector3& midPosition, const Vector3& endPosition, const Vector3& targetPosition, const Vector3& poleVector, const Quaternion& rootWorldRotation, const Quaternion& midWorldRotation, Quaternion& outRootWorldRotation, Quaternion& outMidWorldRotation)
	{
		constexpr Float epsilon = 1e-5f;

		Vector3 rootToMid = midPosition - rootPosition;
		Vector3 midToEnd = endPosition - midPosition;
		Vector3 rootToEnd = endPosition - rootPosition;

		Float upperLength = rootToMid.Length();
		Float lowerLength = midToEnd.Length();
		Float maxLength = Max(upperLength + lowerLength - epsilon, epsilon);
		Float minLength = Abs(upperLength - lowerLength) + epsilon;

		Vector3 rootToTarget = targetPosition - rootPosition;
		Float targetDistance = Clamp(rootToTarget.Length(), Min(minLength, maxLength), maxLength);

		Float oldMidAngle = TriangleAngle(upperLength, lowerLength, rootToEnd.Length());
		Float newMidAngle = TriangleAngle(upperLength, lowerLength, targetDistance);

		Vector3 bendAxis = rootToMid.Cross(poleVector - rootPosition);
		if (bendAxis.LengthSquared() < epsilon)
		{
			bendAxis = rootToMid.Cross(rootToEnd);
		}
		if (bendAxis.LengthSquared() < epsilon)
		{
			bendAxis = rootToMid.Cross(Vector3::Up);
		}
		if (bendAxis.LengthSquared() < epsilon)
		{
			bendAxis = rootToMid.Cross(Vector3::Right);
		}
		bendAxis.Normalize();

		Quaternion bendDelta = Quaternion::CreateFromAxisAngle(bendAxis, newMidAngle - oldMidAngle);
		outMidWorldRotation = midWorldRotation * bendDelta;

		Vector3 bentMidToEnd = Vector3::Transform(midToEnd, bendDelta);
		Vector3 bentRootToEnd = rootToMid + bentMidToEnd;

		Quaternion aimDelta = FromToRotation(bentRootToEnd, rootToTarget);
		outRootWorldRotation = rootWorldRotation * aimDelta;
		outMidWorldRotation = outMidWorldRotation * aimDelta;
	}
}
