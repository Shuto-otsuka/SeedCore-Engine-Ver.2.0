#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component holding an entity's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンティティの表示名を保持するコンポーネント。
	*/
	struct Name
	{
		/// [EN] The entity's display name.
		/// [JP] エンティティの表示名。
		String name_;
	};
	REGISTER_COMPONENT(Name, "Core", ComponentStorage::Archetype);
}
