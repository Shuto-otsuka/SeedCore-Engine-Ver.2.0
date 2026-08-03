#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class Rigidbody :public SeedScript
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

		SC_REFLECTION_CLAMPED_EX("空気抵抗", 0.0f, 10.0f)
		Float linearDrag_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("角抵抗", 0.0f, 10.0f)
		Float angularDrag_ = 0.5f;

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

	public:
		void OnAwake();

		void OnFixedTick(Float elapsedTime);

		void OnDestroy();

	private:
		JPH::BodyID bodyID_;

		ShapeHandle shapeHandle_;
	};
	REGISTER_COMPONENT(Rigidbody, "Physics");
}