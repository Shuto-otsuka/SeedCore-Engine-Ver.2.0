#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class CylinderCollider :public SeedScript
	{
	public:
		SC_REFLECTION_CLAMPED_EX("高さ", 0.001f, 100.0f)
		Float height_ = 2.0f;

		SC_REFLECTION_CLAMPED_EX("半径", 0.001f, 100.0f)
		Float radius_ = 0.5f;

	public:
		ShapeHandle GetShapeHandle()const;

		void OnAwake();

		void OnDestroy();

	private:
		JPH::BodyID bodyID_;

		ShapeHandle shapeHandle_;
	};
	REGISTER_COMPONENT(CylinderCollider, "Collider");
}