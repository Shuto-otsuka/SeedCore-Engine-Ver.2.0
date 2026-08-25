#include <PhysicsEngine/Collider/MeshCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	void MeshCollider::OnAwake()
	{
		pending_ = meshID_ != 0;
	}

	void MeshCollider::OnDestroy()
	{
		if (shapeHandle_.empty())
		{
			return;
		}

		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);

		bodyID_ = JPH::BodyID();
		shapeHandle_ = ShapeHandle::null();
		pending_ = meshID_ != 0;
	}

	Bool MeshCollider::IsPending()const
	{
		return pending_;
	}

	void MeshCollider::Build(const MeshCollision& meshCollision)
	{
		Actor& actor = GetActor();
		Rigidbody* rigidbody = actor.GetComponent<Rigidbody>();

		Bool convex = convex_;
		if (!convex && rigidbody && rigidbody->bodyType_ == Rigidbody::BodyType::Dynamic)
		{
			SC_LOG_WARNING("MeshCollider: Dynamic な Rigidbody には凹メッシュ形状を使えないため凸包で生成します (assetID: %u)", meshID_);
			convex = true;
		}

		shapeHandle_ = convex
			? actor.GetPhysics().CreateConvexShape(meshID_, meshCollision.Positions())
			: actor.GetPhysics().CreateMeshShape(meshID_, meshCollision.Positions(), meshCollision.Indices());

		if (shapeHandle_.empty())
		{
			return;
		}

		pending_ = false;

		if (rigidbody && !rigidbody->GetBodyID().IsInvalid())
		{
			actor.GetPhysics().SetBodyShape(rigidbody->GetBodyID(), shapeHandle_);
			return;
		}

		bodyID_ = PhysicsSystem::CreateColliderBody(actor, shapeHandle_, isTrigger_);
	}
}
