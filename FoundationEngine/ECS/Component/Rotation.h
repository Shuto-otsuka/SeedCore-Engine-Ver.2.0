#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component holding an entity's local-space rotation, in Euler angles.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンティティのローカル空間回転（オイラー角）を保持するコンポーネント。
	*/
	struct Rotation
	{
		/// [EN] Rotation about the X axis.
		/// [JP] X 軸周りの回転。
		SC_REFLECTION_FIELD()
		Float x;

		/// [EN] Rotation about the Y axis.
		/// [JP] Y 軸周りの回転。
		SC_REFLECTION_FIELD()
		Float y;

		/// [EN] Rotation about the Z axis.
		/// [JP] Z 軸周りの回転。
		SC_REFLECTION_FIELD()
		Float z;
	};
	REGISTER_COMPONENT(Rotation, "Core", ComponentStorage::Archetype);
}
