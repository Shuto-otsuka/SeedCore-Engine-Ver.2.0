#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class SEEDCORE_API BoxCollider :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("サイズ")
		Vector3 size_ = { 1.0f, 1.0f, 1.0f };

		SC_REFLECTION_FIELD_EX("中心オフセット")
		Vector3 center_ = { 0.0f, 0.0f, 0.0f };

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
	REGISTER_COMPONENT(BoxCollider, "Collider");
}