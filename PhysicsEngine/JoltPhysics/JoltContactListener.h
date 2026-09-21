#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Collects Jolt body-contact callbacks and dispatches SeedCore collision
	* and trigger events on the active World.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt のボディ接触コールバックを収集し、アクティブな World へ
	* SeedCore の衝突・トリガーイベントを通知する。
	*/
	class JoltContactListener final :public JPH::ContactListener
	{
	public:
		/**
		* [EN]
		* Identifies the lifecycle phase of a queued contact event.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キューへ積む接触イベントのライフサイクル段階を表す。
		*/
		enum class ContactEventKind
		{
			/// [EN] Contact began during the physics update.
			/// [JP] 物理更新中に接触が始まった。
			Enter,

			/// [EN] Contact continued during the physics update.
			/// [JP] 物理更新中に接触が継続した。
			Stay,

			/// [EN] Contact ended during the physics update.
			/// [JP] 物理更新中に接触が終了した。
			Exit,
		};

		/**
		* [EN]
		* Deferred contact notification transferred from physics callbacks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理コールバックから引き渡される遅延接触通知。
		*/
		struct ContactEvent
		{
			/// [EN] Entity represented by the first body.
			/// [JP] 1つ目のボディが表すエンティティ。
			EntityID entityID_;

			/// [EN] Entity represented by the second body.
			/// [JP] 2つ目のボディが表すエンティティ。
			EntityID otherEntityID_;

			/// [EN] Lifecycle phase dispatched for this contact.
			/// [JP] この接触で通知するライフサイクル段階。
			ContactEventKind kind_ = ContactEventKind::Enter;

			/// [EN] Whether either contacted body is a sensor.
			/// [JP] 接触したボディのいずれかがセンサーか。
			Bool isSensor_ = false;
		};

	public:
		/**
		* [EN]
		* Sets the World that receives dispatched contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接触イベントの通知先となる World を設定する。
		*/
		void ActiveWorld(World* world);

		/**
		* [EN]
		* Returns the World that receives dispatched contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接触イベントの通知先となる World を返す。
		*/
		World* ActiveWorld()const;

		/**
		* [EN]
		* Drains queued contacts and dispatches their collision or trigger events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キュー内の接触を取り出し、衝突またはトリガーイベントを通知する。
		*/
		void DispatchEvent();

	public:
		/**
		* [EN]
		* Queues an enter event for a newly added body contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新しく追加されたボディ接触の Enter イベントをキューへ積む。
		*/
		void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)override;

		/**
		* [EN]
		* Queues a stay event for a persisted body contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 継続中のボディ接触の Stay イベントをキューへ積む。
		*/
		void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)override;

		/**
		* [EN]
		* Queues an exit event for a removed body contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 削除されたボディ接触の Exit イベントをキューへ積む。
		*/
		void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)override;

	private:
		/**
		* [EN]
		* Cached entity and sensor data associated with one Jolt body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つの Jolt ボディに対応するキャッシュ済みエンティティ・センサーデータ。
		*/
		struct BodyInfo
		{
			/// [EN] Entity represented by the body.
			/// [JP] ボディが表すエンティティ。
			EntityID entityID_;

			/// [EN] Whether the body is a sensor.
			/// [JP] ボディがセンサーか。
			Bool isSensor_ = false;
		};

		/// [EN] World that receives deferred contact events.
		/// [JP] 遅延接触イベントを受け取る World。
		World* world_ = nullptr;

		/// [EN] Synchronizes callback writes with event dispatch.
		/// [JP] コールバックによる書き込みとイベント通知を同期する。
		std::mutex mutex_;

		/// [EN] Contact events waiting for dispatch on the active World.
		/// [JP] アクティブな World への通知を待つ接触イベント。
		DynamicArray<ContactEvent> pendingEvents_;

		/// [EN] Body data retained for removal callbacks that provide only IDs.
		/// [JP] ID だけを渡す削除コールバック用に保持するボディデータ。
		std::unordered_map<JPH::BodyID, BodyInfo> bodyEntityCache_;
	};
}
