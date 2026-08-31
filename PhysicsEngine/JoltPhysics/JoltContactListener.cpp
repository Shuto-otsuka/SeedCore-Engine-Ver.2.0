#include <PhysicsEngine/JoltPhysics/JoltContactListener.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>

namespace SeedCore
{
	void JoltContactListener::SetActiveWorld(World* world)
	{
		world_ = world;
	}

	World* JoltContactListener::GetActiveWorld()const
	{
		return world_;
	}

	void JoltContactListener::DispatchPendingEvents()
	{
		DynamicArray<ContactEvent> events;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			events.assign(pendingEvents_.begin(), pendingEvents_.end());
			pendingEvents_.clear();
		}

		if (!world_)
		{
			return;
		}

		for (const ContactEvent& event : events)
		{
			switch (event.kind_)
			{
			case ContactEventKind::Enter:
				if (event.isSensor_)
				{
					PhysicsSystem::DispatchTriggerEnter(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchTriggerEnter(*world_, event.otherEntityID_, event.entityID_);
				}
				else
				{
					PhysicsSystem::DispatchCollisionEnter(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchCollisionEnter(*world_, event.otherEntityID_, event.entityID_);
				}
				break;
			case ContactEventKind::Stay:
				if (event.isSensor_)
				{
					PhysicsSystem::DispatchTriggerStay(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchTriggerStay(*world_, event.otherEntityID_, event.entityID_);
				}
				else
				{
					PhysicsSystem::DispatchCollisionStay(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchCollisionStay(*world_, event.otherEntityID_, event.entityID_);
				}
				break;
			case ContactEventKind::Exit:
				if (event.isSensor_)
				{
					PhysicsSystem::DispatchTriggerExit(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchTriggerExit(*world_, event.otherEntityID_, event.entityID_);
				}
				else
				{
					PhysicsSystem::DispatchCollisionExit(*world_, event.entityID_, event.otherEntityID_);
					PhysicsSystem::DispatchCollisionExit(*world_, event.otherEntityID_, event.entityID_);
				}
				break;
			}
		}
	}

	void JoltContactListener::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
	{
		QueueEvent(body1, body2, ContactEventKind::Enter);
	}

	void JoltContactListener::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
	{
		QueueEvent(body1, body2, ContactEventKind::Stay);
	}

	void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		auto it1 = bodyEntityCache_.find(subShapePair.GetBody1ID());
		auto it2 = bodyEntityCache_.find(subShapePair.GetBody2ID());
		if (it1 == bodyEntityCache_.end() || it2 == bodyEntityCache_.end())
		{
			return;
		}

		Bool isSensor = it1->second.isSensor_ || it2->second.isSensor_;
		pendingEvents_.push_back(ContactEvent{ it1->second.entityID_, it2->second.entityID_, ContactEventKind::Exit, isSensor });
	}

	void JoltContactListener::QueueEvent(const JPH::Body& body1, const JPH::Body& body2, ContactEventKind kind)
	{
		BodyInfo info1{ std::bit_cast<EntityID>(static_cast<Uint64>(body1.GetUserData())), body1.IsSensor() };
		BodyInfo info2{ std::bit_cast<EntityID>(static_cast<Uint64>(body2.GetUserData())), body2.IsSensor() };

		std::lock_guard<std::mutex> lock(mutex_);

		bodyEntityCache_[body1.GetID()] = info1;
		bodyEntityCache_[body2.GetID()] = info2;

		pendingEvents_.push_back(ContactEvent{ info1.entityID_, info2.entityID_, kind, info1.isSensor_ || info2.isSensor_ });
	}
}
