#pragma once
#include <algorithm>
#include <concepts>
#include <numbers>
#include <cmath>
#include <type_traits>

namespace SeedCore
{
	/**
	* [EN]
	* Compile-time collection of pi and its common derived constants for
	* floating-point type T (multiples/fractions, reciprocal, and
	* degree/radian conversion factors).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 浮動小数点型 T に対する、円周率とその代表的な派生定数（倍数/分数、
	* 逆数、度/ラジアン変換係数）をまとめたコンパイル時定数集。
	*/
	template<typename T>
		requires std::floating_point<T>
	struct Pi
	{
		/// [EN] The value of pi.
		/// [JP] 円周率の値。
		static constexpr T Value = std::numbers::pi_v<T>;

		/// [EN] Two times pi (a full turn in radians).
		/// [JP] 円周率の2倍（ラジアンでの1回転）。
		static constexpr T Two = Value * T(2);

		/// [EN] Four times pi.
		/// [JP] 円周率の4倍。
		static constexpr T Four = Value * T(4);

		/// [EN] Half of pi (a quarter turn in radians).
		/// [JP] 円周率の半分（ラジアンでの4分の1回転）。
		static constexpr T Half = Value * T(0.5);

		/// [EN] A quarter of pi.
		/// [JP] 円周率の4分の1。
		static constexpr T Quarter = Value * T(0.25);

		/// [EN] The reciprocal of pi.
		/// [JP] 円周率の逆数。
		static constexpr T Inverse = T(1) / Value;

		/// [EN] Multiplier converting degrees to radians.
		/// [JP] 度をラジアンへ変換する際に乗じる係数。
		static constexpr T ToRadian = Value / T(180);

		/// [EN] Multiplier converting radians to degrees.
		/// [JP] ラジアンを度へ変換する際に乗じる係数。
		static constexpr T ToDegree = T(180) / Value;
	};
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the greater of a and b.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* a と b のうち大きい方を返す。
	*/
	template<typename T>
		requires std::totally_ordered<T>
	[[nodiscard]] constexpr const T& Max(const T& a, const T& b)noexcept
	{
		return (a < b) ? b : a;
	}

	/**
	* [EN]
	* Returns the greatest of first and args (variadic fold via pairwise Max).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* first と args のうち最大のものを返す（Max によるペアワイズの
	* 可変長畳み込み）。
	*/
	template<typename T, typename... Args>
		requires std::totally_ordered<T>
	[[nodiscard]] constexpr std::common_type_t<T, Args...> Max(T first, Args... args)noexcept
	{
		/// [EN] Everything is folded through one common type and handed to the
		///      two-argument overload with an explicit template argument. Passing
		///      mixed types straight through instead would make that overload fail
		///      deduction (T cannot be two different types at once), leaving this
		///      variadic as the only candidate - it would re-form the identical
		///      mixed-type call and recurse forever.
		/// [JP] すべてを 1 つの共通型へ畳み込み、明示的テンプレート引数付きで
		///      2 引数版へ渡す。型が混在したまま渡すと 2 引数版は T を 1 つに
		///      決められず推論に失敗し、候補がこの可変長版だけになる —
		///      同じ混在型の呼び出しを再構成して無限に再帰してしまう。
		using ReturnType = std::common_type_t<T, Args...>;

		if constexpr (sizeof...(args) == 0)
		{
			return static_cast<ReturnType>(first);
		}
		else
		{
			return Max<ReturnType>(static_cast<ReturnType>(first), static_cast<ReturnType>(Max(args...)));
		}
	}

	/**
	* [EN]
	* Returns the lesser of a and b.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* a と b のうち小さい方を返す。
	*/
	template<typename T>
		requires std::totally_ordered<T>
	[[nodiscard]] constexpr const T& Min(const T& a, const T& b)noexcept
	{
		return (b < a) ? b : a;
	}

	/**
	* [EN]
	* Returns the least of first and args (variadic fold via pairwise Min).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* first と args のうち最小のものを返す（Min によるペアワイズの
	* 可変長畳み込み）。
	*/
	template<typename T, typename... Args>
		requires std::totally_ordered<T>
	[[nodiscard]] constexpr std::common_type_t<T, Args...> Min(T first, Args... args) noexcept
	{
		/// [EN] Everything is folded through one common type and handed to the
		///      two-argument overload with an explicit template argument. Passing
		///      mixed types straight through instead would make that overload fail
		///      deduction (T cannot be two different types at once), leaving this
		///      variadic as the only candidate - it would re-form the identical
		///      mixed-type call and recurse forever.
		/// [JP] すべてを 1 つの共通型へ畳み込み、明示的テンプレート引数付きで
		///      2 引数版へ渡す。型が混在したまま渡すと 2 引数版は T を 1 つに
		///      決められず推論に失敗し、候補がこの可変長版だけになる —
		///      同じ混在型の呼び出しを再構成して無限に再帰してしまう。
		using ReturnType = std::common_type_t<T, Args...>;

		if constexpr (sizeof...(args) == 0)
		{
			return static_cast<ReturnType>(first);
		}
		else
		{
			return Min<ReturnType>(static_cast<ReturnType>(first), static_cast<ReturnType>(Min(args...)));
		}
	}

}

namespace SeedCore
{
	/**
	* [EN]
	* Clamps value into the inclusive range [min, max].
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* value を [min, max] の閉区間へクランプする。
	*/
	template<typename T, typename U>
		requires std::totally_ordered<T>
	[[nodiscard]] constexpr T Clamp(const U& value, const T& min, const T& max)noexcept
	{
		using ReturnType = std::common_type_t<T, U>;
		return Max(static_cast<ReturnType>(min), Min(static_cast<ReturnType>(value), static_cast<ReturnType>(max)));
	}

	/**
	* [EN]
	* Wraps value into the range [min, max) using floating-point modulo,
	* looping around at the boundaries instead of clamping. Returns min
	* if the range is empty/inverted.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 浮動小数点の剰余を用いて value を [min, max) の範囲へラップする。
	* 境界でクランプせず巻き戻る。範囲が空/逆転している場合は min を返す。
	*/
	template<typename T, typename U>
		requires std::totally_ordered<T>&& std::floating_point<T>
	[[nodiscard]] constexpr T Wrap(U value, T min, T max)noexcept
	{
		const T range = max - min;
		if (range <= 0) [[unlikely]]
		{
			return min;
		}

		T result = std::fmod(value - min, range);
		if (result < 0)
		{
			result += range;
		}
		return min + result;
	}

	/**
	* [EN]
	* Integer overload of Wrap: wraps value into the range [min, max)
	* using integer modulo. Returns min if the range is empty/inverted.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Wrap の整数版: 整数剰余を用いて value を [min, max) の範囲へ
	* ラップする。範囲が空/逆転している場合は min を返す。
	*/
	template<typename T, typename U>
		requires std::totally_ordered<T>&& std::integral<T>
	[[nodiscard]] constexpr T Wrap(U value, T min, T max)noexcept
	{
		const T range = max - min;
		if (range <= 0) [[unlikely]]
		{
			return min;
		}

		T result = (value - min) % range;
		if (result < 0)
		{
			result += range;
		}
		return min + result;
	}

	/**
	* [EN]
	* Bounces value back and forth within [min, max] (ping-pong wrapping)
	* using floating-point modulo. Returns min if the range is empty/inverted.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 浮動小数点の剰余を用いて value を [min, max] 内で往復させる
	* （ピンポン方式のラップ）。範囲が空/逆転している場合は min を返す。
	*/
	template<typename T, typename U>
		requires std::totally_ordered<T>&& std::floating_point<T>
	[[nodiscard]] constexpr T Pingpong(U value, T min, T max)noexcept
	{
		const T range = max - min;
		if (range <= 0) [[unlikely]]
		{
			return min;
		}

		const T doubleRange = range * 2;
		T result = std::fmod(value - min, doubleRange);
		if (result < 0)
		{
			result += doubleRange;
		}
		return min + (result > range ? (doubleRange - result) : result);
	}

	/**
	* [EN]
	* Integer overload of Pingpong: bounces value back and forth within
	* [min, max] using integer modulo. Returns min if the range is empty/inverted.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Pingpong の整数版: 整数剰余を用いて value を [min, max] 内で
	* 往復させる。範囲が空/逆転している場合は min を返す。
	*/
	template<typename T, typename U>
		requires std::totally_ordered<T>&& std::integral<T>
	[[nodiscard]] constexpr T Pingpong(U value, T min, T max)noexcept
	{
		const T range = max - min;
		if (range <= 0) [[unlikely]]
		{
			return min;
		}

		const T doubleRange = range * 2;
		T result = (value - min) % doubleRange;
		if (result < 0)
		{
			result += doubleRange;
		}
		return min + (result > range ? (doubleRange - result) : result);
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Linearly interpolates between min and max by t, clamping t to [0, 1] first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* t を [0, 1] へクランプした上で、min と max の間を線形補間する。
	*/
	template<typename T, typename U>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Lerp(T min, T max, U t)noexcept
	{
		if (t <= 0) [[unlikely]]
		{
			return min;
		}
		if (t >= 1) [[unlikely]]
		{
			return max;
		}
		return min + (max - min) * t;
	}

	/**
	* [EN]
	* Inverse of Lerp: returns the interpolation factor t such that
	* Lerp(min, max, t) == value. Returns 0 if min equals max.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Lerp の逆演算: Lerp(min, max, t) == value となる補間係数 t を返す。
	* min と max が等しい場合は 0 を返す。
	*/
	template<typename T, typename U>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Unlerp(T min, T max, U value)noexcept
	{
		const T range = max - min;
		if (range == 0) [[unlikely]]
		{
			return 0;
		}
		return (value - min) / range;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the absolute value of a floating-point value.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 浮動小数点値の絶対値を返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Abs(T value)noexcept
	{
		return (value < 0) ? -value : value;
	}

	/**
	* [EN]
	* Returns the absolute value of an integral value.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 整数値の絶対値を返す。
	*/
	template<typename T>
		requires std::integral<T>
	[[nodiscard]] constexpr T Abs(T value)noexcept
	{
		return (value < 0) ? -value : value;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Converts degrees to radians.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 度をラジアンへ変換する。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T ToRadians(T degrees)noexcept
	{
		return degrees * Pi<T>::ToRadian;
	}

	/**
	* [EN]
	* Converts radians to degrees.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ラジアンを度へ変換する。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T ToDegrees(T radians)noexcept
	{
		return radians * Pi<T>::ToDegree;
	}

	/**
	* [EN]
	* Returns the sine of radian.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* radian の正弦（サイン）を返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Sin(T radian)noexcept
	{
		return std::sin(radian);
	}

	/**
	* [EN]
	* Returns the cosine of radian.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* radian の余弦（コサイン）を返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Cos(T radian)noexcept
	{
		return std::cos(radian);
	}

	/**
	* [EN]
	* Returns the tangent of radian.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* radian の正接（タンジェント）を返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Tan(T radian)noexcept
	{
		return std::tan(radian);
	}

	/**
	* [EN]
	* Returns the arcsine of value, in radians.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* value の逆正弦（アークサイン）をラジアンで返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Asin(T value)noexcept
	{
		return std::asin(value);
	}

	/**
	* [EN]
	* Returns the arccosine of value, in radians.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* value の逆余弦（アークコサイン）をラジアンで返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Acos(T value)noexcept
	{
		return std::acos(value);
	}

	/**
	* [EN]
	* Returns the arctangent of value, in radians.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* value の逆正接（アークタンジェント）をラジアンで返す。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Atan(T value)noexcept
	{
		return std::atan(value);
	}

	/**
	* [EN]
	* Returns the arctangent of y/x, in radians, using the signs of both
	* arguments to determine the correct quadrant.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* y/x の逆正接をラジアンで返す。両引数の符号を用いて正しい象限を
	* 判定する。
	*/
	template<typename T>
		requires std::floating_point<T>
	[[nodiscard]] constexpr T Atan2(T y, T x)noexcept
	{
		return std::atan2(y, x);
	}
}
