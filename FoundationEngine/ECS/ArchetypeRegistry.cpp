#include <FoundationEngine/ECS/ArchetypeRegistry.h>
#include <FoundationEngine/ECS/Archetype.h>

namespace SeedCore
{
	/**
	* [EN]
	* Deletes every Archetype currently held in registry_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* registry_ に現在保持されている全ての Archetype を削除する。
	*/
	ArchetypeRegistry::RegistryCleaner::~RegistryCleaner()
	{
		for (auto& pair : registry_)
		{
			delete pair.second;
		}
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the existing Archetype matching layout (order-independent),
	* or creates and registers a new one if none exists yet.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* layout に一致する（順序に依存しない）既存の Archetype を返す。
	* まだ存在しなければ新しく生成して登録する。
	*/
	Archetype* ArchetypeRegistry::GetOrCreate(const DynamicArray<ComponentID>& layout)
	{
		DynamicArray<ComponentID> sortedLayout = layout;
		std::ranges::sort(sortedLayout);

		/// [EN] Combine every component ID into a single order-independent hash (sortedLayout is sorted, so the same set of components always yields the same hash).
		/// [JP] 全コンポーネント ID を単一の順序非依存なハッシュへ結合する（sortedLayout はソート済みのため、同じコンポーネント集合は常に同じハッシュになる）。
		Size hash = 0;
		for (ComponentID id : sortedLayout)
		{
			hash ^= reinterpret_cast<uintptr_t>(id) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}

		if (registry_.contains(hash))
		{
			Archetype* existing = registry_.at(hash);
			if (existing->Layout() == sortedLayout)
			{
				return existing;
			}
		}

		Archetype* newArchetype = new Archetype(sortedLayout);
		registry_.insert({ hash, newArchetype });

		return newArchetype;
	}
}
