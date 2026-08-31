#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <External/JoltPhysics/Jolt/Physics/Character/CharacterVirtual.h>

namespace SeedCore
{
	class World;

	class JoltCharacterContactListener final :public JPH::CharacterContactListener, public JPH::RefTarget<JoltCharacterContactListener>
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

	public:
		void SetTarget(World* world, EntityID entityID);

	public:
		void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)override;

		void OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)override;

		void OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& bodyID2, const JPH::SubShapeID& subShapeID2)override;

	private:
		World* world_ = nullptr;

		EntityID entityID_;

		std::unordered_map<JPH::BodyID, Bool> sensorCache_;
	};
}
