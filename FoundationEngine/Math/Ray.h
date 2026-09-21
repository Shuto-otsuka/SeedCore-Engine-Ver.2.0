#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* A world-space ray: origin_ plus a direction_ that need not be normalized.
	* ScreenSpace::ScreenToWorld() returns it for picking; origin_/direction_ go straight into Physics::Raycast()/Spherecast().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ワールド空間のレイ。origin_ と、正規化されているとは限らない direction_ の組。
	* ピッキング用に ScreenSpace::ScreenToWorld() が返し、origin_/direction_ はそのまま Physics::Raycast()/Spherecast() に渡せる。
	*/
	struct Ray
	{
		Vector3 origin_ = Vector3::Zero;

		Vector3 direction_ = Vector3::Zero;
	};
}
