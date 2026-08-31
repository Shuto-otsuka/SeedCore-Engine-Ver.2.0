#include <FoundationEngine/ECS/System/LifetimeSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/CommandBuffer.h>
#include <FoundationEngine/ECS/Component/Lifetime.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every Lifetime component's countdown by deltaTime and
	* records, on cmd, the destruction of any actor whose remaining time
	* has reached zero. A holding actor seen for the first time starts
	* its countdown at Lifetime::duration_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全 Lifetime コンポーネントのカウントダウンを deltaTime だけ進め、
	* 残り時間が 0 に達したアクターの破棄を cmd へ記録する。初めて
	* 見つかった保持アクターは Lifetime::duration_ からカウントダウンを
	* 開始する。
	*/
	void LifetimeSystem::Execute(CommandBuffer& cmd, World& world, Float deltaTime)
	{
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
				cmd.DestroyEntity(entityID);
				remaining_.erase(entityID);
			}
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
