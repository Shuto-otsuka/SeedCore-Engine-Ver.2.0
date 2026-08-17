#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class FABRIK
	{
	public:
		static void Solve(DynamicArray<Vector3>& jointPositions, const Vector3& targetPosition, Float tolerance, Int maxIterations);
	};
}
