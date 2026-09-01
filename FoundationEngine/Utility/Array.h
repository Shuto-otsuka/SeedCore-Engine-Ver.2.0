#pragma once
#include <FoundationEngine/Utility/DynamicArray.h>
#include <FoundationEngine/Utility/StaticArray.h>
#include <FoundationEngine/JobSystem/JobVector.h>

namespace SeedCore
{
	/// [EN] Project-wide small-buffer-optimized array (JobVector): stores up to N elements inline before spilling to the heap.
	/// [JP] 小バッファ最適化された配列（JobVector）のプロジェクト共通エイリアス。N 個までの要素はインラインに保持し、それを超えるとヒープへ溢れる。
	template<typename T, Size N = 2>
		requires (N > 0)
	using HybridArray = JobVector<T, N>;
}
