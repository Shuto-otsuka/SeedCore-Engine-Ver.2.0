#include <GraphicsEngine/Model/Collision/MeshCollision.h>

namespace SeedCore
{
	const DynamicArray<Vector3>& MeshCollision::Positions()const
	{
		return positions_;
	}

	const DynamicArray<Uint32>& MeshCollision::Indices()const
	{
		return indices_;
	}
}
