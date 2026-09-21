#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	/**
	* [EN]
	* Marker component controlling whether an entity is active (e.g.
	* eligible for ticking/rendering/collision). Presence alone does not
	* imply active — see active_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンティティがアクティブか（tick/描画/衝突などの対象か）を制御する
	* マーカーコンポーネント。付与されているだけではアクティブとは
	* 限らない（active_ を参照）。
	*/
	struct Active
	{
		/// [EN] Whether the entity is currently active.
		/// [JP] エンティティが現在アクティブかどうか。
		Bool active_ = true;
	};
	REGISTER_COMPONENT(Active, "Core", ComponentStorage::Archetype);
}
