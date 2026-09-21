#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/World/ECS/Entity/Entity.h>
#include <External/JoltPhysics/Jolt/Physics/Character/CharacterVirtual.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Bridges Jolt virtual-character contacts to SeedCore collision and trigger
	* events for the owning entity and the contacted entity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt 仮想キャラクターの接触を、所有エンティティと接触先エンティティの
	* SeedCore 衝突・トリガーイベントへ橋渡しする。
	*/
	class JoltCharacterContactListener final :public JPH::CharacterContactListener, public JPH::RefTarget<JoltCharacterContactListener>
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

	public:
		/**
		* [EN]
		* Sets the World and entity that receive this listener's contact events.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このリスナーの接触イベントを受け取る World とエンティティを設定する。
		*/
		void SetTarget(World* world, EntityID entityID);

	public:
		/**
		* [EN]
		* Dispatches enter events for a newly added character contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新しく追加されたキャラクター接触の Enter イベントを通知する。
		*/
		void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)override;

		/**
		* [EN]
		* Dispatches stay events for a persisted character contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 継続中のキャラクター接触の Stay イベントを通知する。
		*/
		void OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::CharacterContact& contact, JPH::CharacterContactSettings& settings)override;

		/**
		* [EN]
		* Dispatches exit events for a removed character contact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 削除されたキャラクター接触の Exit イベントを通知する。
		*/
		void OnContactRemoved(const JPH::CharacterVirtual* character, const JPH::BodyID& bodyID2, const JPH::SubShapeID& subShapeID2)override;

	private:
		/// [EN] World that owns the character entity receiving contact events.
		/// [JP] 接触イベントを受け取るキャラクターエンティティを所有する World。
		World* world_ = nullptr;

		/// [EN] Entity represented by the virtual character.
		/// [JP] 仮想キャラクターが表すエンティティ。
		EntityID entityID_;

		/// [EN] Sensor classification retained per body for contact-removal events.
		/// [JP] 接触削除イベント用にボディごとに保持するセンサー区分。
		std::unordered_map<JPH::BodyID, Bool> sensorCache_;
	};
}
