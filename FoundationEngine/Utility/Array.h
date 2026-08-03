#pragma once
#include <FoundationEngine/JobSystem/JobVector.h>

namespace SeedCore
{
	/// [EN] Constraint for element types usable in DynamicArray/StaticArray: must be destructible.
	/// [JP] DynamicArray/StaticArray の要素型に求める制約: 破棄可能であること。
	template<typename T>
	concept DefaultStorable = std::destructible<T>;

	/// [EN] Constraint for element types usable in HybridArray: must be destructible.
	/// [JP] HybridArray の要素型に求める制約: 破棄可能であること。
	template<typename T>
	concept AtomicStorable = std::destructible<T>;

	/// [EN] Project-wide alias for a heap-allocated, resizable array (std::vector).
	/// [JP] ヒープ確保されるサイズ可変配列（std::vector）のプロジェクト共通エイリアス。
	template<DefaultStorable T>
	using DynamicArray = std::vector<T>;

	/// [EN] Project-wide alias for a fixed-size, stack-allocated array (std::array).
	/// [JP] 固定サイズでスタック確保される配列（std::array）のプロジェクト共通エイリアス。
	template<DefaultStorable T, Size N>
		requires (N > 0)
	using StaticArray = std::array<T, N>;

	/// [EN] Project-wide alias for a small-buffer-optimized array (JobVector):
	///      stores up to N elements inline before spilling to the heap.
	/// [JP] 小バッファ最適化された配列（JobVector）のプロジェクト共通エイリアス。
	///      N 個までの要素はインラインに保持し、それを超えるとヒープへ溢れる。
	template<DefaultStorable T, Size N = 2>
		requires (N > 0)
	using HybridArray = JobVector<T, N>;
}