#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <GraphicsEngine/Model/Collision/MeshCollision.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class SEEDCORE_API MeshCollider :public SeedScript
	{
	public:
		SC_PAYLOAD_FIELD_EX("コリジョンメッシュ", MeshCollision)
		Uint32 meshID_ = 0;

		SC_REFLECTION_FIELD_EX("凸包にする")
		Bool convex_ = false;

		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		void OnAwake();

		void OnDestroy();

	public:
		Bool IsPending()const;

		void Build(const MeshCollision& meshCollision);

	private:
		JPH::BodyID bodyID_;

		ShapeHandle shapeHandle_;

		Bool pending_ = true;
	};
	REGISTER_COMPONENT(MeshCollider, "Collider");
}
