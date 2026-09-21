#include <FoundationEngine/World/ECS/System/MoveSystem.h>
#include <FoundationEngine/World/ECS/Query/Query.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/ECS/Component/Position.h>
#include <FoundationEngine/World/ECS/Component/Velocity.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every active actor with both Position and Velocity by
	* deltaTime, adding Velocity's components onto Position's. Inactive
	* actors keep their Position unchanged.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Position と Velocity の両方を持つアクティブな全 actor を deltaTime
	* 分だけ進める。Velocity の各成分を Position へ加算する。非アクティブ
	* な actor の Position は変えない。
	*/
	void MoveSystem::Execute(World& world, Float deltaTime)
	{
		Query<Read<Velocity>, Write<Position>> query(world);

		query.ForEach([&](EntityID entityID, const Velocity& velocity, Position& position)
			{
				Actor actor = world.GetActor(entityID);
				if (actor && !actor.Active())
				{
					return;
				}

				position.x_ += velocity.x_ * deltaTime;
				position.y_ += velocity.y_ * deltaTime;
				position.z_ += velocity.z_ * deltaTime;
			});
	}
}
