#include <PhysicsEngine/JoltPhysics/JoltContactListener.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Sets the World that receives dispatched contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 接触イベントの通知先となる World を設定する。
	*/
	void JoltContactListener::ActiveWorld(World* world)
	{
		world_ = world;
	}

	/**
	* [EN]
	* Returns the World that receives dispatched contact events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 接触イベントの通知先となる World を返す。
	*/
	World* JoltContactListener::ActiveWorld()const
	{
		return world_;
	}

	/**
	* [EN]
	* Drains queued contacts and dispatches their collision or trigger events.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キュー内の接触を取り出し、衝突またはトリガーイベントを通知する。
	*/
	void JoltContactListener::DispatchEvent()
	{
		/// [EN] Move pending events to local storage while holding the callback mutex.
		/// [JP] コールバック用ミューテックスの保持中に、保留イベントをローカルへ移す。
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

		/// [EN] Dispatch each phase symmetrically to both participating entities.
		/// [JP] 各段階のイベントを、参加する両方のエンティティへ対称に通知する。
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

	/**
	* [EN]
	* Queues an enter event for a newly added body contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 新しく追加されたボディ接触の Enter イベントをキューへ積む。
	*/
	void JoltContactListener::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
	{
		/// [EN] Capture body metadata before entering the synchronized queue section.
		/// [JP] 同期されたキュー区間へ入る前に、ボディのメタデータを取得する。
		BodyInfo info1{ std::bit_cast<EntityID>(static_cast<Uint64>(body1.GetUserData())), body1.IsSensor() };
		BodyInfo info2{ std::bit_cast<EntityID>(static_cast<Uint64>(body2.GetUserData())), body2.IsSensor() };

		std::lock_guard<std::mutex> lock(mutex_);

		bodyEntityCache_[body1.GetID()] = info1;
		bodyEntityCache_[body2.GetID()] = info2;

		pendingEvents_.push_back(ContactEvent{ info1.entityID_, info2.entityID_, ContactEventKind::Enter, info1.isSensor_ || info2.isSensor_ });
	}

	/**
	* [EN]
	* Queues a stay event for a persisted body contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 継続中のボディ接触の Stay イベントをキューへ積む。
	*/
	void JoltContactListener::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
	{
		/// [EN] Refresh cached body metadata and queue the continued contact atomically.
		/// [JP] ボディ情報のキャッシュ更新と継続接触の追加を一括して行う。
		BodyInfo info1{ std::bit_cast<EntityID>(static_cast<Uint64>(body1.GetUserData())), body1.IsSensor() };
		BodyInfo info2{ std::bit_cast<EntityID>(static_cast<Uint64>(body2.GetUserData())), body2.IsSensor() };

		std::lock_guard<std::mutex> lock(mutex_);

		bodyEntityCache_[body1.GetID()] = info1;
		bodyEntityCache_[body2.GetID()] = info2;

		pendingEvents_.push_back(ContactEvent{ info1.entityID_, info2.entityID_, ContactEventKind::Stay, info1.isSensor_ || info2.isSensor_ });
	}

	/**
	* [EN]
	* Queues an exit event for a removed body contact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 削除されたボディ接触の Exit イベントをキューへ積む。
	*/
	void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		/// [EN] Recover entity and sensor data cached while the contact was active.
		/// [JP] 接触中にキャッシュしたエンティティ・センサーデータを取得する。
		auto it1 = bodyEntityCache_.find(subShapePair.GetBody1ID());
		auto it2 = bodyEntityCache_.find(subShapePair.GetBody2ID());
		if (it1 == bodyEntityCache_.end() || it2 == bodyEntityCache_.end())
		{
			return;
		}

		Bool isSensor = it1->second.isSensor_ || it2->second.isSensor_;
		pendingEvents_.push_back(ContactEvent{ it1->second.entityID_, it2->second.entityID_, ContactEventKind::Exit, isSensor });
	}
}
