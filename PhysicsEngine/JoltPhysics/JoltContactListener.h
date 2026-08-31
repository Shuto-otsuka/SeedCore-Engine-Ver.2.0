#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;

	class JoltContactListener final :public JPH::ContactListener
	{
	public:
		enum class ContactEventKind
		{
			Enter,
			Stay,
			Exit,
		};

		struct ContactEvent
		{
			EntityID entityID_;
			EntityID otherEntityID_;
			ContactEventKind kind_ = ContactEventKind::Enter;
			Bool isSensor_ = false;
		};

	public:
		void SetActiveWorld(World* world);

		World* GetActiveWorld()const;

		void DispatchPendingEvents();

	public:
		void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)override;

		void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)override;

		void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)override;

	private:
		struct BodyInfo
		{
			EntityID entityID_;
			Bool isSensor_ = false;
		};

	private:
		void QueueEvent(const JPH::Body& body1, const JPH::Body& body2, ContactEventKind kind);

	private:
		World* world_ = nullptr;

		std::mutex mutex_;

		DynamicArray<ContactEvent> pendingEvents_;

		std::unordered_map<JPH::BodyID, BodyInfo> bodyEntityCache_;
	};
}
