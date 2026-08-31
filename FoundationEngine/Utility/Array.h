#pragma once
#include <FoundationEngine/JobSystem/JobVector.h>

namespace SeedCore
{
	/// [EN] Project-wide alias for a heap-allocated, resizable array (std::vector). Behaves exactly like std::vector, including support for incomplete element types (self-referential structs).
	/// [JP] ヒープ確保されるサイズ可変配列（std::vector）のプロジェクト共通エイリアス。不完全な要素型（自己参照構造体）のサポートを含め、std::vector と完全に同じ挙動をする。
	template<typename T>
	using DynamicArray = std::vector<T>;

	/// [EN] Project-wide alias for a fixed-size, stack-allocated array (std::array).
	/// [JP] 固定サイズでスタック確保される配列（std::array）のプロジェクト共通エイリアス。
	template<typename T, Size N>
		requires (N > 0)
	using StaticArray = std::array<T, N>;

	/// [EN] Project-wide alias for a small-buffer-optimized array (JobVector):
	///      stores up to N elements inline before spilling to the heap.
	/// [JP] 小バッファ最適化された配列（JobVector）のプロジェクト共通エイリアス。
	///      N 個までの要素はインラインに保持し、それを超えるとヒープへ溢れる。
	template<typename T, Size N = 2>
		requires (N > 0)
	using HybridArray = JobVector<T, N>;
}
