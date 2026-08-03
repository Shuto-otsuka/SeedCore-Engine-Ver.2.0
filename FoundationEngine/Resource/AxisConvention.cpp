#include <FoundationEngine/Resource/AxisConvention.h>

namespace SeedCore
{
	namespace
	{
		Vector3 SignedAxisToVector(SignedAxis axis)
		{
			switch (axis)
			{
			case SignedAxis::PositiveX:
				return Vector3(1.0f, 0.0f, 0.0f);
			case SignedAxis::NegativeX:
				return Vector3(-1.0f, 0.0f, 0.0f);
			case SignedAxis::PositiveY:
				return Vector3(0.0f, 1.0f, 0.0f);
			case SignedAxis::NegativeY:
				return Vector3(0.0f, -1.0f, 0.0f);
			case SignedAxis::PositiveZ:
				return Vector3(0.0f, 0.0f, 1.0f);
			case SignedAxis::NegativeZ:
			default:
				return Vector3(0.0f, 0.0f, -1.0f);
			}
		}

		/// [EN] Two picks are collinear (invalid combination) when they name the same underlying axis, regardless of sign.
		/// [JP] 符号に関わらず同じ軸を指している場合、2つの選択は共線（無効な組み合わせ）とみなす。
		Bool IsCollinear(const Vector3& a, const Vector3& b)
		{
			return (a == b) || (a == -b);
		}
	}

	ResolvedAxisConvention ResolvedAxisConvention::Resolve(const AxisConvention& convention)
	{
		Vector3 up = SignedAxisToVector(convention.up_);
		Vector3 right = SignedAxisToVector(convention.right_);
		Vector3 forward = SignedAxisToVector(convention.forward_);

		ResolvedAxisConvention resolved{};
		resolved.valid_ = !IsCollinear(up, right) && !IsCollinear(up, forward) && !IsCollinear(right, forward);
		if (!resolved.valid_)
		{
			return resolved;
		}

		/// [EN] Row-vector convention, matching this engine's existing Matrix usage
		///      (Node::globalTransform_ etc.): row 0 = Right, row 1 = Up, row 2 = Forward,
		///      row 3 = identity translation. v' = Vector3::Transform(v, basis_).
		/// [JP] このエンジンの既存Matrix使用（Node::globalTransform_ 等）と同じ行ベクトル
		///      規約: row0=Right, row1=Up, row2=Forward, row3=平行移動の単位。
		///      v' = Vector3::Transform(v, basis_)。
		resolved.basis_ = Matrix::Identity;
		resolved.basis_._11 = right.x; 
		resolved.basis_._12 = right.y; 
		resolved.basis_._13 = right.z;
		resolved.basis_._21 = up.x;   
		resolved.basis_._22 = up.y;    
		resolved.basis_._23 = up.z;
		resolved.basis_._31 = forward.x;
		resolved.basis_._32 = forward.y; 
		resolved.basis_._33 = forward.z;

		Float determinant = resolved.basis_.Determinant();
		resolved.isMirror_ = (determinant < 0.0f);
		resolved.isRightHanded_ = resolved.isMirror_;

		switch (convention.windingOverride_)
		{
		case WindingOverride::AsIs:
			resolved.flipWinding_ = false;
			break;
		case WindingOverride::Flip:
			resolved.flipWinding_ = true;
			break;
		case WindingOverride::Auto:
		default:
			resolved.flipWinding_ = false;
			break;
		}

		return resolved;
	}
}
