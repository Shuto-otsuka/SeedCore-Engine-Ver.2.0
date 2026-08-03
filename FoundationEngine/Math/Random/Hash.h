#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Math/Vector.h>

namespace SeedCore
{
	SEEDCORE_API Uint64 HashCombine(Uint64 seed, Uint64 value)noexcept;

	SEEDCORE_API Uint64 HashFloat(Uint64 seed, Float value)noexcept;

	SEEDCORE_API Uint64 HashVector2(Uint64 seed, const Vector2& value)noexcept;

	SEEDCORE_API Uint64 HashVector3(Uint64 seed, const Vector3& value)noexcept;
}
