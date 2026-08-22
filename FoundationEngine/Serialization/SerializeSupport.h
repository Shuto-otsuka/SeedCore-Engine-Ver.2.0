#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	template<typename Archive>
	void Save(Archive& archive, const Vector2& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
	}

	template<typename Archive>
	void Load(Archive& archive, Vector2& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
	}

	template<typename Archive>
	void Save(Archive& archive, const Vector3& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
		archive.Field("z", v.z);
	}

	template<typename Archive>
	void Load(Archive& archive, Vector3& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
		archive.TryField("z", v.z);
	}

	template<typename Archive>
	void Save(Archive& archive, const Vector4& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
		archive.Field("z", v.z);
		archive.Field("w", v.w);
	}

	template<typename Archive>
	void Load(Archive& archive, Vector4& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
		archive.TryField("z", v.z);
		archive.TryField("w", v.w);
	}

	template<typename Archive>
	void Save(Archive& archive, const Color& c)
	{
		archive.Field("r", c.x);
		archive.Field("g", c.y);
		archive.Field("b", c.z);
		archive.Field("a", c.w);
	}

	template<typename Archive>
	void Load(Archive& archive, Color& c)
	{
		archive.TryField("r", c.x);
		archive.TryField("g", c.y);
		archive.TryField("b", c.z);
		archive.TryField("a", c.w);
	}

	template<typename Archive>
	void Save(Archive& archive, const Quaternion& q)
	{
		archive.Field("x", q.x);
		archive.Field("y", q.y);
		archive.Field("z", q.z);
		archive.Field("w", q.w);
	}

	template<typename Archive>
	void Load(Archive& archive, Quaternion& q)
	{
		archive.TryField("x", q.x);
		archive.TryField("y", q.y);
		archive.TryField("z", q.z);
		archive.TryField("w", q.w);
	}

	template<typename Archive>
	void Save(Archive& archive, const Matrix& m)
	{
		archive.Field("_11", m._11); archive.Field("_12", m._12); archive.Field("_13", m._13); archive.Field("_14", m._14);
		archive.Field("_21", m._21); archive.Field("_22", m._22); archive.Field("_23", m._23); archive.Field("_24", m._24);
		archive.Field("_31", m._31); archive.Field("_32", m._32); archive.Field("_33", m._33); archive.Field("_34", m._34);
		archive.Field("_41", m._41); archive.Field("_42", m._42); archive.Field("_43", m._43); archive.Field("_44", m._44);
	}

	template<typename Archive>
	void Load(Archive& archive, Matrix& m)
	{
		archive.TryField("_11", m._11); archive.TryField("_12", m._12); archive.TryField("_13", m._13); archive.TryField("_14", m._14);
		archive.TryField("_21", m._21); archive.TryField("_22", m._22); archive.TryField("_23", m._23); archive.TryField("_24", m._24);
		archive.TryField("_31", m._31); archive.TryField("_32", m._32); archive.TryField("_33", m._33); archive.TryField("_34", m._34);
		archive.TryField("_41", m._41); archive.TryField("_42", m._42); archive.TryField("_43", m._43); archive.TryField("_44", m._44);
	}

	template<typename Archive>
	void Save(Archive& archive, const XmUint2& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
	}

	template<typename Archive>
	void Load(Archive& archive, XmUint2& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
	}

	template<typename Archive>
	void Save(Archive& archive, const XmUint3& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
		archive.Field("z", v.z);
	}

	template<typename Archive>
	void Load(Archive& archive, XmUint3& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
		archive.TryField("z", v.z);
	}

	template<typename Archive>
	void Save(Archive& archive, const XmUint4& v)
	{
		archive.Field("x", v.x);
		archive.Field("y", v.y);
		archive.Field("z", v.z);
		archive.Field("w", v.w);
	}

	template<typename Archive>
	void Load(Archive& archive, XmUint4& v)
	{
		archive.TryField("x", v.x);
		archive.TryField("y", v.y);
		archive.TryField("z", v.z);
		archive.TryField("w", v.w);
	}
}
