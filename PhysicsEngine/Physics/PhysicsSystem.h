#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Interop/ColliderInstance.h>
#include <PhysicsEngine/Physics/Physics.h>

namespace SeedCore
{
	class Actor;
	class Rigidbody;
	class World;
	class ResourceCache;
	struct LoaderSystem;

	class SEEDCORE_API PhysicsSystem
	{
	public:
		static ShapeHandle FindColliderShape(Actor actor);

		static JPH::EAllowedDOFs ToAllowedDOFs(const Rigidbody& rigidbody);

		static void ApplyActorTransform(Actor actor, RigidbodyDesc& desc);

		static JPH::BodyID CreateColliderBody(Actor actor, ShapeHandle shape, Bool isTrigger = false);

		static void DestroyColliderBody(Actor actor, JPH::BodyID bodyID, ShapeHandle shape);

		static void DispatchCollisionEnter(World& world, EntityID entityID, EntityID otherEntityID);

		static void DispatchCollisionStay(World& world, EntityID entityID, EntityID otherEntityID);

		static void DispatchCollisionExit(World& world, EntityID entityID, EntityID otherEntityID);

		static void DispatchTriggerEnter(World& world, EntityID entityID, EntityID otherEntityID);

		static void DispatchTriggerStay(World& world, EntityID entityID, EntityID otherEntityID);

		static void DispatchTriggerExit(World& world, EntityID entityID, EntityID otherEntityID);

		static DynamicArray<ColliderInstance> GatherColliderInstances(World& world);

		static void ResolveMeshColliders(LoaderSystem& loader, ResourceCache& cache, World& world);

		static void ResolveSoftbodies(LoaderSystem& loader, ResourceCache& cache, World& world);

	private:
		static constexpr Float defaultColliderShapeRadius = 0.5f;
	};
}
