#include <FoundationEngine/ECS/System/MoveSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Velocity.h>

namespace SeedCore
{
	/**
	* [EN]
	* Advances every actor with both Position and Velocity by deltaTime,
	* adding Velocity's components onto Position's.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Position と Velocity の両方を持つ全 actor を deltaTime 分だけ進める。
	* Velocity の各成分を Position へ加算する。
	*/
	void MoveSystem::Execute(World& world, Float deltaTime)
	{
		Query<Read<Velocity>, Write<Position>> query(world);

		query.ForEach([&](const Velocity& velocity, Position& position)
			{
				position.x_ += velocity.x_ * deltaTime;
				position.y_ += velocity.y_ * deltaTime;
				position.z_ += velocity.z_ * deltaTime;
			});
	}
}
