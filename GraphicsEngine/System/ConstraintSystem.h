#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <unordered_set>

namespace SeedCore
{
	class World;
	class Actor;

	class ConstraintSystem
	{
	public:
		void Execute(World& world);

	private:
		void MarkDirtySubtree(Actor actor, std::unordered_set<EntityID>& dirty);

		void UpdateActor(Actor actor, const Matrix& parentMatrix, World& world, const std::unordered_set<EntityID>& dirty);
	};
}
