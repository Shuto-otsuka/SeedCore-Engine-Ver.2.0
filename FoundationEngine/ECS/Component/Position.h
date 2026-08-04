#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component holding an entity's local-space position.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンティティのローカル空間位置を保持するコンポーネント。
	*/
	struct Position
	{
		/// [EN] X coordinate.
		/// [JP] X 座標。
		SC_SERIALIZE_FIELD()
		Float x;

		/// [EN] Y coordinate.
		/// [JP] Y 座標。
		SC_SERIALIZE_FIELD()
		Float y;

		/// [EN] Z coordinate.
		/// [JP] Z 座標。
		SC_SERIALIZE_FIELD()
		Float z;
	};
	REGISTER_COMPONENT(Position, "Core", ComponentStorage::Archetype);
}
