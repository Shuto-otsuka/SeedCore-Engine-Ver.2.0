#include <FoundationEngine/World/ECS/Archetype/ArchetypeRegistry.h>
#include <FoundationEngine/World/ECS/Archetype/Archetype.h>

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
		/// [EN] Archetypes are allocated with new in GetOrCreate and owned by registry_ for the whole process.
		/// [JP] アーキタイプは GetOrCreate で new され、プロセスの終わりまで registry_ が所有する。
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
		/// [EN] Sort so the same set of components always maps to one archetype, whatever order it was given in.
		/// [JP] どの順で渡されても同じコンポーネント集合が1つのアーキタイプになるよう、並べ替える。
		DynamicArray<ComponentID> sortedLayout = layout;
		std::ranges::sort(sortedLayout);

		/// [EN] Combine every component ID into a single order-independent hash (sortedLayout is sorted, so the same set of components always yields the same hash).
		/// [JP] 全コンポーネント ID を単一の順序非依存なハッシュへ結合する（sortedLayout はソート済みのため、同じコンポーネント集合は常に同じハッシュになる）。
		Size hash = 0;
		for (ComponentID id : sortedLayout)
		{
			/// [EN] boost-style hash_combine: 0x9e3779b9 (golden-ratio constant) and the shifts spread each ID's bits across the whole hash.
			/// [JP] boost の hash_combine と同じ式。0x9e3779b9(黄金比由来の定数)とシフトで、各 ID のビットをハッシュ全体に散らす。
			hash ^= reinterpret_cast<uintptr_t>(id) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}

		/// [EN] A hash hit is confirmed by comparing the actual layout, since different sets can share a hash.
		/// [JP] 異なる集合が同じハッシュになりうるので、ハッシュが一致したら実際のレイアウトも比べて確かめる。
		if (registry_.contains(hash))
		{
			/// [EN] The stored layout is sorted too, so a plain element-wise comparison decides it.
			/// [JP] 登録済みのレイアウトもソート済みなので、要素ごとの単純な比較で判定できる。
			Archetype* existing = registry_.at(hash);
			if (existing->Layout() == sortedLayout)
			{
				return existing;
			}
		}

		/// [EN] No match yet: create the archetype and register it under the hash.
		/// [JP] まだ無いので、アーキタイプを作ってハッシュで登録する。
		Archetype* newArchetype = new Archetype(sortedLayout);
		registry_.insert({ hash, newArchetype });

		return newArchetype;
	}
}
