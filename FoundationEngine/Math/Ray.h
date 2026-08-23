#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] A world-space ray - origin_ plus a (not necessarily
	///      normalized) direction_. Returned by ScreenSpace::ScreenToWorld()
	///      for picking; pass origin_/direction_ straight into
	///      Physics::Raycast()/Spherecast().
	/// [JP] ワールド空間のレイ - origin_ と(正規化されているとは限らない)
	///      direction_ の組。ピッキング用に ScreenSpace::ScreenToWorld() が
	///      返す - origin_/direction_ はそのまま Physics::Raycast()/
	///      Spherecast() に渡せる。
	struct Ray
	{
		Vector3 origin_ = Vector3::Zero;

		Vector3 direction_ = Vector3::Zero;
	};
}
