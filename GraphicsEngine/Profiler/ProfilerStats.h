#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct SEEDCORE_API ProfilerStats
	{
		static inline Uint32 drawCallCount_ = 0;

		static void Reset()
		{
			drawCallCount_ = 0;
		}

		static void AddDrawCall()
		{
			++drawCallCount_;
		}
	};
}
