#pragma once
#include <FoundationEngine/Math/Vector.h>
#include <FoundationEngine/Math/Matrix.h>
#include <FoundationEngine/Math/Quaternion.h>
#include <FoundationEngine/Utility/String.h>

/**
* [EN]
* Cereal serialization support for SeedCore's own math/string types.
* These are free-function ADL customization points (save/load/serialize)
* that Cereal finds automatically for SeedCore::String and the
* Vector/Color/Quaternion/Matrix types whenever they appear as a field of
* a serialized struct (see FoundationEngine/Resource/ActorSerialization.h).
*
* ---------------------------------------------------------------------
*
* [JP]
* SeedCore独自の数学/文字列型に対する Cereal シリアライズサポート。
* これらは Cereal が ADL 経由で自動的に見つけるフリー関数の
* カスタマイズポイント（save/load/serialize）であり、SeedCore::String や
* Vector/Color/Quaternion/Matrix 系の型がシリアライズ対象の構造体の
* フィールドとして現れた際に使われる（FoundationEngine/Resource/ActorSerialization.h 参照）。
*/
namespace cereal
{
	/**
	* [EN]
	* Writes s to archive as its UTF-8 string form.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* s をUTF-8文字列形式で archive に書き込む。
	*/
	template<class Archive>
	void save(Archive& archive, const SeedCore::String& s)
	{
		archive(s.str());
	}

	/**
	* [EN]
	* Reads a UTF-8 string from archive and interns it into s.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* archive からUTF-8文字列を読み込み、s へインターンする。
	*/
	template<class Archive>
	void load(Archive& archive, SeedCore::String& s)
	{
		std::string value;
		archive(value);
		s = SeedCore::String(std::string_view(value));
	}

	/**
	* [EN]
	* Serializes v's x/y components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Vector2& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y)
		);
	}

	/**
	* [EN]
	* Serializes v's x/y/z components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y/z 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Vector3& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y),
			make_nvp("z", v.z)
		);
	}

	/**
	* [EN]
	* Serializes v's x/y/z/w components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y/z/w 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Vector4& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y),
			make_nvp("z", v.z),
			make_nvp("w", v.w)
		);
	}

	/**
	* [EN]
	* Serializes c's r/g/b/a components (backed by the same x/y/z/w
	* storage as Vector4) as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* c の r/g/b/a 成分（Vector4 と同じ x/y/z/w ストレージを利用）を
	* 名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Color& c)
	{
		archive(
			make_nvp("r", c.x),
			make_nvp("g", c.y),
			make_nvp("b", c.z),
			make_nvp("a", c.w)
		);
	}

	/**
	* [EN]
	* Serializes q's x/y/z/w components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* q の x/y/z/w 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Quaternion& q)
	{
		archive(
			make_nvp("x", q.x),
			make_nvp("y", q.y),
			make_nvp("z", q.z),
			make_nvp("w", q.w)
		);
	}

	/**
	* [EN]
	* Serializes every element of m (row-major _11.._44) as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* m の全要素（行優先の _11〜_44）を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::Matrix& m)
	{
		archive(
			make_nvp("_11", m._11), make_nvp("_12", m._12), make_nvp("_13", m._13), make_nvp("_14", m._14),
			make_nvp("_21", m._21), make_nvp("_22", m._22), make_nvp("_23", m._23), make_nvp("_24", m._24),
			make_nvp("_31", m._31), make_nvp("_32", m._32), make_nvp("_33", m._33), make_nvp("_34", m._34),
			make_nvp("_41", m._41), make_nvp("_42", m._42), make_nvp("_43", m._43), make_nvp("_44", m._44)
		);
	}

	/**
	* [EN]
	* Serializes v's x/y components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::XmUint2& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y)
		);
	}

	/**
	* [EN]
	* Serializes v's x/y/z components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y/z 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::XmUint3& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y),
			make_nvp("z", v.z)
		);
	}

	/**
	* [EN]
	* Serializes v's x/y/z/w components as named fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* v の x/y/z/w 成分を名前付きフィールドとしてシリアライズする。
	*/
	template<class Archive>
	void serialize(Archive& archive, SeedCore::XmUint4& v)
	{
		archive(
			make_nvp("x", v.x),
			make_nvp("y", v.y),
			make_nvp("z", v.z),
			make_nvp("w", v.w)
		);
	}
}
