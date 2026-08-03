#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class Softbody :public SeedScript
	{
	public:
		enum class BodyType
		{
			Dynamic,
			Kinematic,
			Static,
		};

	public:
		SC_REFLECTION_FIELD_EX("動作モード")
		BodyType bodyType_ = BodyType::Dynamic;

		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 100.0f)
		Float mass_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("面積弾性力", 0.0f, 1.0f)
		Float areaStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("体積弾性力", 0.0f, 1.0f)
		Float volumeStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("抵抗力", 0.0f, 1.0f)
		Float damping_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("ポアソン比", 0.0f, 0.5f)
		Float poissonRatio_ = 0.3f;

		SC_REFLECTION_CLAMPED_EX("最大許容距離", 0.0f, 10.0f)
		Float maxDistance_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("辺補強係数", 0.0f, 1.0f)
		Float edgeStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("曲耐性", 0.0f, 1.0f)
		Float bendStiffness_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("内部圧力", -10.0f, 10.0f)
		Float pressure_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("摩擦係数", 0.0f, 1.0f)
		Float friction_ = 0.2f;

		SC_REFLECTION_CLAMPED_EX("反発係数", 0.0f, 1.0f)
		Float restitution_ = 0.5f;

		SC_REFLECTION_FIELD_EX("重力")
		Bool useGravity_ = true;

		SC_REFLECTION_FIELD_CONDITION(useGravity_)
		SC_REFLECTION_FIELD_EX("重力倍率")
		Float gravityScale_ = 1.0f;

		SC_REFLECTION_FIELD_EX("位置X軸固定")
		Bool freezePositionX_ = false;

		SC_REFLECTION_FIELD_EX("位置Y軸固定")
		Bool freezePositionY_ = false;

		SC_REFLECTION_FIELD_EX("位置Z軸固定")
		Bool freezePositionZ_ = false;

		SC_REFLECTION_FIELD_EX("回転X軸固定")
		Bool freezeRotationX_ = false;

		SC_REFLECTION_FIELD_EX("回転Y軸固定")
		Bool freezeRotationY_ = false;

		SC_REFLECTION_FIELD_EX("回転Z軸固定")
		Bool freezeRotationZ_ = false;

		SC_REFLECTION_CLAMPED_EX("サブステップ数", 1, 20)
		Int subSteps_ = 4;

		SC_REFLECTION_CLAMPED_EX("反転回数", 1, 10)
		Int iterationCount_ = 4;
	};
	REGISTER_COMPONENT(Softbody, "Physics");
}