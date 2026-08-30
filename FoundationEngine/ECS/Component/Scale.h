#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component holding an entity's local-space scale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンティティのローカル空間スケールを保持するコンポーネント。
	*/
	struct Scale
	{
		/// [EN] Scale along the X axis.
		/// [JP] X 軸方向のスケール。
		SC_SERIALIZE_FIELD()
		Float x_;

		/// [EN] Scale along the Y axis.
		/// [JP] Y 軸方向のスケール。
		SC_SERIALIZE_FIELD()
		Float y_;

		/// [EN] Scale along the Z axis.
		/// [JP] Z 軸方向のスケール。
		SC_SERIALIZE_FIELD()
		Float z_;
	};
	REGISTER_COMPONENT(Scale, "Core", ComponentStorage::Archetype);
}
