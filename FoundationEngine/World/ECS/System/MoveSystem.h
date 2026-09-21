#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;

	/**
	* [EN]
	* Built-in system integrating every Velocity-holding actor's Position
	* by deltaTime each frame (Position += Velocity * deltaTime).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Velocity を持つ全 actor の Position を、毎フレーム deltaTime 分だけ
	* 積分する組み込みシステム(Position += Velocity * deltaTime)。
	*/
	class MoveSystem
	{
	public:
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
		void Execute(World& world, Float deltaTime);
	};
}
