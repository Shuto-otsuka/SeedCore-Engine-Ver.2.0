#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	constexpr Uint32 HaltonJitterSequenceLength = 8;

	[[nodiscard]] constexpr Float HaltonRadicalInverse(Uint32 index, Uint32 base)noexcept
	{
		Float result = 0.0f;
		Float denominator = 1.0f;
		while (index > 0)
		{
			denominator *= static_cast<Float>(base);
			result += static_cast<Float>(index % base) / denominator;
			index /= base;
		}
		return result;
	}

	[[nodiscard]] inline Vector2 Halton23Jitter(Uint32 frameIndex)noexcept
	{
		const Uint32 sampleIndex = (frameIndex % HaltonJitterSequenceLength) + 1;
		const Float x = HaltonRadicalInverse(sampleIndex, 2) - 0.5f;
		const Float y = HaltonRadicalInverse(sampleIndex, 3) - 0.5f;
		return Vector2(x, y);
	}
}
