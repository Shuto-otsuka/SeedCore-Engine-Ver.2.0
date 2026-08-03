#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Unique identifier for an Entity within a World.
	/// [JP] World 内で Entity を一意に識別する識別子。
	using EntityID = Uint32;

	/// [EN] Unique identifier for a component type, implemented as a pointer to that type's static metadata (stable across the process, distinct per type).
	/// [JP] コンポーネント型を一意に識別する識別子。その型の静的メタデータへのポインタとして実装される（プロセス内で安定し、型ごとに異なる値を持つ）。
	using ComponentID = const void*;
}
