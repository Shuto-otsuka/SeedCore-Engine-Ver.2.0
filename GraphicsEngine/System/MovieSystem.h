#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	struct LoaderSystem;

	class MovieSystem
	{
	public:
		void Update(LoaderSystem& loader, World& world, ResourceCache& resourceCache);
	};
}
