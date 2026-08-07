#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <PhysicsEngine/Collider/BoxCollider.h>
#include <PhysicsEngine/Collider/SphereCollider.h>
#include <PhysicsEngine/Collider/CapsuleCollider.h>
#include <PhysicsEngine/Collider/CylinderCollider.h>
#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Collider/CircleCollider.h>
#include <PhysicsEngine/Collider/MeshCollider.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionResource.h>

namespace SeedCore
{
	ShapeHandle PhysicsSystem::FindColliderShape(Actor& actor)
	{
		if (BoxCollider* collider = actor.GetComponent<BoxCollider>())
		{
			return collider->GetShapeHandle();
		}
		if (SphereCollider* collider = actor.GetComponent<SphereCollider>())
		{
			return collider->GetShapeHandle();
		}
		if (CapsuleCollider* collider = actor.GetComponent<CapsuleCollider>())
		{
			return collider->GetShapeHandle();
		}
		if (CylinderCollider* collider = actor.GetComponent<CylinderCollider>())
		{
			return collider->GetShapeHandle();
		}
		if (RectCollider* collider = actor.GetComponent<RectCollider>())
		{
			return collider->GetShapeHandle();
		}
		if (CircleCollider* collider = actor.GetComponent<CircleCollider>())
		{
			return collider->GetShapeHandle();
		}

		return actor.GetPhysics().CreateSphereShape(defaultColliderShapeRadius);
	}

	JPH::EAllowedDOFs PhysicsSystem::ToAllowedDOFs(const Rigidbody& rigidbody)
	{
		JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;

		if (rigidbody.freezePositionX_)
		{
			dofs &= ~JPH::EAllowedDOFs::TranslationX;
		}
		if (rigidbody.freezePositionY_)
		{
			dofs &= ~JPH::EAllowedDOFs::TranslationY;
		}
		if (rigidbody.freezePositionZ_)
		{
			dofs &= ~JPH::EAllowedDOFs::TranslationZ;
		}
		if (rigidbody.freezeRotationX_)
		{
			dofs &= ~JPH::EAllowedDOFs::RotationX;
		}
		if (rigidbody.freezeRotationY_)
		{
			dofs &= ~JPH::EAllowedDOFs::RotationY;
		}
		if (rigidbody.freezeRotationZ_)
		{
			dofs &= ~JPH::EAllowedDOFs::RotationZ;
		}

		return dofs;
	}

	void PhysicsSystem::ApplyActorTransform(const Actor& actor, BodyDesc& desc)
	{
		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();

		desc.position_ = position ? Vector3{ position->x, position->y, position->z } : Vector3{ 0.0f, 0.0f, 0.0f };
		desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y), ToRadians(rotation->x), ToRadians(rotation->z)) : Quaternion::Identity;
	}

	JPH::BodyID PhysicsSystem::CreateColliderBody(Actor& actor, ShapeHandle shape)
	{
		if (actor.GetComponent<Rigidbody>())
		{
			return JPH::BodyID();
		}

		BodyDesc desc;
		desc.shape_ = shape;
		desc.motionType_ = JPH::EMotionType::Static;
		desc.layer_ = Layers::STATIC;
		ApplyActorTransform(actor, desc);

		return actor.GetPhysics().CreateBody(desc);
	}

	void PhysicsSystem::DestroyColliderBody(Actor& actor, JPH::BodyID bodyID, ShapeHandle shape)
	{
		if (!bodyID.IsInvalid())
		{
			actor.GetPhysics().DestroyBody(bodyID);
		}

		actor.GetPhysics().ReleaseShape(shape);
	}

	namespace
	{
		void GetActorTransform(const Actor& actor, Vector3& outPosition, Quaternion& outRotation)
		{
			const Position* position = actor.GetComponent<Position>();
			const Rotation* rotation = actor.GetComponent<Rotation>();

			outPosition = position ? Vector3(position->x, position->y, position->z) : Vector3(0.0f, 0.0f, 0.0f);
			outRotation = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y), ToRadians(rotation->x), ToRadians(rotation->z)) : Quaternion::Identity;
		}
	}

	DynamicArray<ColliderInstance> PhysicsSystem::GatherColliderInstances(World& world)
	{
		DynamicArray<ColliderInstance> instances;

		const Color colliderDebugColor(0.0f, 1.0f, 0.0f, 1.0f);

		for (EntityID id : world.GetComponents<BoxCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			BoxCollider* collider = actor->GetComponent<BoxCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			ColliderInstance instance{};
			instance.position_ = actorPosition + Vector3::Transform(collider->center_, actorRotation);
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Box);
			instance.rotation_ = actorRotation;
			instance.dimensions_ = collider->size_ * 0.5f;
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		for (EntityID id : world.GetComponents<SphereCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			SphereCollider* collider = actor->GetComponent<SphereCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			ColliderInstance instance{};
			instance.position_ = actorPosition;
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Sphere);
			instance.rotation_ = actorRotation;
			instance.dimensions_ = Vector3(collider->radius_, 0.0f, 0.0f);
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		for (EntityID id : world.GetComponents<CapsuleCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			CapsuleCollider* collider = actor->GetComponent<CapsuleCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			ColliderInstance instance{};
			instance.position_ = actorPosition;
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Capsule);
			instance.rotation_ = actorRotation;
			instance.dimensions_ = Vector3(collider->radius_, collider->height_ * 0.5f, 0.0f);
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		for (EntityID id : world.GetComponents<CylinderCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			CylinderCollider* collider = actor->GetComponent<CylinderCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			ColliderInstance instance{};
			instance.position_ = actorPosition;
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Cylinder);
			instance.rotation_ = actorRotation;
			instance.dimensions_ = Vector3(collider->radius_, collider->height_ * 0.5f, 0.0f);
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		for (EntityID id : world.GetComponents<RectCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			RectCollider* collider = actor->GetComponent<RectCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			Vector3 localOffset(collider->center_.x, collider->center_.y, 0.0f);

			ColliderInstance instance{};
			instance.position_ = actorPosition + Vector3::Transform(localOffset, actorRotation);
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Rect);
			instance.rotation_ = actorRotation;
			instance.dimensions_ = Vector3(collider->size_.x * 0.5f, collider->size_.y * 0.5f, 0.0f);
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		for (EntityID id : world.GetComponents<CircleCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			CircleCollider* collider = actor->GetComponent<CircleCollider>();

			Vector3 actorPosition;
			Quaternion actorRotation;
			GetActorTransform(*actor, actorPosition, actorRotation);

			/// [JP] JoltShapePool::CreateCircleShape と同じく、局所形状を X軸
			///      まわりに90度回転してから actor の回転を重ねる — 円の面が
			///      2Dスプライトのように +Z を向く。
			Quaternion faceForwardRotation = Quaternion::CreateFromAxisAngle(Vector3::UnitX, ToRadians(90.0f));
			Vector3 localOffset(collider->center_.x, collider->center_.y, 0.0f);

			ColliderInstance instance{};
			instance.position_ = actorPosition + Vector3::Transform(localOffset, actorRotation);
			instance.shapeKind_ = static_cast<Uint32>(ColliderShapeKind::Circle);
			instance.rotation_ = Quaternion::Concatenate(faceForwardRotation, actorRotation);
			instance.dimensions_ = Vector3(collider->radius_, 0.0f, 0.0f);
			instance.color_ = colliderDebugColor;
			instances.push_back(instance);
		}

		return instances;
	}

	void PhysicsSystem::ResolveMeshColliders(LoaderSystem& loader, ResourceCache& cache, World& world)
	{
		MeshCollisionResource* meshCollisionResource = cache.GetMeshCollisionResource();
		if (!meshCollisionResource)
		{
			return;
		}

		for (EntityID id : world.GetComponents<MeshCollider>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			MeshCollider* collider = actor->GetComponent<MeshCollider>();
			if (!collider || !collider->IsPending())
			{
				continue;
			}

			Handle<MeshCollision> handle = meshCollisionResource->GetHandle(collider->meshID_);
			if (handle.empty())
			{
				continue;
			}

			MeshCollision* meshCollision = meshCollisionResource->Resolve(loader, handle);
			if (!meshCollision)
			{
				continue;
			}

			collider->Build(*meshCollision);
		}
	}
}
