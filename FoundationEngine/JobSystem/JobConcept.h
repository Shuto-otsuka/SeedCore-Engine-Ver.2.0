#pragma once
#include <FoundationEngine/Utility/IndexRange.h>

namespace SeedCore
{
	/// [EN] Satisfied by any type convertible to String (e.g. task/node names).
	/// [JP] String に変換可能な任意の型（タスク/ノード名など）を満たす。
	template<typename T>
	concept StringLike = std::convertible_to<T, String>;

	/// [EN] Satisfied by an IndexRange of any rank, ignoring reference/const
	///      qualification (e.g. std::reference_wrapper).
	/// [JP] 参照/const修飾（std::reference_wrapper など）を無視して、
	///      任意のランクの IndexRange を満たす。
	template<typename R>
	concept IndexRangeLike = IsIndexRangeValue<std::decay_t<std::unwrap_ref_decay_t<R>>>;

	/// [EN] Satisfied specifically by a rank-1 IndexRange, ignoring
	///      reference/const qualification.
	/// [JP] 参照/const修飾を無視して、特にランク1の IndexRange を満たす。
	template<typename R>
	concept IndexRange1DLike = Is1DIndexRangeValue<std::decay_t<std::unwrap_ref_decay_t<R>>>;

	/// [EN] Satisfied by an IndexRange with more than one dimension,
	///      ignoring reference/const qualification.
	/// [JP] 参照/const修飾を無視して、2次元以上の IndexRange を満たす。
	template<typename R>
	concept IndexRangeMultiDimensionalLike = IsMultiDimensionalIndexRangeValue<std::decay_t<std::unwrap_ref_decay_t<R>>>;
}