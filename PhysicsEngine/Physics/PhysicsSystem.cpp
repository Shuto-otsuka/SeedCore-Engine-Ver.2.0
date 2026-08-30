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
#include <PhysicsEngine/Softbody/Softbody.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionResource.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>

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

	void PhysicsSystem::ApplyActorTransform(const Actor& actor, RigidbodyDesc& desc)
	{
		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();

		desc.position_ = position ? Vector3{ position->x_, position->y_, position->z_ } : Vector3{ 0.0f, 0.0f, 0.0f };
		desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y_), ToRadians(rotation->x_), ToRadians(rotation->z_)) : Quaternion::Identity;
		desc.userData_ = static_cast<EntityID>(actor.GetEntity().GetHandle().index_);
	}

	JPH::BodyID PhysicsSystem::CreateColliderBody(Actor& actor, ShapeHandle shape, Bool isTrigger)
	{
		if (actor.GetComponent<Rigidbody>())
		{
			return JPH::BodyID();
		}

		RigidbodyDesc desc;
		desc.shape_ = shape;
		desc.motionType_ = JPH::EMotionType::Static;
		desc.layer_ = Layers::Pack(Layers::STATIC, actor.GetLayer());
		desc.isSensor_ = isTrigger;
		ApplyActorTransform(actor, desc);

		return actor.GetPhysics().CreateRigidbody(desc);
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

			outPosition = position ? Vector3(position->x_, position->y_, position->z_) : Vector3(0.0f, 0.0f, 0.0f);
			outRotation = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y_), ToRadians(rotation->x_), ToRadians(rotation->z_)) : Quaternion::Identity;
		}

		template<typename DispatchFunction>
		void DispatchToActor(World& world, EntityID entityID, EntityID otherEntityID, DispatchFunction dispatchFunction)
		{
			Actor* actor = world.GetActor(entityID);
			Actor* otherActor = world.GetActor(otherEntityID);
			if (!actor || !otherActor)
			{
				return;
			}

			Entity otherEntity = otherActor->GetEntity();
			for (ComponentID id : actor->ComponentBaseIDList())
			{
				if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
				{
					dispatchFunction(component, otherEntity);
				}
			}
		}
	}

	void PhysicsSystem::DispatchCollisionEnter(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchCollisionEnter(otherEntity); });
	}

	void PhysicsSystem::DispatchCollisionStay(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchCollisionStay(otherEntity); });
	}

	void PhysicsSystem::DispatchCollisionExit(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchCollisionExit(otherEntity); });
	}

	void PhysicsSystem::DispatchTriggerEnter(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchTriggerEnter(otherEntity); });
	}

	void PhysicsSystem::DispatchTriggerStay(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchTriggerStay(otherEntity); });
	}

	void PhysicsSystem::DispatchTriggerExit(World& world, EntityID entityID, EntityID otherEntityID)
	{
		DispatchToActor(world, entityID, otherEntityID, [](ComponentBase* component, Entity otherEntity) { component->DispatchTriggerExit(otherEntity); });
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

	void PhysicsSystem::ResolveSoftbodies(LoaderSystem& loader, ResourceCache& cache, World& world)
	{
		ModelResource* modelResource = cache.GetModelResource();
		if (!modelResource)
		{
			return;
		}

		for (EntityID id : world.GetComponents<Softbody>())
		{
			Actor* actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			Softbody* softbody = actor->GetComponent<Softbody>();
			if (!softbody || !softbody->IsPending())
			{
				continue;
			}

			const Mesh* mesh = actor->GetComponent<Mesh>();
			if (!mesh)
			{
				continue;
			}

			Handle<Crister> handle = modelResource->GetHandle(mesh->meshID_);
			if (handle.empty())
			{
				continue;
			}

			Crister* crister = modelResource->Resolve(loader, handle);
			if (!crister)
			{
				continue;
			}

			softbody->Build(*crister);
		}
	}
}
