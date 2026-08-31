#pragma once
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	/// [EN] Unique identifier for a component type, implemented as a pointer to that type's static metadata (stable across the process, distinct per type).
	/// [JP] コンポーネント型を一意に識別する識別子。その型の静的メタデータへのポインタとして実装される（プロセス内で安定し、型ごとに異なる値を持つ）。
	using ComponentID = const void*;
}
