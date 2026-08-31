#include <PhysicsEngine/JoltPhysics/JoltCharacterContactListener.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>

namespace SeedCore
{
	void JoltCharacterContactListener::SetTarget(World* world, EntityID entityID)
	{
		world_ = world;
		entityID_ = entityID;
	}

	void JoltCharacterContactListener::OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)
	{
		if (!world_ || contact.mBodyB.IsInvalid())
		{
			return;
		}

		EntityID otherEntityID = Physics().GetBodyEntityID(contact.mBodyB);
		if (otherEntityID == EntityID{})
		{
			return;
		}

		sensorCache_[contact.mBodyB] = contact.mIsSensorB;

		if (contact.mIsSensorB)
		{
			PhysicsSystem::DispatchTriggerEnter(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchTriggerEnter(*world_, otherEntityID, entityID_);
		}
		else
		{
			PhysicsSystem::DispatchCollisionEnter(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchCollisionEnter(*world_, otherEntityID, entityID_);
		}
	}

	void JoltCharacterContactListener::OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)
	{
		if (!world_ || contact.mBodyB.IsInvalid())
		{
			return;
		}

		EntityID otherEntityID = Physics().GetBodyEntityID(contact.mBodyB);
		if (otherEntityID == EntityID{})
		{
			return;
		}

		sensorCache_[contact.mBodyB] = contact.mIsSensorB;

		if (contact.mIsSensorB)
		{
			PhysicsSystem::DispatchTriggerStay(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchTriggerStay(*world_, otherEntityID, entityID_);
		}
		else
		{
			PhysicsSystem::DispatchCollisionStay(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchCollisionStay(*world_, otherEntityID, entityID_);
		}
	}

	void JoltCharacterContactListener::OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& bodyID2, const JPH::SubShapeID& subShapeID2)
	{
		if (!world_)
		{
			return;
		}

		EntityID otherEntityID = Physics().GetBodyEntityID(bodyID2);
		if (otherEntityID == EntityID{})
		{
			return;
		}

		auto it = sensorCache_.find(bodyID2);
		Bool isSensor = it != sensorCache_.end() && it->second;

		if (isSensor)
		{
			PhysicsSystem::DispatchTriggerExit(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchTriggerExit(*world_, otherEntityID, entityID_);
		}
		else
		{
			PhysicsSystem::DispatchCollisionExit(*world_, entityID_, otherEntityID);
			PhysicsSystem::DispatchCollisionExit(*world_, otherEntityID, entityID_);
		}
	}
}
