#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace SeedCore
{
	/**
	* [EN]
	* Fixed-size, stack-allocated array of exactly N elements - the
	* project's std::array equivalent. Like std::array it is an
	* aggregate (no user-declared constructors), so
	* StaticArray<Int, 3> a{ 1, 2, 3 }; works.
	*
	* The one deliberate difference from std::array: iterators are raw
	* pointers (iterator = T*). MSVC's std::array iterators are checked
	* wrapper objects under _ITERATOR_DEBUG_LEVEL (2 by default in
	* Debug), adding a bounds check to every dereference and increment;
	* the engine cannot globally disable that flag because the vendored
	* prebuilt libraries are compiled at the default level and MSVC
	* mangles symbol names by it. See DynamicArray for the full rationale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ちょうど N 要素の固定サイズ・スタック確保配列 - プロジェクトの
	* std::array 相当。std::array 同様、集成体（ユーザー宣言の
	* コンストラクタなし）なので StaticArray<Int, 3> a{ 1, 2, 3 }; が動く。
	*
	* std::array との意図的な違いは1つ: イテレータは生ポインタ
	* （iterator = T*）。MSVC の std::array のイテレータは
	* _ITERATOR_DEBUG_LEVEL（Debug では既定で2）の下ではチェック付きの
	* ラッパーオブジェクトになり、逆参照・インクリメントごとに境界
	* チェックが入る。vendored のプリビルドライブラリが既定レベルで
	* コンパイル済みで MSVC がそのレベルでシンボル名を変えるため、
	* エンジン全体でこのフラグを無効にはできない。詳しい根拠は
	* DynamicArray 参照。
	*/
	template<typename T, Size N>
		requires (N > 0)
	struct StaticArray
	{
		/// [EN] The element type.
		/// [JP] 要素の型。
		using value_type = T;

		/// [EN] Type used for sizes and indices.
		/// [JP] サイズとインデックスに使う型。
		using size_type = Size;

		/// [EN] Signed type for the distance between two iterators.
		/// [JP] 2つのイテレータ間の距離を表す符号付き型。
		using difference_type = std::ptrdiff_t;

		/// [EN] Reference to a mutable element.
		/// [JP] 可変要素への参照。
		using reference = T&;

		/// [EN] Reference to a const element.
		/// [JP] const 要素への参照。
		using const_reference = const T&;

		/// [EN] Pointer to a mutable element.
		/// [JP] 可変要素へのポインタ。
		using pointer = T*;

		/// [EN] Pointer to a const element.
		/// [JP] const 要素へのポインタ。
		using const_pointer = const T*;

		/// [EN] Mutable iterator - a raw pointer, so it carries no debug-iterator overhead (see the class comment).
		/// [JP] 可変イテレータ - 生ポインタなので debug-iterator のオーバーヘッドが無い（クラスコメント参照）。
		using iterator = T*;

		/// [EN] Const iterator - a raw pointer to const.
		/// [JP] const イテレータ - const への生ポインタ。
		using const_iterator = const T*;

		/// [EN] Mutable reverse iterator.
		/// [JP] 可変逆イテレータ。
		using reverse_iterator = std::reverse_iterator<iterator>;

		/// [EN] Const reverse iterator.
		/// [JP] const 逆イテレータ。
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		/// [EN] The N contiguous elements. Public so the type stays an aggregate.
		/// [JP] 連続する N 個の要素。集成体であり続けるため public。
		T data_[N];

		/**
		* [EN]
		* Unchecked access to the element at index (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index の要素への未チェックアクセス（const / 非 const）。
		*/
		reference operator[](Size index)
		{
			return data_[index];
		}

		const_reference operator[](Size index)const
		{
			return data_[index];
		}

		/**
		* [EN]
		* Bounds-checked access to the element at index; throws
		* std::out_of_range when index is past the end (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index の要素への境界チェック付きアクセス。index が末尾を超えると
		* std::out_of_range を投げる（const / 非 const）。
		*/
		reference at(Size index)
		{
			if (index >= N)
			{
				throw std::out_of_range("StaticArray::at");
			}
			return data_[index];
		}

		const_reference at(Size index)const
		{
			if (index >= N)
			{
				throw std::out_of_range("StaticArray::at");
			}
			return data_[index];
		}

		/**
		* [EN]
		* Returns the first element (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素を返す（const / 非 const）。
		*/
		reference front()
		{
			return data_[0];
		}

		const_reference front()const
		{
			return data_[0];
		}

		/**
		* [EN]
		* Returns the last element (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素を返す（const / 非 const）。
		*/
		reference back()
		{
			return data_[N - 1];
		}

		const_reference back()const
		{
			return data_[N - 1];
		}

		/**
		* [EN]
		* Returns a pointer to the element storage (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素ストレージへのポインタを返す（const / 非 const）。
		*/
		pointer data()noexcept
		{
			return data_;
		}

		const_pointer data()const noexcept
		{
			return data_;
		}

		/**
		* [EN]
		* Returns an iterator to the first element (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素へのイテレータを返す（const / 非 const）。
		*/
		iterator begin()noexcept
		{
			return data_;
		}

		const_iterator begin()const noexcept
		{
			return data_;
		}

		/**
		* [EN]
		* Returns a const iterator to the first element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素への const イテレータを返す。
		*/
		const_iterator cbegin()const noexcept
		{
			return data_;
		}

		/**
		* [EN]
		* Returns an iterator one past the last element (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素の1つ後ろを指すイテレータを返す（const / 非 const）。
		*/
		iterator end()noexcept
		{
			return data_ + N;
		}

		const_iterator end()const noexcept
		{
			return data_ + N;
		}

		/**
		* [EN]
		* Returns a const iterator one past the last element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素の1つ後ろを指す const イテレータを返す。
		*/
		const_iterator cend()const noexcept
		{
			return data_ + N;
		}

		/**
		* [EN]
		* Returns a reverse iterator to the last element (const and non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素を指す逆イテレータを返す（const / 非 const）。
		*/
		reverse_iterator rbegin()noexcept
		{
			return reverse_iterator(end());
		}

		const_reverse_iterator rbegin()const noexcept
		{
			return const_reverse_iterator(end());
		}

		/**
		* [EN]
		* Returns a reverse iterator one before the first element (const and
		* non-const).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素の1つ前を指す逆イテレータを返す（const / 非 const）。
		*/
		reverse_iterator rend()noexcept
		{
			return reverse_iterator(begin());
		}

		const_reverse_iterator rend()const noexcept
		{
			return const_reverse_iterator(begin());
		}

		/**
		* [EN]
		* Returns whether N is 0 - always false, since N > 0 is required.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* N が0かを返す - N > 0 が要求されるため常に false。
		*/
		Bool empty()const noexcept
		{
			return N == 0;
		}

		/**
		* [EN]
		* Returns the element count, N.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素数 N を返す。
		*/
		Size size()const noexcept
		{
			return N;
		}

		/**
		* [EN]
		* Returns N (a fixed-size array's max size equals its size).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* N を返す（固定サイズ配列の最大サイズはサイズに等しい）。
		*/
		Size max_size()const noexcept
		{
			return N;
		}

		/**
		* [EN]
		* Assigns value to every element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全要素に value を代入する。
		*/
		void fill(const T& value)
		{
			for (Size index = 0; index < N; ++index)
			{
				data_[index] = value;
			}
		}

		/**
		* [EN]
		* Swaps contents element-wise with other.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* other と要素ごとに内容を交換する。
		*/
		void swap(StaticArray& other)noexcept
		{
			for (Size index = 0; index < N; ++index)
			{
				std::swap(data_[index], other.data_[index]);
			}
		}

		/**
		* [EN]
		* Element-wise equality.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素ごとの等価。
		*/
		friend Bool operator==(const StaticArray& lhs, const StaticArray& rhs)
		{
			for (Size index = 0; index < N; ++index)
			{
				if (!(lhs.data_[index] == rhs.data_[index]))
				{
					return false;
				}
			}
			return true;
		}
	};

	/**
	* [EN]
	* Swaps two StaticArrays; found by ADL.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つの StaticArray を交換する。ADL で見つかる。
	*/
	template<typename T, Size N>
	void swap(StaticArray<T, N>& lhs, StaticArray<T, N>& rhs)noexcept
	{
		lhs.swap(rhs);
	}
}
