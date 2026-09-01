#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace SeedCore
{
	/**
	* [EN]
	* Heap-allocated, contiguous, resizable array - the project's
	* std::vector equivalent. Two deliberate differences from
	* std::vector, both chosen for performance on this codebase:
	*
	*  - Iterators are raw pointers (iterator = T*). MSVC's std::vector
	*    iterators are checked wrapper objects under _ITERATOR_DEBUG_LEVEL
	*    (2 by default in Debug), which adds a bounds/validity check to
	*    every dereference and increment - often a 10-100x slowdown on
	*    tight loops in Debug. The engine cannot globally set
	*    _ITERATOR_DEBUG_LEVEL to 0 because the vendored prebuilt
	*    libraries (JoltPhysics, DirectXTK, ...) are compiled at the
	*    default level and MSVC mangles symbol names by that level, so a
	*    mismatch fails to link. A raw pointer sidesteps the checked
	*    machinery per-container without touching the ABI-wide flag.
	*
	*  - Growth factor is 1.5x, not 2x. With a factor of exactly 2 the
	*    sum of all previous allocations (1 + 2 + 4 + ... + 2^(n-1) =
	*    2^n - 1) is always smaller than the next request (2^n), so a
	*    reallocation can never reuse any previously-freed block. A
	*    factor below 2 (1.5 here) eventually lets an allocation fit into
	*    the space vacated by earlier ones, reducing fragmentation and
	*    peak memory. This follows folly::fbvector and EASTL.
	*
	* Relocation on growth is a single memcpy when T is trivially
	* copyable, and a move-or-copy loop (std::move_if_noexcept, for the
	* strong-ish guarantee) otherwise.
	*
	* Like std::vector (C++17+), T may be incomplete at the point the
	* DynamicArray type is named; each member is only instantiated when
	* called, by which point T is complete.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ヒープ確保される連続・サイズ可変配列 - プロジェクトの std::vector
	* 相当。std::vector との意図的な違いが2つあり、どちらもこの
	* コードベースでの性能のために選んでいる:
	*
	*  - イテレータは生ポインタ（iterator = T*）。MSVC の std::vector の
	*    イテレータは _ITERATOR_DEBUG_LEVEL（Debug では既定で2）の下では
	*    チェック付きのラッパーオブジェクトになり、逆参照・インクリメント
	*    ごとに境界/有効性チェックが入る - タイトなループでは Debug で
	*    しばしば 10〜100倍遅くなる。エンジン全体で _ITERATOR_DEBUG_LEVEL
	*    を0にはできない。vendored のプリビルドライブラリ（JoltPhysics、
	*    DirectXTK など）が既定レベルでコンパイル済みで、MSVC は
	*    そのレベルでシンボル名を変えるため、不一致だとリンクできない。
	*    生ポインタなら ABI 全体のフラグに触れずコンテナ単位で
	*    チェック機構を回避できる。
	*
	*  - 成長率は 2倍ではなく 1.5倍。ちょうど2倍だと、これまでの全確保の
	*    合計（1 + 2 + 4 + ... + 2^(n-1) = 2^n - 1）が常に次の要求（2^n）
	*    より小さく、再確保が解放済みブロックを一切再利用できない。
	*    2未満（ここでは1.5）なら、いずれ以前に空いた領域へ確保が
	*    収まるようになり、断片化とピークメモリが減る。folly::fbvector
	*    および EASTL に倣う。
	*
	* 成長時の再配置は、T が trivially copyable なら単一の memcpy、
	* そうでなければ move-or-copy ループ（強めの保証のため
	* std::move_if_noexcept）。
	*
	* std::vector（C++17+）同様、DynamicArray 型を書いた時点では T は
	* 不完全でよい。各メンバは呼ばれた時にのみ実体化され、その時点では
	* T は完全になっている。
	*/
	template<typename T>
	class DynamicArray
	{
	public:
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

		/**
		* [EN]
		* Constructs an empty array; no allocation is performed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空の配列を構築する。確保は行わない。
		*/
		DynamicArray() = default;

		/**
		* [EN]
		* Constructs an array of count default-constructed elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* count 個のデフォルト構築された要素からなる配列を構築する。
		*/
		explicit DynamicArray(Size count)
		{
			if (count > 0)
			{
				data_ = Allocate(count);
				capacity_ = count;
				std::uninitialized_default_construct_n(data_, count);
				size_ = count;
			}
		}

		/**
		* [EN]
		* Constructs an array of count copies of value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピー count 個からなる配列を構築する。
		*/
		DynamicArray(Size count, const T& value)
		{
			if (count > 0)
			{
				data_ = Allocate(count);
				capacity_ = count;
				std::uninitialized_fill_n(data_, count, value);
				size_ = count;
			}
		}

		/**
		* [EN]
		* Constructs an array from the iterator range [first, last). The
		* integral guard keeps this from hijacking DynamicArray(count, value).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* イテレータ範囲 [first, last) から配列を構築する。整数ガードにより
		* DynamicArray(count, value) を奪わないようにしている。
		*/
		template<typename InputIt, typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
		DynamicArray(InputIt first, InputIt last)
		{
			AssignRange(first, last);
		}

		/**
		* [EN]
		* Constructs an array from an initializer list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 初期化子リストから配列を構築する。
		*/
		DynamicArray(std::initializer_list<T> values)
		{
			AssignRange(values.begin(), values.end());
		}

		/**
		* [EN]
		* Constructs an array by copying a std::vector's elements - for
		* interop with third-party APIs (e.g. TinyglTF) that hand back
		* std::vector.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::vector の要素をコピーして配列を構築する - std::vector を
		* 返す外部 API（例: TinyglTF）との相互運用のため。
		*/
		template<typename Allocator>
		DynamicArray(const std::vector<T, Allocator>& source)
		{
			AssignRange(source.begin(), source.end());
		}

		/**
		* [EN]
		* Copy constructor: a deep copy of other's elements, with capacity
		* trimmed to size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピーコンストラクタ: other の要素のディープコピー。容量は
		* サイズに切り詰める。
		*/
		DynamicArray(const DynamicArray& other)
		{
			AssignRange(other.data_, other.data_ + other.size_);
		}

		/**
		* [EN]
		* Move constructor: takes over other's buffer, leaving other empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブコンストラクタ: other のバッファを引き取り、other を
		* 空にする。
		*/
		DynamicArray(DynamicArray&& other)noexcept :data_(other.data_), size_(other.size_), capacity_(other.capacity_)
		{
			other.data_ = nullptr;
			other.size_ = 0;
			other.capacity_ = 0;
		}

		/**
		* [EN]
		* Destroys every element and frees the buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全要素を破棄し、バッファを解放する。
		*/
		~DynamicArray()
		{
			DestroyAll();
			Deallocate(data_);
		}

		/**
		* [EN]
		* Copy assignment; safe against self-assignment.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー代入。自己代入に対して安全。
		*/
		DynamicArray& operator=(const DynamicArray& other)
		{
			if (this != &other)
			{
				assign(other.data_, other.data_ + other.size_);
			}
			return *this;
		}

		/**
		* [EN]
		* Move assignment: frees this array's buffer and takes over other's.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブ代入: この配列のバッファを解放し、other のものを引き取る。
		*/
		DynamicArray& operator=(DynamicArray&& other)noexcept
		{
			if (this != &other)
			{
				DestroyAll();
				Deallocate(data_);
				data_ = other.data_;
				size_ = other.size_;
				capacity_ = other.capacity_;
				other.data_ = nullptr;
				other.size_ = 0;
				other.capacity_ = 0;
			}
			return *this;
		}

		/**
		* [EN]
		* Replaces the contents with the initializer list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内容を初期化子リストで置き換える。
		*/
		DynamicArray& operator=(std::initializer_list<T> values)
		{
			assign(values.begin(), values.end());
			return *this;
		}

		/**
		* [EN]
		* Replaces the contents by copying a std::vector's elements
		* (third-party interop).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::vector の要素をコピーして内容を置き換える（外部 API との
		* 相互運用）。
		*/
		template<typename Allocator>
		DynamicArray& operator=(const std::vector<T, Allocator>& source)
		{
			assign(source.begin(), source.end());
			return *this;
		}

		/**
		* [EN]
		* Replaces the contents with count copies of value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内容を value のコピー count 個で置き換える。
		*/
		void assign(Size count, const T& value)
		{
			clear();
			if (count > 0)
			{
				Reserve(count);
				std::uninitialized_fill_n(data_, count, value);
				size_ = count;
			}
		}

		/**
		* [EN]
		* Replaces the contents with the range [first, last).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内容を範囲 [first, last) で置き換える。
		*/
		template<typename InputIt, typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
		void assign(InputIt first, InputIt last)
		{
			clear();
			for (; first != last; ++first)
			{
				push_back(*first);
			}
		}

		/**
		* [EN]
		* Replaces the contents with the initializer list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内容を初期化子リストで置き換える。
		*/
		void assign(std::initializer_list<T> values)
		{
			assign(values.begin(), values.end());
		}

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
			if (index >= size_)
			{
				throw std::out_of_range("DynamicArray::at");
			}
			return data_[index];
		}

		const_reference at(Size index)const
		{
			if (index >= size_)
			{
				throw std::out_of_range("DynamicArray::at");
			}
			return data_[index];
		}

		/**
		* [EN]
		* Returns the first element (const and non-const); undefined if empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素を返す（const / 非 const）。空の場合は未定義。
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
		* Returns the last element (const and non-const); undefined if empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素を返す（const / 非 const）。空の場合は未定義。
		*/
		reference back()
		{
			return data_[size_ - 1];
		}

		const_reference back()const
		{
			return data_[size_ - 1];
		}

		/**
		* [EN]
		* Returns a pointer to the element storage (const and non-const);
		* nullptr while capacity is 0.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素ストレージへのポインタを返す（const / 非 const）。容量が0の
		* 間は nullptr。
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
			return data_ + size_;
		}

		const_iterator end()const noexcept
		{
			return data_ + size_;
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
			return data_ + size_;
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
		* Returns whether the array holds no elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 配列が要素を1つも保持していないかを返す。
		*/
		Bool empty()const noexcept
		{
			return size_ == 0;
		}

		/**
		* [EN]
		* Returns the number of elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素数を返す。
		*/
		Size size()const noexcept
		{
			return size_;
		}

		/**
		* [EN]
		* Returns how many elements the current allocation can hold.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の確保が保持できる要素数を返す。
		*/
		Size capacity()const noexcept
		{
			return capacity_;
		}

		/**
		* [EN]
		* Returns a theoretical upper bound on size (address space divided
		* by element size).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* サイズの理論上の上限を返す（アドレス空間を要素サイズで割った値）。
		*/
		Size max_size()const noexcept
		{
			return static_cast<Size>(-1) / sizeof(T);
		}

		/**
		* [EN]
		* Ensures capacity for at least newCapacity elements, reallocating
		* (and relocating the existing elements) if it has to grow. Never
		* shrinks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 少なくとも newCapacity 要素分の容量を確保する。拡大が必要なら
		* 再確保し既存要素を再配置する。縮小はしない。
		*/
		void reserve(Size newCapacity)
		{
			if (newCapacity > capacity_)
			{
				Reallocate(newCapacity);
			}
		}

		/**
		* [EN]
		* Reallocates so capacity equals size; releases the buffer entirely
		* when the array is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 容量をサイズに一致させるよう再確保する。配列が空ならバッファを
		* 完全に解放する。
		*/
		void shrink_to_fit()
		{
			if (capacity_ > size_)
			{
				if (size_ == 0)
				{
					Deallocate(data_);
					data_ = nullptr;
					capacity_ = 0;
				}
				else
				{
					Reallocate(size_);
				}
			}
		}

		/**
		* [EN]
		* Destroys every element; keeps the allocated buffer for reuse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全要素を破棄する。確保済みバッファは再利用のため保持する。
		*/
		void clear()noexcept
		{
			DestroyAll();
			size_ = 0;
		}

		/**
		* [EN]
		* Appends a copy of value, growing the buffer if it is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーを末尾に追加する。バッファが満杯なら拡大する。
		*/
		void push_back(const T& value)
		{
			EnsureOneMore();
			new (data_ + size_) T(value);
			++size_;
		}

		/**
		* [EN]
		* Appends value by move, growing the buffer if it is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value をムーブで末尾に追加する。バッファが満杯なら拡大する。
		*/
		void push_back(T&& value)
		{
			EnsureOneMore();
			new (data_ + size_) T(std::move(value));
			++size_;
		}

		/**
		* [EN]
		* Constructs a new element in place at the end from args and returns
		* a reference to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* args から末尾に新しい要素をその場で構築し、その参照を返す。
		*/
		template<typename... Args>
		reference emplace_back(Args&&... args)
		{
			EnsureOneMore();
			new (data_ + size_) T(std::forward<Args>(args)...);
			++size_;
			return data_[size_ - 1];
		}

		/**
		* [EN]
		* Removes the last element; undefined if empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾要素を削除する。空の場合は未定義。
		*/
		void pop_back()
		{
			--size_;
			data_[size_].~T();
		}

		/**
		* [EN]
		* Grows or shrinks to count elements, default-constructing any new
		* ones.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* count 要素へ拡大または縮小する。新規要素はデフォルト構築する。
		*/
		void resize(Size count)
		{
			if (count < size_)
			{
				std::destroy(data_ + count, data_ + size_);
			}
			else if (count > size_)
			{
				Reserve(count);
				std::uninitialized_default_construct_n(data_ + size_, count - size_);
			}
			size_ = count;
		}

		/**
		* [EN]
		* Grows or shrinks to count elements, copying value into any new
		* ones.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* count 要素へ拡大または縮小する。新規要素には value をコピーする。
		*/
		void resize(Size count, const T& value)
		{
			if (count < size_)
			{
				std::destroy(data_ + count, data_ + size_);
			}
			else if (count > size_)
			{
				Reserve(count);
				std::uninitialized_fill_n(data_ + size_, count - size_, value);
			}
			size_ = count;
		}

		/**
		* [EN]
		* Inserts a copy of value before position and returns an iterator to
		* the inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の前に value のコピーを挿入し、挿入した要素への
		* イテレータを返す。
		*/
		iterator insert(const_iterator position, const T& value)
		{
			return EmplaceAt(position, value);
		}

		/**
		* [EN]
		* Inserts value by move before position and returns an iterator to
		* the inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の前に value をムーブで挿入し、挿入した要素への
		* イテレータを返す。
		*/
		iterator insert(const_iterator position, T&& value)
		{
			return EmplaceAt(position, std::move(value));
		}

		/**
		* [EN]
		* Inserts count copies of value before position and returns an
		* iterator to the first inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の前に value のコピーを count 個挿入し、最初に挿入した
		* 要素へのイテレータを返す。
		*/
		iterator insert(const_iterator position, Size count, const T& value)
		{
			Size offset = static_cast<Size>(position - data_);
			if (count == 0)
			{
				return data_ + offset;
			}
			MakeGap(offset, count);
			for (Size index = 0; index < count; ++index)
			{
				new (data_ + offset + index) T(value);
			}
			size_ += count;
			return data_ + offset;
		}

		/**
		* [EN]
		* Inserts the range [first, last) before position and returns an
		* iterator to the first inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の前に範囲 [first, last) を挿入し、最初に挿入した要素
		* へのイテレータを返す。
		*/
		template<typename InputIt, typename = std::enable_if_t<!std::is_integral_v<InputIt>>>
		iterator insert(const_iterator position, InputIt first, InputIt last)
		{
			Size offset = static_cast<Size>(position - data_);
			Size count = static_cast<Size>(std::distance(first, last));
			if (count == 0)
			{
				return data_ + offset;
			}
			MakeGap(offset, count);
			Size index = 0;
			for (; first != last; ++first, ++index)
			{
				new (data_ + offset + index) T(*first);
			}
			size_ += count;
			return data_ + offset;
		}

		/**
		* [EN]
		* Inserts the initializer list before position and returns an
		* iterator to the first inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の前に初期化子リストを挿入し、最初に挿入した要素への
		* イテレータを返す。
		*/
		iterator insert(const_iterator position, std::initializer_list<T> values)
		{
			return insert(position, values.begin(), values.end());
		}

		/**
		* [EN]
		* Constructs a new element in place before position from args and
		* returns an iterator to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* args から position の前に新しい要素をその場で構築し、その
		* イテレータを返す。
		*/
		template<typename... Args>
		iterator emplace(const_iterator position, Args&&... args)
		{
			return EmplaceAt(position, std::forward<Args>(args)...);
		}

		/**
		* [EN]
		* Removes the element at position; elements after it shift down one.
		* Returns an iterator to the element that followed the removed one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* position の要素を削除する。後続の要素が1つ前へ詰められる。
		* 削除した要素の直後にあった要素へのイテレータを返す。
		*/
		iterator erase(const_iterator position)
		{
			Size offset = static_cast<Size>(position - data_);
			std::move(data_ + offset + 1, data_ + size_, data_ + offset);
			--size_;
			data_[size_].~T();
			return data_ + offset;
		}

		/**
		* [EN]
		* Removes the range [first, last); elements after it shift down.
		* Returns an iterator to the element that followed the last removed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 範囲 [first, last) を削除する。後続の要素が詰められる。最後に
		* 削除した要素の直後にあった要素へのイテレータを返す。
		*/
		iterator erase(const_iterator first, const_iterator last)
		{
			Size offset = static_cast<Size>(first - data_);
			Size count = static_cast<Size>(last - first);
			if (count == 0)
			{
				return data_ + offset;
			}
			std::move(data_ + offset + count, data_ + size_, data_ + offset);
			std::destroy(data_ + size_ - count, data_ + size_);
			size_ -= count;
			return data_ + offset;
		}

		/**
		* [EN]
		* Swaps the buffers of this array and other in O(1).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この配列と other のバッファを O(1) で交換する。
		*/
		void swap(DynamicArray& other)noexcept
		{
			std::swap(data_, other.data_);
			std::swap(size_, other.size_);
			std::swap(capacity_, other.capacity_);
		}

		/**
		* [EN]
		* Element-wise equality: equal sizes and every element compares
		* equal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要素ごとの等価: サイズが等しく、全要素が等しい。
		*/
		friend Bool operator==(const DynamicArray& lhs, const DynamicArray& rhs)
		{
			if (lhs.size_ != rhs.size_)
			{
				return false;
			}
			for (Size index = 0; index < lhs.size_; ++index)
			{
				if (!(lhs.data_[index] == rhs.data_[index]))
				{
					return false;
				}
			}
			return true;
		}

	private:
		/**
		* [EN]
		* Allocates raw, uninitialized, suitably-aligned storage for count
		* elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* count 要素分の、生の未初期化で適切にアラインされたストレージを
		* 確保する。
		*/
		static pointer Allocate(Size count)
		{
			return static_cast<pointer>(::operator new(count * sizeof(T), std::align_val_t{ alignof(T) }));
		}

		/**
		* [EN]
		* Frees storage returned by Allocate (a null block is a no-op).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Allocate が返したストレージを解放する（null ブロックは無操作）。
		*/
		static void Deallocate(pointer block)noexcept
		{
			::operator delete(block, std::align_val_t{ alignof(T) });
		}

		/**
		* [EN]
		* Runs the destructor of every constructed element; does not touch
		* size_ or the buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 構築済み全要素のデストラクタを実行する。size_ やバッファには
		* 触れない。
		*/
		void DestroyAll()noexcept
		{
			std::destroy(data_, data_ + size_);
		}

		/**
		* [EN]
		* Returns the capacity to grow to: 1.5x the current capacity (see
		* the class comment for why not 2x), bumped up to required, with a
		* floor of 1 so the first insertion allocates.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 拡大先の容量を返す: 現在の容量の1.5倍（2倍にしない理由は
		* クラスコメント参照）、required まで引き上げ、最初の挿入が確保
		* できるよう下限1。
		*/
		Size GrowthTarget(Size required)const
		{
			Size grown = capacity_ + capacity_ / 2;
			if (grown < required)
			{
				grown = required;
			}
			if (grown < 1)
			{
				grown = 1;
			}
			return grown;
		}

		/**
		* [EN]
		* Grows the buffer by the growth factor if it is exactly full, so
		* one more element fits.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バッファがちょうど満杯なら成長率ぶん拡大し、あと1要素入る
		* ようにする。
		*/
		void EnsureOneMore()
		{
			if (size_ == capacity_)
			{
				Reallocate(GrowthTarget(size_ + 1));
			}
		}

		/**
		* [EN]
		* Internal reserve that applies the growth factor (public reserve
		* allocates exactly what was asked); used by assign/resize/insert.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 成長率を適用する内部 reserve（公開 reserve は要求ちょうどを
		* 確保する）。assign/resize/insert が使う。
		*/
		void Reserve(Size required)
		{
			if (required > capacity_)
			{
				Reallocate(GrowthTarget(required));
			}
		}

		/**
		* [EN]
		* Opens a gap of count uninitialized slots at offset by moving the
		* elements at [offset, size_) up by count, growing the buffer first
		* if needed. size_ is left unchanged; the caller constructs into the
		* gap and adds count to size_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [offset, size_) の要素を count 個分後ろへずらして、offset に
		* count 個の未初期化スロットの隙間を空ける。必要なら先にバッファを
		* 拡大する。size_ は変えない。呼び出し側が隙間へ構築し size_ に
		* count を足す。
		*/
		void MakeGap(Size offset, Size count)
		{
			if (size_ + count > capacity_)
			{
				Reallocate(GrowthTarget(size_ + count));
			}
			for (Size index = size_; index > offset; --index)
			{
				new (data_ + index - 1 + count) T(std::move(data_[index - 1]));
				data_[index - 1].~T();
			}
		}

		/**
		* [EN]
		* Shared implementation of insert(pos, value) / emplace(pos, args):
		* opens a one-slot gap and constructs the element into it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* insert(pos, value) / emplace(pos, args) の共通実装: 1スロットの
		* 隙間を空けてそこへ要素を構築する。
		*/
		template<typename... Args>
		iterator EmplaceAt(const_iterator position, Args&&... args)
		{
			Size offset = static_cast<Size>(position - data_);
			MakeGap(offset, 1);
			new (data_ + offset) T(std::forward<Args>(args)...);
			++size_;
			return data_ + offset;
		}

		/**
		* [EN]
		* Moves count elements from source to destination (a single memcpy
		* when T is trivially copyable, a move-or-copy loop with
		* std::move_if_noexcept otherwise) and destroys the source elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* source から destination へ count 要素を移動する（T が trivially
		* copyable なら単一 memcpy、そうでなければ std::move_if_noexcept の
		* move-or-copy ループ）。source 側の要素は破棄する。
		*/
		static void Relocate(pointer destination, pointer source, Size count)
		{
			if constexpr (std::is_trivially_copyable_v<T>)
			{
				std::memcpy(destination, source, count * sizeof(T));
			}
			else
			{
				for (Size index = 0; index < count; ++index)
				{
					new (destination + index) T(std::move_if_noexcept(source[index]));
					source[index].~T();
				}
			}
		}

		/**
		* [EN]
		* Allocates a buffer of newCapacity, relocates the existing elements
		* into it, frees the old buffer, and adopts the new one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* newCapacity のバッファを確保し、既存要素をそこへ再配置し、古い
		* バッファを解放して新しいものに置き換える。
		*/
		void Reallocate(Size newCapacity)
		{
			pointer block = Allocate(newCapacity);
			Relocate(block, data_, size_);
			Deallocate(data_);
			data_ = block;
			capacity_ = newCapacity;
		}

		/**
		* [EN]
		* Fills a freshly-empty array from [first, last) by repeated
		* push_back; shared by the range/list/copy/vector constructors.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空になったばかりの配列を、push_back の繰り返しで [first, last)
		* から満たす。範囲/リスト/コピー/vector コンストラクタで共有する。
		*/
		template<typename InputIt>
		void AssignRange(InputIt first, InputIt last)
		{
			for (; first != last; ++first)
			{
				push_back(*first);
			}
		}

		/// [EN] Start of the element storage; nullptr while capacity_ is 0.
		/// [JP] 要素ストレージの先頭。capacity_ が0の間は nullptr。
		T* data_ = nullptr;

		/// [EN] Number of constructed elements.
		/// [JP] 構築済み要素の数。
		Size size_ = 0;

		/// [EN] Number of elements the current allocation can hold.
		/// [JP] 現在の確保が保持できる要素数。
		Size capacity_ = 0;
	};

	/**
	* [EN]
	* Swaps two DynamicArrays; found by ADL (e.g. by std::ranges::sort).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つの DynamicArray を交換する。ADL で見つかる（例: std::ranges::sort）。
	*/
	template<typename T>
	void swap(DynamicArray<T>& lhs, DynamicArray<T>& rhs)noexcept
	{
		lhs.swap(rhs);
	}

	/**
	* [EN]
	* Erases every element of container equal to value and returns the
	* count removed. The ADL counterpart of std::erase (which is defined
	* only for standard containers) - call it unqualified as
	* erase(container, value).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* container の中で value に等しい全要素を削除し、削除数を返す。
	* std::erase（標準コンテナ専用）の ADL 版 - erase(container, value)
	* と非修飾で呼ぶ。
	*/
	template<typename T, typename U>
	Size erase(DynamicArray<T>& container, const U& value)
	{
		Size before = container.size();
		auto newEnd = std::remove(container.begin(), container.end(), value);
		container.erase(newEnd, container.end());
		return before - container.size();
	}

	/**
	* [EN]
	* Erases every element of container for which predicate is true and
	* returns the count removed. The ADL counterpart of std::erase_if -
	* call it unqualified as erase_if(container, predicate).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* container の中で predicate が真になる全要素を削除し、削除数を返す。
	* std::erase_if の ADL 版 - erase_if(container, predicate) と非修飾で
	* 呼ぶ。
	*/
	template<typename T, typename Predicate>
	Size erase_if(DynamicArray<T>& container, Predicate predicate)
	{
		Size before = container.size();
		auto newEnd = std::remove_if(container.begin(), container.end(), predicate);
		container.erase(newEnd, container.end());
		return before - container.size();
	}
}
