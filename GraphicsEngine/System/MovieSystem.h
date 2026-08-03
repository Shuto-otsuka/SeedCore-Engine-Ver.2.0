#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	class ResourceCache;

	class MovieSystem
	{
	public:
		void Update(World& world, ResourceCache& resourceCache);
	};
}
