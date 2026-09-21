#include <PhysicsEngine/JoltPhysics/JoltCharacterContactListener.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Sets the World and entity that receive this listener's contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このリスナーの接触イベントを受け取る World とエンティティを設定する。
	*/
	void JoltCharacterContactListener::SetTarget(World* world, EntityID entityID)
	{
		world_ = world;
		entityID_ = entityID;
	}

	/**
	* [EN]
	* Dispatches enter events for a newly added character contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 新しく追加されたキャラクター接触の Enter イベントを通知する。
	*/
	void JoltCharacterContactListener::OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)
	{
		if (!world_ || contact.mBodyB.IsInvalid())
		{
			return;
		}

		/// [EN] Resolve the contacted body to an entity and retain its sensor classification.
		/// [JP] 接触先ボディをエンティティへ解決し、センサー区分を保持する。
		EntityID otherEntityID = Physics().BodyEntityID(contact.mBodyB);
		if (otherEntityID == EntityID{})
		{
			return;
		}

		sensorCache_[contact.mBodyB] = contact.mIsSensorB;

		/// [EN] Notify both entities with the event type selected by the contacted body.
		/// [JP] 接触先ボディの種別に応じたイベントを両方のエンティティへ通知する。
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

	/**
	* [EN]
	* Dispatches stay events for a persisted character contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 継続中のキャラクター接触の Stay イベントを通知する。
	*/
	void JoltCharacterContactListener::OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)
	{
		if (!world_ || contact.mBodyB.IsInvalid())
		{
			return;
		}

		/// [EN] Refresh the sensor classification and notify both entities of the ongoing contact.
		/// [JP] センサー区分を更新し、継続中の接触を両方のエンティティへ通知する。
		EntityID otherEntityID = Physics().BodyEntityID(contact.mBodyB);
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

	/**
	* [EN]
	* Dispatches exit events for a removed character contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 削除されたキャラクター接触の Exit イベントを通知する。
	*/
	void JoltCharacterContactListener::OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& bodyID2, const JPH::SubShapeID& subShapeID2)
	{
		if (!world_)
		{
			return;
		}

		EntityID otherEntityID = Physics().BodyEntityID(bodyID2);
		if (otherEntityID == EntityID{})
		{
			return;
		}

		/// [EN] Use the cached classification because the removal callback supplies only body IDs.
		/// [JP] 削除コールバックではボディ ID だけが渡されるため、保持した区分を使う。
		auto it = sensorCache_.find(bodyID2);

		if (it != sensorCache_.end() && it->second)
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
