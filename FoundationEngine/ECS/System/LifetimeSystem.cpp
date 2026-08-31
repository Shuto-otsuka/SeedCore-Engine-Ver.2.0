#include <FoundationEngine/ECS/System/LifetimeSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Lifetime.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every Lifetime component's countdown by deltaTime and
	* destroys any actor whose remaining time has reached zero. A
	* holding actor seen for the first time starts its countdown at
	* Lifetime::duration_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Lifetime コンポーネントのカウントダウンを deltaTime だけ進め、
	* 残り時間が 0 に達したアクターを破棄する。初めて見つかった保持
	* アクターは Lifetime::duration_ からカウントダウンを開始する。
	*/
	void LifetimeSystem::Update(World& world, Float deltaTime)
	{
		/// [EN] Collect expired entities during the scan and destroy them afterward: DestroyActor mutates the actor list, which must not happen while iterating the component set.
		/// [JP] 走査中は期限切れエンティティを集めるだけにして後で破棄する: DestroyActor はアクター一覧を変更するため、コンポーネント集合の反復中に呼んではならない。
		DynamicArray<EntityID> expired;

		for (EntityID entityID : world.GetComponents<Lifetime>())
		{
			Actor actor = world.GetActor(entityID);
			if (!actor)
			{
				continue;
			}

			Lifetime* lifetime = world.GetComponent<Lifetime>(actor.GetEntity());
			if (lifetime == nullptr)
			{
				continue;
			}

			/// [EN] First time this entity is seen: seed its countdown from the configured duration.
			/// [JP] このエンティティを初めて見たとき: 設定された生存時間からカウントダウンを開始する。
			if (!remaining_.contains(entityID))
			{
				remaining_[entityID] = lifetime->duration_;
			}

			Float& remaining = remaining_[entityID];
			remaining -= deltaTime;
			if (remaining <= 0.0f)
			{
				expired.push_back(entityID);
			}
		}

		for (EntityID entityID : expired)
		{
			Actor actor = world.GetActor(entityID);
			if (actor)
			{
				world.DestroyActor(actor);
			}
			remaining_.erase(entityID);
		}
	}

	/**
	* [EN]
	* Forgets every Lifetime's countdown without touching the actual
	* Actors.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Lifetime のカウントダウンを、実際の Actor には触れずに忘れる。
	*/
	void LifetimeSystem::Reset()
	{
		remaining_.clear();
	}
}
