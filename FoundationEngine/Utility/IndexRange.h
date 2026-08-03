#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns whether [begin, end) with step is a malformed range:
	* a zero step with begin != end, or a step whose sign doesn't move
	* begin toward end.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* [begin, end) と step が不正な範囲かどうかを返す。
	* ステップが0で begin != end の場合、またはステップの符号が
	* begin を end に近づけない方向の場合に不正となる。
	*/
	template<std::integral T>
	constexpr Bool IsIndexRangeInvalid(T begin, T end, T step)
	{
		return ((step == T{ 0 } && begin != end) || (begin < end && step <= T{ 0 }) || (begin > end && step >= T{ 0 }));
	}

	/**
	* [EN]
	* Returns the number of steps (elements) in [begin, end) advancing
	* by step, rounding away from zero so a partial final step still
	* counts (ceiling division).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* [begin, end) を step ずつ進むときのステップ数（要素数）を返す。
	* 端数の最終ステップも数えられるよう、ゼロから遠ざかる方向に丸める
	* （切り上げ除算）。
	*/
	template<std::integral T>
	constexpr Size Distance(T begin, T end, T step)
	{
		if constexpr (std::is_unsigned_v<T>)
		{
			return static_cast<Size>((end - begin + step - T{ 1 }) / step);
		}
		else
		{
			return static_cast<Size>((end - begin + step + (step > T{ 0 } ? T{ -1 } : T{ 1 })) / step);
		}
	}

	/**
	* [EN]
	* An N-dimensional index range: N independent IndexRange<T, 1>
	* axes, addressable both per-axis and as a single flattened index
	* space. Used to describe multi-dimensional iteration/chunking
	* domains (e.g. for parallel-for style workloads) without materializing
	* the indices.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* N 次元のインデックス範囲。N 個の独立した IndexRange<T, 1> 軸を
	* 持ち、軸ごとにも、1本のフラット化されたインデックス空間としても
	* アドレス指定できる。並列forのようなワークロード向けに、実際の
	* インデックスを展開せずに多次元の反復/分割領域を表現するために使う。
	*/
	template<std::integral T, Size N = 1>
	class IndexRange
	{
	public:
		/// [EN] The integral index type used by every axis.
		/// [JP] 各軸で使われる整数のインデックス型。
		using IndexType = T;

		/// [EN] Number of dimensions (N).
		/// [JP] 次元数（N）。
		static constexpr Size rank_ = N;

	public:
		IndexRange() = default;

		/**
		* [EN]
		* Constructs from exactly N per-axis IndexRange<T, 1> values.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ちょうど N 個の各軸の IndexRange<T, 1> から構築する。
		*/
		template<typename... Ranges>
			requires(sizeof...(Ranges) == N) && (std::same_as<std::remove_cvref_t<Ranges>, IndexRange<T, 1>>&&...)
		explicit IndexRange(Ranges&&... ranges) :dimensions_(std::forward<Ranges>(ranges)...)
		{
			/// No Code
		}

		/**
		* [EN]
		* Constructs from an array of N per-axis IndexRange<T, 1> values.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* N 個の各軸の IndexRange<T, 1> からなる配列から構築する。
		*/
		explicit IndexRange(const StaticArray<IndexRange<T, 1>, N>& dimensions) :dimensions_(dimensions)
		{
			/// No Code
		}

		/**
		* [EN]
		* Returns the IndexRange<T, 1> for the given axis (deducing-this
		* overload, so it forwards const/ref-qualification from the caller).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した軸の IndexRange<T, 1> を返す（deducing-this により、
		* 呼び出し元の const/参照修飾をそのまま転送する）。
		*/
		template <typename Self>
		auto& dimension(this Self&& self, Size dimension)
		{
			return std::forward<Self>(self).dimensions_[dimension];
		}

		/**
		* [EN]
		* Returns all per-axis ranges.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全軸の範囲を返す。
		*/
		const StaticArray<IndexRange<T, 1>, N>& dimensions()const
		{
			return dimensions_;
		}

		/**
		* [EN]
		* Returns the element count of a single axis.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単一軸の要素数を返す。
		*/
		Size size(Size dimension)const
		{
			return dimensions_[dimension].size();
		}

		/**
		* [EN]
		* Returns the total flattened element count (the product of every
		* axis's size). If an inner axis (index > 0) is empty, its size is
		* skipped rather than zeroing the whole product; if the outermost
		* axis (index 0) is empty, the total is 0.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フラット化した総要素数（全軸のサイズの積）を返す。内側の軸
		* （インデックス > 0）が空の場合、その軸のサイズはスキップされ、
		* 積全体をゼロにはしない。最も外側の軸（インデックス 0）が空の
		* 場合は合計が 0 になる。
		*/
		Size size()const
		{
			Size total = 1;
			for (Size dimension = 0;dimension < N;++dimension)
			{
				Size size = dimensions_[dimension].size();
				if (size == 0)
				{
					return dimension == 0 ? 0 : total;
				}
				total *= size;
			}
			return total;
		}

		/**
		* [EN]
		* Carves out a contiguous "box" of roughly chunkSize flattened
		* elements starting at flat index flatBegin, by growing from the
		* innermost axis outward until at least chunkSize elements are
		* covered ("ceil": the resulting chunk may be larger than
		* chunkSize, e.g. when a single step of the growing axis already
		* overshoots it). Returns the resulting sub-IndexRange and how
		* many flattened elements it actually covers. Used to split
		* multi-dimensional work into chunks of roughly-even size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フラットインデックス flatBegin から始まる、おおよそ chunkSize
		* 個のフラット化要素からなる連続した「box」を切り出す。最も内側の
		* 軸から外側へ向かって、少なくとも chunkSize 個を覆うまで
		* 拡張していく（「ceil」: 拡張中の軸の1ステップだけで既に超過する
		* 場合など、結果のチャンクは chunkSize より大きくなり得る）。
		* 結果の部分 IndexRange と、実際に覆っているフラット要素数を返す。
		* 多次元の作業を、おおよそ均等なサイズのチャンクに分割するために使う。
		*/
		std::pair<IndexRange<T, N>, Size> slice_ceil(Size flatBegin, Size chunkSize)const
		{
			if (chunkSize == 0)
			{
				return{ *this,0 };
			}

			Size dimensionSizes[N];
			Size dimensionActive = N;
			for (Size dimension = 0;dimension < N;++dimension)
			{
				dimensionSizes[dimension] = dimensions_[dimension].size();
				if (dimensionSizes[dimension] == 0)
				{
					dimensionActive = dimension;
					break;
				}
			}

			if (dimensionActive == 0)
			{
				return { *this,0 };
			}

			Size coords[N] = {};
			Size temporary = flatBegin;
			for (Size dimension = dimensionActive;dimension-- > 0;)
			{
				coords[dimension] = temporary % dimensionSizes[dimension];
				temporary /= dimensionSizes[dimension];
			}

			Size growDimension = dimensionActive - 1;
			Size innerVolume = 1;
			Size activeInnerVolume = 1;
			for (Size dimension = dimensionActive;dimension-- > 0;)
			{
				if (dimension + 1 < dimensionActive && coords[dimension + 1] != 0)
				{
					break;
				}

				growDimension = dimension;
				activeInnerVolume = innerVolume;

				if (innerVolume >= chunkSize)
				{
					break;
				}

				innerVolume *= dimensionSizes[dimension];
			}

			Size stepsLeft = dimensionSizes[growDimension] - coords[growDimension];
			Size stepsNeeded = Max(Size{ 1 }, chunkSize / activeInnerVolume);
			Size stepsToTake = Min(stepsLeft, stepsNeeded);

			StaticArray<IndexRange<T, 1>, N> boxDimensions;

			for (Size dimension = 0;dimension < growDimension;++dimension)
			{
				const T begin = dimensions_[dimension].begin();
				const T step = dimensions_[dimension].step_size();
				boxDimensions[dimension] = IndexRange<T, 1>(begin + static_cast<T>(coords[dimension]) * step, begin + static_cast<T>(coords[dimension] + 1) * step, step);
			}

			{
				const T begin = dimensions_[growDimension].begin();
				const T step = dimensions_[growDimension].step_size();
				boxDimensions[growDimension] = IndexRange<T, 1>(begin + static_cast<T>(coords[growDimension]) * step, begin + static_cast<T>(coords[growDimension] + stepsToTake) * step, step);
			}

			for (Size dimension = growDimension + 1;dimension < N;++dimension)
			{
				boxDimensions[dimension] = dimensions_[dimension];
			}

			return { IndexRange<T,N>(boxDimensions),stepsToTake * activeInnerVolume };
		}

		/**
		* [EN]
		* Same purpose as slice_ceil, but grows the chunk without ever
		* exceeding chunkSize ("floor": the resulting chunk may be
		* smaller than chunkSize when it doesn't divide evenly). Returns
		* the resulting sub-IndexRange and how many flattened elements it
		* actually covers.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* slice_ceil と同じ目的だが、chunkSize を超えないようにチャンクを
		* 拡張する（「floor」: 割り切れない場合、結果のチャンクは
		* chunkSize より小さくなり得る）。結果の部分 IndexRange と、
		* 実際に覆っているフラット要素数を返す。
		*/
		std::pair<IndexRange<T, N>, Size> slice_floor(Size flatBegin, Size chunkSize)const
		{
			if (chunkSize == 0)
			{
				return { *this, 0 };
			}

			Size dimensionSizes[N];
			Size dimensionActive = N;
			for (Size dimension = 0; dimension < N; ++dimension)
			{
				dimensionSizes[dimension] = dimensions_[dimension].size();
				if (dimensionSizes[dimension] == 0)
				{
					dimensionActive = dimension;
					break;
				}
			}

			if (dimensionActive == 0)
			{
				return { *this, 0 };
			}

			Size coords[N] = {};
			Size temporary = flatBegin;
			for (Size dimension = dimensionActive; dimension-- > 0; )
			{
				coords[dimension] = temporary % dimensionSizes[dimension];
				temporary /= dimensionSizes[dimension];
			}

			Size growDimension = dimensionActive - 1;
			Size innerVolume = 1;
			Size activeInnerVolume = 1;

			for (Size dimension = dimensionActive; dimension-- > 0; )
			{
				if (dimension + 1 < dimensionActive && coords[dimension + 1] != 0)
				{
					break;
				}

				Size nextVolume = innerVolume * dimensionSizes[dimension];
				if (nextVolume > chunkSize && innerVolume > 1)
				{
					break;
				}

				growDimension = dimension;
				activeInnerVolume = innerVolume;
				innerVolume = nextVolume;
			}

			Size stepsLeft = dimensionSizes[growDimension] - coords[growDimension];
			Size stepsNeeded = Max(Size{ 1 }, chunkSize / activeInnerVolume);
			Size stepsToTake = Min(stepsLeft, stepsNeeded);

			StaticArray<IndexRange<T, 1>, N> boxDimensions;

			for (Size dimension = 0; dimension < growDimension; ++dimension)
			{
				const T begin = dimensions_[dimension].begin();
				const T step = dimensions_[dimension].step_size();
				boxDimensions[dimension] = IndexRange<T, 1>(begin + static_cast<T>(coords[dimension]) * step, begin + static_cast<T>(coords[dimension] + 1) * step, step);
			}

			{
				const T begin = dimensions_[growDimension].begin();
				const T step = dimensions_[growDimension].step_size();
				boxDimensions[growDimension] = IndexRange<T, 1>(begin + static_cast<T>(coords[growDimension]) * step, begin + static_cast<T>(coords[growDimension] + stepsToTake) * step, step);
			}

			for (Size dimension = growDimension + 1; dimension < N; ++dimension)
			{
				boxDimensions[dimension] = dimensions_[dimension];
			}

			return { IndexRange<T, N>(boxDimensions), stepsToTake * activeInnerVolume };
		}

		/**
		* [EN]
		* Rounds chunkSize up to the nearest inner-volume boundary
		* achievable by growing axes from the innermost outward (the
		* smallest such volume that is >= chunkSize, or the full inner
		* volume if none is large enough).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* chunkSize を、最も内側の軸から外側へ拡張して到達可能な
		* 内部ボリュームの境界のうち、直近上位のものに切り上げる
		* （chunkSize 以上となる最小のボリューム。十分な大きさのものが
		* なければ内部ボリューム全体）。
		*/
		Size ceil(Size chunkSize)const
		{
			Size dimensionSizes[N];
			Size dimensionActive = N;
			for (Size dimension = 0;dimension < N;++dimension)
			{
				dimensionSizes[dimension] = dimensions_[dimension].size();
				if (dimensionSizes[dimension] == 0)
				{
					dimensionActive = dimension;
					break;
				}
			}

			if (dimensionActive == 0)
			{
				return 0;
			}

			Size innerVolume = 1;
			if (innerVolume >= chunkSize)
			{
				return innerVolume;
			}

			for (Size dimension = dimensionActive;dimension-- > 0;)
			{
				innerVolume *= dimensionSizes[dimension];
				if (innerVolume >= chunkSize)
				{
					return innerVolume;
				}
			}
			return innerVolume;
		}

		/**
		* [EN]
		* Rounds chunkSize down to the nearest inner-volume boundary
		* achievable by growing axes from the innermost outward (the
		* largest such volume that is <= chunkSize).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* chunkSize を、最も内側の軸から外側へ拡張して到達可能な
		* 内部ボリュームの境界のうち、直近下位のものに切り下げる
		* （chunkSize 以下となる最大のボリューム）。
		*/
		Size floor(Size chunkSize)const
		{
			Size dimensionSizes[N];
			Size dimensionActive = N;
			for (Size dimension = 0;dimension < N;++dimension)
			{
				dimensionSizes[dimension] = dimensions_[dimension].size();
				if (dimensionSizes[dimension] == 0)
				{
					dimensionActive = dimension;
					break;
				}
			}

			if (dimensionActive == 0)
			{
				return 0;
			}

			Size innerVolume = 1;
			Size lastFit = 1;
			for (Size dimension = dimensionActive;dimension-- > 0;)
			{
				Size next = innerVolume * dimensionSizes[dimension];
				if (next > chunkSize)
				{
					break;
				}
				innerVolume = next;
				lastFit = innerVolume;
			}
			return lastFit;
		}

	private:
		/// [EN] The N per-axis ranges.
		/// [JP] N 個の各軸の範囲。
		StaticArray<IndexRange<T, 1>, N> dimensions_;
	};

	/**
	* [EN]
	* Single-axis (rank-1) specialization of IndexRange: a plain
	* [begin, end) range stepped by stepSize. Also the leaf type that
	* every axis of the N-dimensional IndexRange<T, N> is built from.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* IndexRange の単一軸（ランク1）特殊化。stepSize ずつ進む単純な
	* [begin, end) 範囲。N 次元の IndexRange<T, N> の各軸を構成する
	* 末端の型でもある。
	*/
	template<std::integral T>
	class IndexRange<T, 1>
	{
	public:
		/// [EN] The integral index type.
		/// [JP] 整数のインデックス型。
		using IndexType = T;

		/// [EN] Number of dimensions (always 1 for this specialization).
		/// [JP] 次元数（この特殊化では常に1）。
		static constexpr Size rank_ = 1;

	public:
		IndexRange() = default;

		/**
		* [EN]
		* Constructs a [begin, end) range stepped by stepSize.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* stepSize ずつ進む [begin, end) 範囲を構築する。
		*/
		explicit IndexRange(T begin, T end, T stepSize) :begin_(begin), end_(end), stepSize_(stepSize)
		{
			/// No Code
		}

		/**
		* [EN]
		* Returns the range's start value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 範囲の開始値を返す。
		*/
		T begin()const
		{
			return begin_;
		}

		/**
		* [EN]
		* Returns the range's (exclusive) end value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 範囲の（排他的な）終了値を返す。
		*/
		T end()const
		{
			return end_;
		}

		/**
		* [EN]
		* Returns the step size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ステップ幅を返す。
		*/
		T step_size()const
		{
			return stepSize_;
		}

		/**
		* [EN]
		* Overwrites begin/end/stepSize in place and returns *this.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* begin/end/stepSize をその場で上書きし、*this を返す。
		*/
		IndexRange<T, 1>& reset(T begin, T end, T stepSize)
		{
			begin_ = begin;
			end_ = end;
			stepSize_ = stepSize;
			return *this;
		}

		/**
		* [EN]
		* Sets the start value and returns *this.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 開始値を設定し、*this を返す。
		*/
		IndexRange<T, 1>& begin(T newBegin)
		{
			begin_ = newBegin;
			return *this;
		}

		/**
		* [EN]
		* Sets the (exclusive) end value and returns *this.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* （排他的な）終了値を設定し、*this を返す。
		*/
		IndexRange<T, 1>& end(T newEnd)
		{
			end_ = newEnd;
			return *this;
		}

		/**
		* [EN]
		* Sets the step size and returns *this.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ステップ幅を設定し、*this を返す。
		*/
		IndexRange<T, 1>& step_size(T newStepSize)
		{
			stepSize_ = newStepSize;
			return *this;
		}

		/**
		* [EN]
		* Returns the number of elements in this range (see Distance).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この範囲の要素数を返す（Distance を参照）。
		*/
		Size size()const
		{
			return Distance(begin_, end_, stepSize_);
		}

		/**
		* [EN]
		* Maps a sub-range of element indices [partBegin, partEnd) (as
		* returned by e.g. slice_ceil/slice_floor) back to the
		* corresponding [begin, end) value range on this axis.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素インデックスの部分範囲 [partBegin, partEnd)（slice_ceil/
		* slice_floor などが返すもの）を、この軸上の対応する
		* [begin, end) 値範囲へ変換する。
		*/
		IndexRange<T, 1> unravel(Size partBegin, Size partEnd)const
		{
			return IndexRange<T, 1>(static_cast<T>(partBegin) * stepSize_ + begin_, static_cast<T>(partEnd) * stepSize_ + begin_, stepSize_);
		}

		/**
		* [EN]
		* Recursion terminal for IndexRange<T, N>::dimension: at rank 1
		* there is only one axis, so this simply returns self.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* IndexRange<T, N>::dimension の再帰終端。ランク1では軸が
		* 1つしかないため、単に self を返す。
		*/
		template <typename Self>
		auto& dimension(this Self&& self, Size dimension)
		{
			return std::forward<Self>(self);
		}

	private:
		/// [EN] Range start value.
		/// [JP] 範囲の開始値。
		T begin_;

		/// [EN] Range (exclusive) end value.
		/// [JP] 範囲の（排他的な）終了値。
		T end_;

		/// [EN] Step size.
		/// [JP] ステップ幅。
		T stepSize_;
	};

	/// [EN] Deduction guide: IndexRange(begin, end, step) deduces the rank-1 specialization.
	/// [JP] 推論ガイド: IndexRange(begin, end, step) はランク1特殊化に推論される。
	template<std::integral T>
	IndexRange(T, T, T) -> IndexRange<T, 1>;

	/// [EN] true iff T is any IndexRange<...> (any rank).
	/// [JP] T が（任意のランクの）IndexRange<...> である場合に true。
	template<typename>
	constexpr Bool IsIndexRangeValue = false;

	template <typename T, Size N>
	constexpr Bool IsIndexRangeValue<IndexRange<T, N>> = true;

	/// [EN] true iff T is specifically the rank-1 IndexRange<U, 1>.
	/// [JP] T が特にランク1の IndexRange<U, 1> である場合に true。
	template<typename T>
	constexpr Bool Is1DIndexRangeValue = false;

	template <typename T>
	constexpr Bool Is1DIndexRangeValue<IndexRange<T, 1>> = true;

	/// [EN] true iff T is an IndexRange<U, N> with N > 1.
	/// [JP] T が N > 1 の IndexRange<U, N> である場合に true。
	template<typename T>
	constexpr Bool IsMultiDimensionalIndexRangeValue = false;

	template <typename T, Size N>
		requires(N > 1)
	constexpr Bool IsMultiDimensionalIndexRangeValue<IndexRange<T, N>> = true;
}