#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class CircleCollider :public SeedScript
	{
	public:
		SC_REFLECTION_CLAMPED_EX("半径", 0.001f, 100.0f)
		Float radius_ = 0.5f;

		SC_REFLECTION_FIELD_EX("中心オフセット")
		Vector2 center_ = { 0.0f, 0.0f };

		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		ShapeHandle GetShapeHandle()const;

		void OnAwake();

		void OnDestroy();

	private:
		JPH::BodyID bodyID_;

		ShapeHandle shapeHandle_;
	};
	REGISTER_COMPONENT(CircleCollider, "Collider");
}