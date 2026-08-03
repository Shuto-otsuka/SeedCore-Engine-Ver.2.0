#include <FoundationEngine/Math/Random/Hash.h>

namespace SeedCore
{
	Uint64 HashCombine(Uint64 seed, Uint64 value)noexcept
	{
		return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2));
	}

	Uint64 HashFloat(Uint64 seed, Float value)noexcept
	{
		Uint32 bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		return HashCombine(seed, bits);
	}

	Uint64 HashVector2(Uint64 seed, const Vector2& value)noexcept
	{
		seed = HashFloat(seed, value.x);
		return HashFloat(seed, value.y);
	}

	Uint64 HashVector3(Uint64 seed, const Vector3& value)noexcept
	{
		seed = HashFloat(seed, value.x);
		seed = HashFloat(seed, value.y);
		return HashFloat(seed, value.z);
	}
}
