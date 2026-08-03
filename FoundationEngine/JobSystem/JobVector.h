#pragma once
#include <FoundationEngine/Utility/Types.h>

#include <cstddef>
#include <iterator>
#include <memory>
#include <vector>

namespace SeedCore
{
	/**
	* [EN]
	* Forward declaration; defined in JobVector.cpp. Rounds array up to
	* the next power of two (used to size new backing storage).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 前方宣言。JobVector.cpp で定義される。array を次の2の冪へ切り上げる
	* （新しい裏付けストレージのサイズ決定に使う）。
	*/
	inline Uint64 ArrayNextCapacity(Uint64 array);

	/**
	* [EN]
	* Trait selecting the "POD-like" storage/growth strategy: true for
	* standard-layout, trivial types, which can be moved/copied/destroyed
	* via raw memory operations (memcpy/realloc) instead of per-element
	* constructor/destructor calls. Drives which JobVectorTemplateBase
	* specialization JobVectorImplementation inherits from.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 「POD的」なストレージ/成長戦略を選択するトレイト: standard-layout かつ
	* trivial な型であれば true になり、要素ごとのコンストラクタ/デストラクタ
	* 呼び出しではなく、生のメモリ操作（memcpy/realloc）で移動/コピー/破棄
	* できる。JobVectorImplementation がどの JobVectorTemplateBase 特殊化を
	* 継承するかを決定する。
	*/
	template<typename T>
	struct IsPod :std::integral_constant<Bool, std::is_standard_layout<T>::value&& std::is_trivial<T>::value>
	{
		/// No Code
	};

	/**
	* [EN]
	* Type-erased (no template parameter on T) base for JobVector, holding
	* only the three raw pointers (begin_/end_/capacity_) that describe
	* the buffer. Factoring this out of the templated layers keeps
	* non-inline logic (like grow_pod) from being duplicated per T.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobVector の型消去された（T をテンプレートパラメータに持たない）
	* 基底クラス。バッファを記述する3本の生ポインタ（begin_/end_/capacity_）
	* のみを保持する。これをテンプレート層から切り出すことで、
	* （grow_pod のような）インライン化されないロジックが T ごとに
	* 重複するのを防ぐ。
	*/
	class SEEDCORE_API JobVectorBase
	{
	protected:
		/// [EN] Pointer to the first element (or to inline storage if not yet grown onto the heap).
		/// [JP] 最初の要素へのポインタ（ヒープへ成長する前はインラインストレージを指す）。
		void* begin_;

		/// [EN] Pointer one past the last live element.
		/// [JP] 最後の有効要素の1つ先を指すポインタ。
		void* end_;

		/// [EN] Pointer one past the end of allocated storage.
		/// [JP] 確保済みストレージの終端の1つ先を指すポインタ。
		void* capacity_;

	protected:
		/**
		* [EN]
		* Constructs pointing at firstElement (typically the derived
		* class's inline storage) with size bytes of initial capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* firstElement（通常は派生クラスのインラインストレージ）を指し、
		* size バイト分の初期容量で構築する。
		*/
		JobVectorBase(void* firstElement, Size size);

		/**
		* [EN]
		* POD-path buffer growth: reallocates (or, if still on inline
		* storage identified by firstElement, mallocs+copies) to at least
		* minSizeInBytes, growing geometrically by roughly 2x + size otherwise.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* POD経路でのバッファ成長: 少なくとも minSizeInBytes まで
		* 再確保する（まだインラインストレージ上であれば、firstElement で
		* 判定し malloc+コピーする）。それ以外の場合は概ね2倍+size で
		* 幾何的に成長する。
		*/
		void grow_pod(void* firstElement, Size minSizeInBytes, Size size);

	public:
		/**
		* [EN]
		* Returns the number of live bytes currently stored (end_ - begin_).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在格納されている有効バイト数を返す（end_ - begin_）。
		*/
		Size size_in_byte()const;

		/**
		* [EN]
		* Returns the total allocated capacity in bytes (capacity_ - begin_).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 確保済みの総容量をバイト単位で返す（capacity_ - begin_）。
		*/
		Size capacity_in_byte()const;

		/**
		* [EN]
		* Returns whether there are currently no live elements.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在有効な要素が無いかどうかを返す。
		*/
		Bool empty()const;
	};

	/**
	* [EN]
	* Forward declaration of the inline-storage holder used by
	* JobVector<T, N> (defined below).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobVector<T, N> が使うインラインストレージ保持クラスの前方宣言
	* （下で定義）。
	*/
	template<typename T, Unsigned N>
	struct JobVectorStorage;

	/**
	* [EN]
	* Typed (but capacity-independent) layer over JobVectorBase: adds
	* T-aware iterators/accessors (begin/end/size/operator[]/etc.) shared
	* by both the POD and non-POD growth strategies, plus the raw
	* T-sized-and-aligned "firstElement" storage that anchors a
	* freshly-constructed, not-yet-grown JobVector's inline buffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobVectorBase の上に構築される、型付き（だが容量には依存しない）層。
	* POD/非PODどちらの成長戦略でも共有される、T を意識したイテレータ/
	* アクセサ（begin/end/size/operator[] など）を追加する。また、構築直後で
	* まだ成長していない JobVector のインラインバッファの起点となる、
	* T のサイズ/アラインメントに合わせた生ストレージ「firstElement」も持つ。
	*/
	template<typename T, typename = void>
	class JobVectorTemplateCommon :public JobVectorBase
	{
	private:
		template<typename, Unsigned>
		friend struct JobVectorStorage;

		/**
		* [EN]
		* A byte buffer sized/aligned to hold one T, used only as the
		* anchor address for "is this still pointing at inline storage?" checks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T を1個保持できるサイズ/アラインメントを持つバイトバッファ。
		* 「まだインラインストレージを指しているか」判定の基準アドレスとして
		* のみ使う。
		*/
		template<typename X>
		struct AlignedUnionType
		{
			/// [EN] Buffer size: at least one byte, or sizeof(X) if larger.
			/// [JP] バッファサイズ: 最低1バイト、X が大きければ sizeof(X)。
			static constexpr Size maxSize = (sizeof(std::byte) > sizeof(X)) ? sizeof(std::byte) : sizeof(X);

			/// [EN] The raw, T-aligned byte storage.
			/// [JP] 生の、T にアラインされたバイトストレージ。
			alignas(X) std::byte buffer_[maxSize];
		};

		typedef AlignedUnionType<T> U;

		/// [EN] Anchor address representing "still using inline storage" (see is()).
		/// [JP] 「まだインラインストレージを使用中」を表す基準アドレス（is() を参照）。
		U firstElement;

	protected:
		/**
		* [EN]
		* Constructs, delegating to JobVectorBase with &firstElement as
		* the initial buffer and size as its byte capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* &firstElement を初期バッファ、size をそのバイト容量として
		* JobVectorBase へ委譲して構築する。
		*/
		JobVectorTemplateCommon(Size size) :JobVectorBase(&firstElement, size)
		{
			/// No Code
		}

		/**
		* [EN]
		* Forwards to JobVectorBase::grow_pod with &firstElement as the
		* inline-storage anchor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* &firstElement をインラインストレージの基準として
		* JobVectorBase::grow_pod へ転送する。
		*/
		void grow_pod(Size minSizeInBytes, Size size)
		{
			JobVectorBase::grow_pod(&firstElement, minSizeInBytes, size);
		}

		/**
		* [EN]
		* Returns whether the buffer still points at the inline
		* firstElement storage (i.e. has never grown onto the heap).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バッファがまだインラインの firstElement ストレージを指しているか
		* （すなわちヒープへ一度も成長していないか）を返す。
		*/
		Bool is()const
		{
			return begin_ == static_cast<const void*>(&firstElement);
		}

		/**
		* [EN]
		* Resets begin_/end_/capacity_ back to the inline firstElement
		* storage, without freeing or destroying anything (caller's
		* responsibility beforehand).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* begin_/end_/capacity_ をインラインの firstElement ストレージへ
		* 戻す。何かを解放/破棄することはない（事前に呼び出し側が行う責任がある）。
		*/
		void reset()
		{
			begin_ = end_ = capacity_ = &firstElement;
		}

		/**
		* [EN]
		* Sets end_ to P.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* end_ を P に設定する。
		*/
		void set_end(T* P)
		{
			this->end_ = P;
		}

	public:
		using size_type = Size;
		using difference_type = std::ptrdiff_t;
		using value_type = T;

		using Iterator = T*;
		using ConstIterator = const T*;

		using ReverseIterator = std::reverse_iterator<Iterator>;
		using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

		using reference = T&;
		using const_reference = const T&;

		using pointer = T*;
		using const_pointer = const T*;

		/// [EN] Returns an iterator to the first element.
		/// [JP] 最初の要素を指すイテレータを返す。
		inline Iterator begin()
		{
			return (Iterator)this->begin_;
		}

		/// [EN] Const overload of begin().
		/// [JP] begin() の const オーバーロード。
		inline ConstIterator begin()const
		{
			return (ConstIterator)this->begin_;
		}

		/// [EN] Returns the past-the-end iterator.
		/// [JP] 終端の次を指すイテレータを返す。
		inline Iterator end()
		{
			return (Iterator)this->end_;
		}

		/// [EN] Const overload of end().
		/// [JP] end() の const オーバーロード。
		inline ConstIterator end()const
		{
			return (ConstIterator)this->end_;
		}

	protected:
		/// [EN] Returns a pointer one past the end of allocated storage.
		/// [JP] 確保済みストレージの終端の1つ先を指すポインタを返す。
		Iterator capacity_ptr()
		{
			return (Iterator)this->capacity_;
		}

		/// [EN] Const overload of capacity_ptr().
		/// [JP] capacity_ptr() の const オーバーロード。
		ConstIterator capacity_ptr()const
		{
			return (ConstIterator)this->capacity_;
		}

	public:
		/// [EN] Returns a reverse iterator to the last element.
		/// [JP] 最後の要素を指す逆イテレータを返す。
		ReverseIterator reverse_begin()
		{
			return ReverseIterator(begin());
		}

		/// [EN] Const overload of reverse_begin().
		/// [JP] reverse_begin() の const オーバーロード。
		ConstReverseIterator reverse_begin()const
		{
			return ConstReverseIterator(begin());
		}

		/// [EN] Returns the reverse past-the-end iterator.
		/// [JP] 逆順走査における終端の次を指すイテレータを返す。
		ReverseIterator reverse_end()
		{
			return ReverseIterator(end());
		}

		/// [EN] Const overload of reverse_end().
		/// [JP] reverse_end() の const オーバーロード。
		ConstReverseIterator reverse_end()const
		{
			return ConstReverseIterator(end());
		}

		/// [EN] Returns the number of live elements.
		/// [JP] 有効な要素数を返す。
		inline size_type size()const
		{
			return end() - begin();
		}

		/// [EN] Returns the theoretical maximum number of elements addressable by size_type.
		/// [JP] size_type で表現可能な、理論上の最大要素数を返す。
		inline size_type max_size()const
		{
			return size_type(-1) / sizeof(T);
		}

		/// [EN] Returns the number of elements the current buffer can hold before regrowing.
		/// [JP] 再成長せずに現在のバッファが保持できる要素数を返す。
		Size capacity()const
		{
			return capacity_ptr() - begin();
		}

		/// [EN] Returns a pointer to the underlying element storage.
		/// [JP] 基盤となる要素ストレージへのポインタを返す。
		pointer data()
		{
			return pointer(begin());
		}

		/// [EN] Const overload of data().
		/// [JP] data() の const オーバーロード。
		const_pointer data()const
		{
			return const_pointer(begin());
		}

		/// [EN] Returns a reference to the element at index (no bounds check).
		/// [JP] index の要素への参照を返す（範囲チェックなし）。
		inline reference operator[](size_type index)
		{
			return begin()[index];
		}

		/// [EN] Const overload of operator[].
		/// [JP] operator[] の const オーバーロード。
		inline const_reference operator[](size_type index)const
		{
			return begin()[index];
		}

		/// [EN] Returns a reference to the first element.
		/// [JP] 最初の要素への参照を返す。
		reference front()
		{
			return begin()[0];
		}

		/// [EN] Const overload of front().
		/// [JP] front() の const オーバーロード。
		const_reference front()const
		{
			return begin()[0];
		}

		/// [EN] Returns a reference to the last element.
		/// [JP] 最後の要素への参照を返す。
		reference back()
		{
			return end()[-1];
		}

		/// [EN] Const overload of back().
		/// [JP] back() の const オーバーロード。
		const_reference back()const
		{
			return end()[-1];
		}
	};

	/**
	* [EN]
	* Non-POD growth/lifetime strategy: moves/copies/destroys elements
	* one at a time via their constructors/destructors, since raw memory
	* operations aren't safe for non-trivial T.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 非PODな成長/生存期間戦略: 生のメモリ操作は非trivialな T には安全で
	* ないため、要素をコンストラクタ/デストラクタ経由で1個ずつ移動/
	* コピー/破棄する。
	*/
	template<typename T, Bool IsPodLike>
	class JobVectorTemplateBase :public JobVectorTemplateCommon<T>
	{
	protected:
		/**
		* [EN]
		* Constructs, delegating size (in bytes) to JobVectorTemplateCommon.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* size（バイト単位）を JobVectorTemplateCommon へ委譲して構築する。
		*/
		JobVectorTemplateBase(Size size) :JobVectorTemplateCommon<T>(size)
		{
			/// No Code
		}

		/**
		* [EN]
		* Destroys every live element in [storage, element) in reverse order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [storage, element) 範囲の有効な要素を逆順に全て破棄する。
		*/
		static void destroy_range(T* storage, T* element)
		{
			while (storage != element)
			{
				--element;
				element->~T();
			}
		}

		/**
		* [EN]
		* Move-constructs elements from [a, element) into uninitialized
		* storage starting at destination.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [a, element) の要素を、destination から始まる未初期化ストレージへ
		* ムーブ構築する。
		*/
		template<typename A, typename B>
		void uninitialized_move(A a, A element, B destination)
		{
			std::uninitialized_copy(std::make_move_iterator(a), std::make_move_iterator(element), destination);
		}

		/**
		* [EN]
		* Copy-constructs elements from [a, element) into uninitialized
		* storage starting at destination.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [a, element) の要素を、destination から始まる未初期化ストレージへ
		* コピー構築する。
		*/
		template<typename A, typename B>
		void uninitialized_copy(A a, A element, B destination)
		{
			std::uninitialized_copy(a, element, destination);
		}

		/**
		* [EN]
		* Grows the buffer geometrically (at least doubling, per
		* ArrayNextCapacity), or to minSize if larger: allocates new
		* storage, move-constructs the live elements into it, destroys and
		* frees the old storage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バッファを幾何的に成長させる（ArrayNextCapacity により最低でも
		* 倍増）。minSize がそれより大きければ minSize まで。新しい
		* ストレージを確保し、有効な要素をそこへムーブ構築し、古い
		* ストレージを破棄・解放する。
		*/
		void grow(Size minSize = 0)
		{
			Size currentCapacity = this->capacity();
			Size currentSize = this->size();
			Size newCapacity = Size(ArrayNextCapacity(currentCapacity + 2));
			if (newCapacity < minSize)
			{
				newCapacity = minSize;
			}

			T* newElement = static_cast<T*>(std::malloc(newCapacity * sizeof(T)));

			this->uninitialized_move(this->begin(), this->end(), newElement);

			destroy_range(this->begin(), this->end());

			if (!this->is())
			{
				std::free(this->begin());
			}

			this->set_end(newElement + currentSize);
			this->begin_ = newElement;
			this->capacity_ = this->begin() + newCapacity;
		}

	public:
		/**
		* [EN]
		* Copy-constructs element onto the end, growing first if the
		* buffer is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* element を末尾へコピー構築する。バッファが満杯であれば先に成長する。
		*/
		void push_back(const T& element)
		{
			if ((this->end >= this->capacity_)) [[unlikely]]
			{
				this->grow();
			}

			::new((void*)this->end()) T(element);
			this->set_end(this->end() + 1);
		}

		/**
		* [EN]
		* Move-constructs element onto the end, growing first if the
		* buffer is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* element を末尾へムーブ構築する。バッファが満杯であれば先に成長する。
		*/
		void push_back(T&& element)
		{
			if ((this->end >= this->capacity_)) [[unlikely]]
			{
				this->grow();
			}

			::new((void*)this->end()) T(::std::move(element));
			this->set_end(this->end() + 1);
		}

		/**
		* [EN]
		* Destroys and removes the last element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最後の要素を破棄して削除する。
		*/
		void pop_back()
		{
			this->set_end(this->end() - 1);
			this->end()->~T();
		}
	};

	/**
	* [EN]
	* POD growth/lifetime strategy specialization: elements have no
	* constructor/destructor to run, so moves/copies/growth use raw
	* memory operations (memcpy/realloc) instead of per-element calls.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* POD向けの成長/生存期間戦略の特殊化。要素には実行すべき
	* コンストラクタ/デストラクタが無いため、移動/コピー/成長は要素ごとの
	* 呼び出しではなく生のメモリ操作（memcpy/realloc）を使う。
	*/
	template<typename T>
	class JobVectorTemplateBase<T, true> :public JobVectorTemplateCommon<T>
	{
	protected:
		/**
		* [EN]
		* Constructs, delegating size (in bytes) to JobVectorTemplateCommon.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* size（バイト単位）を JobVectorTemplateCommon へ委譲して構築する。
		*/
		JobVectorTemplateBase(Size size) :JobVectorTemplateCommon<T>(size)
		{
			/// No Code
		}

		/**
		* [EN]
		* No-op: POD elements have nothing to destroy.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 何もしない: POD要素は破棄すべきものを持たない。
		*/
		static void destroy_range(T*, T*)
		{
			/// No Code
		}

		/**
		* [EN]
		* Forwards to uninitialized_copy (a raw-memory move is just a copy for PODs).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* uninitialized_copy へ転送する（PODにとって生メモリのムーブは
		* コピーと同じ）。
		*/
		template<typename A, typename B>
		static void uninitialized_move(A a, A element, B destination)
		{
			uninitialized_copy(a, element, destination);
		}

		/**
		* [EN]
		* Generic fallback: copy-constructs elements from [a, element)
		* into uninitialized storage starting at destination.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 汎用フォールバック: [a, element) の要素を、destination から
		* 始まる未初期化ストレージへコピー構築する。
		*/
		template<typename A, typename B>
		static void uninitialized_copy(A a, A element, B destination)
		{
			std::uninitialized_copy(a, element, destination);
		}

		/**
		* [EN]
		* Fast path for matching pointer types: bulk-copies [a, element)
		* to destination via memcpy instead of per-element construction.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 型が一致するポインタ向けの高速経路: 要素ごとの構築ではなく
		* memcpy によって [a, element) を destination へ一括コピーする。
		*/
		template<typename A, typename B>
		static void uninitialized_copy(A* a, A* element, B* destination, typename std::enable_if<std::is_same<typename std::remove_const<A>::type, B>::value>::type* = nullptr)
		{
			if (a != element)
			{
				std::memcpy(destination, a, (element - a) * sizeof(T));
			}
		}

		/**
		* [EN]
		* Grows the buffer via JobVectorTemplateCommon::grow_pod, which
		* reallocates in place (no per-element move needed for PODs).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* JobVectorTemplateCommon::grow_pod 経由でバッファを成長させる。
		* その場で再確保する（PODには要素ごとのムーブは不要）。
		*/
		void grow(Size minSize = 0)
		{
			this->grow_pod(minSize * sizeof(T), sizeof(T));
		}

	public:
		/**
		* [EN]
		* Copies element onto the end via memcpy, growing first if the
		* buffer is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* element を memcpy で末尾へコピーする。バッファが満杯であれば
		* 先に成長する。
		*/
		void push_back(const T& element)
		{
			if ((this->end_ >= this->capacity_)) [[unlikely]]
			{
				this->grow();
			}
			memcpy(this->end(), &element, sizeof(T));
			this->set_end(this->end() + 1);
		}

		/**
		* [EN]
		* Removes the last element (no destructor call needed for PODs).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最後の要素を削除する（PODにはデストラクタ呼び出しは不要）。
		*/
		void pop_back()
		{
			this->set_end(this->end() - 1);
		}
	};

	/**
	* [EN]
	* Capacity-independent vector implementation shared by every
	* JobVector<T, N> regardless of N: the full std::vector-like API
	* (resize/reserve/insert/erase/append/assign/swap/operators/etc.),
	* built on top of JobVectorTemplateBase's growth strategy. Non-copyable
	* directly (the copy constructor is deleted) so it's only ever used
	* through the JobVector<T, N> wrapper, which supplies the inline storage.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* N に関わらず全ての JobVector<T, N> が共有する、容量に依存しない
	* ベクタ実装。JobVectorTemplateBase の成長戦略の上に構築された、
	* std::vector 相当のフルAPI（resize/reserve/insert/erase/append/
	* assign/swap/演算子など）を提供する。それ自体は直接コピー不可
	* （コピーコンストラクタは削除済み）で、インラインストレージを
	* 供給する JobVector<T, N> ラッパー経由でのみ使われる。
	*/
	template<typename T>
	class JobVectorImplementation :public JobVectorTemplateBase<T, IsPod<T>::value>
	{
	private:
		typedef JobVectorTemplateBase<T, IsPod<T>::value> SuperClass;

		JobVectorImplementation(const JobVectorImplementation&) = delete;

	public:
		typedef typename SuperClass::Iterator Iterator;
		typedef typename SuperClass::ConstIterator ConstIterator;
		typedef typename SuperClass::size_type size_type;

	protected:
		/**
		* [EN]
		* Constructs with n bytes of initial (inline) capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n バイトの初期（インライン）容量で構築する。
		*/
		explicit JobVectorImplementation(Unsigned n) :JobVectorTemplateBase<T, IsPod<T>::value>(n * sizeof(T))
		{
			/// No Code
		}

	public:
		/**
		* [EN]
		* Destroys every live element and, if the buffer had grown onto
		* the heap, frees it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全ての有効要素を破棄し、バッファがヒープへ成長していれば解放する。
		*/
		~JobVectorImplementation()
		{
			this->destroy_range(this->begin(), this->end());

			if (!this->is())
			{
				std::free(this->begin());
			}
		}

		/**
		* [EN]
		* Destroys every element, leaving the vector empty (capacity unchanged).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全要素を破棄し、ベクタを空にする（容量は変わらない）。
		*/
		void clear()
		{
			this->destroy_range(this->begin(), this->end());
			this->end_ = this->begin_;
		}

		/**
		* [EN]
		* Resizes to n elements: destroys the trailing elements if
		* shrinking, or default-constructs new ones (growing the buffer
		* first if needed) if growing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n 要素にリサイズする: 縮小する場合は末尾の要素を破棄し、拡大する
		* 場合は新しい要素をデフォルト構築する（必要ならまずバッファを成長させる）。
		*/
		void resize(size_type n)
		{
			if (n < this->size())
			{
				this->destroy_range(this->begin() + n, this->end());
				this->set_end(this->begin() + n);
			}
			else if (n > this->size())
			{
				if (this->capacity() < n)
				{
					this->grow(n);
					for (auto index = this->end(), element = this->begin() + n;index != element;++index)
					{
						new(&*index)T();
						this->set_end(this->begin() + n);
					}
				}
			}
		}

		/**
		* [EN]
		* Resizes to n elements: destroys the trailing elements if
		* shrinking, or copy-constructs new ones from nv (growing the
		* buffer first if needed) if growing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n 要素にリサイズする: 縮小する場合は末尾の要素を破棄し、拡大する
		* 場合は nv から新しい要素をコピー構築する（必要ならまずバッファを
		* 成長させる）。
		*/
		void resize(size_type n, const T& nv)
		{
			if (n < this->size())
			{
				this->destroy_range(this->begin() + n, this->end());
				this->set_end(this->begin() + n);
			}
			else if (n > this->size())
			{
				if (this->capacity() < n)
				{
					this->grow(n);
				}
				std::uninitialized_fill(this->end(), this->begin() + n, nv);
				this->set_end(this->begin() + n);
			}
		}

		/**
		* [EN]
		* Ensures capacity for at least n elements, growing the buffer if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 少なくとも n 要素分の容量を保証する。必要ならバッファを成長させる。
		*/
		void reserve(size_type n)
		{
			if (this->capacity() < n)
			{
				this->grow(n);
			}
		}

		/**
		* [EN]
		* Move-extracts and returns the last element, then removes it (see pop_back).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最後の要素をムーブして取り出して返し、それを削除する（pop_back を参照）。
		*/
		T pop_back_value()
		{
			T result = ::std::move(this->back());
			this->pop_back();
			return result;
		}

		/**
		* [EN]
		* Exchanges contents with rhs. Swaps raw pointers directly when
		* both sides have already grown onto the heap; otherwise falls
		* back to an element-wise exchange (growing either side as needed).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs と内容を交換する。両者が既にヒープへ成長していれば生
		* ポインタを直接スワップする。そうでなければ要素ごとの交換に
		* フォールバックする（必要に応じてどちらかを成長させる）。
		*/
		void swap(JobVectorImplementation& rhs)
		{
			if (this == &rhs)
			{
				return;
			}

			if (!this->is() && !rhs.is())
			{
				std::swap(this->begin_, rhs.begin_);
				std::swap(this->end_, rhs.end_);
				std::swap(this->capacity_, rhs.capacity_);
				return;
			}

			if (rhs.size() > this->capacity())
			{
				this->grow(rhs.size());
			}

			if (this->size() > rhs.capacity())
			{
				rhs.grow(this->size());
			}

			Size numberShared = this->size();
			if (numberShared > rhs.size())
			{
				numberShared = rhs.size();
			}

			for (size_type index = 0;index != numberShared;++index)
			{
				std::swap((*this)[index], rhs[index]);
			}

			if (this->size() > rhs.size())
			{
				Size elementDefference = this->size() - rhs.size();
				this->uninitialized_copy(this->begin() + numberShared, this->end(), rhs.end());
				rhs.set_end(rhs.end() + elementDefference);
				this->destroy_range(this->begin() + numberShared, this->end());
				this->set_end(this->begin() + numberShared);
			}
			else if (rhs.size() > this->size())
			{
				Size elementDefference = rhs.size() - this->size();
				this->uninitialized_copy(rhs.begin() + numberShared, rhs.end(), this->end());
				this->set_end(this->end() + elementDefference);
				this->destroy_range(rhs.begin() + numberShared, rhs.end());
				rhs.set_end(rhs.begin() + numberShared);
			}
		}

		/**
		* [EN]
		* Appends copies of every element in [inStart, inEnd) to the end,
		* growing the buffer first if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [inStart, inEnd) の全要素のコピーを末尾へ追加する。必要なら
		* 先にバッファを成長させる。
		*/
		template<typename InIterator>
		void append(InIterator inStart, InIterator inEnd)
		{
			size_type numberInputs = std::distance(inStart, inEnd);

			if (numberInputs > size_type(this->capacity_ptr() - this->end()))
			{
				this->grow(this->size() + numberInputs);
			}
			this->uninitialized_copy(inStart, inEnd, this->end());
			this->set_end(this->end() + numberInputs);
		}

		/**
		* [EN]
		* Appends numberInputs copies of element to the end, growing the
		* buffer first if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* element のコピーを numberInputs 個、末尾へ追加する。必要なら
		* 先にバッファを成長させる。
		*/
		void append(size_type numberInputs, const T& element)
		{
			if (numberInputs > SizeType(this->capacity_ptr() - this->end()))
			{
				this->grow(this->size() + numberInputs);
			}
			std::uninitialized_fill_n(this->end(), numberInputs, element);
			this->set_end(this->end() + numberInputs);
		}

		/**
		* [EN]
		* Appends every element of initialList to the end (see append).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* initialList の全要素を末尾へ追加する（append を参照）。
		*/
		void append(std::initializer_list<T> initialList)
		{
			append(initialList.begin(), initialList.end());
		}

		/**
		* [EN]
		* Clears the vector, then fills it with numberElments copies of element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ベクタをクリアし、element のコピーを numberElments 個で埋める。
		*/
		void assign(size_type numberElments, const T& element)
		{
			clear();
			if (this->capacity() < numberElments)
			{
				this->grow(numberElments);
			}
			this->set_end(this->begin() + numberElments);
			std::uninitialized_fill(this->begin(), this->end(), element);
		}

		/**
		* [EN]
		* Clears the vector, then fills it with a copy of initialList (see append).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ベクタをクリアし、initialList のコピーで埋める（append を参照）。
		*/
		void assign(std::initializer_list<T> initialList)
		{
			clear();
			append(initialList);
		}

		/**
		* [EN]
		* Removes the single element at cIterator, shifting subsequent
		* elements down by one. Returns an iterator to the element that
		* now occupies that position.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cIterator の位置の要素を1つ削除し、それ以降の要素を1つ前へ
		* 詰める。その位置に新たに来た要素へのイテレータを返す。
		*/
		Iterator erase(ConstIterator cIterator)
		{
			Iterator iterator = const_cast<Iterator>(cIterator);

			Iterator n = iterator;
			std::move(iterator + 1, this->end(), iterator);

			this->pop_back();
			return(n);
		}

		/**
		* [EN]
		* Removes every element in [cSize, cElement), shifting subsequent
		* elements down. Returns an iterator to the element that now
		* occupies the start of the removed range.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [cSize, cElement) の全要素を削除し、それ以降の要素を前へ詰める。
		* 削除範囲の先頭に新たに来た要素へのイテレータを返す。
		*/
		Iterator erase(ConstIterator cSize, ConstIterator cElement)
		{
			Iterator size = const_cast<Iterator>(cSize);
			Iterator element = const_cast<Iterator>(cElement);

			Iterator n = size;
			Iterator iterator = std::move(element, this->end(), size);

			this->destroy_range(iterator, this->end());
			this->set_end(iterator);
			return(n);
		}

		/**
		* [EN]
		* Inserts element (moved) at iterator, shifting subsequent
		* elements up by one (growing the buffer first if needed).
		* Returns an iterator to the inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator の位置に element を（ムーブして）挿入し、それ以降の
		* 要素を1つ後ろへずらす（必要なら先にバッファを成長させる）。
		* 挿入された要素へのイテレータを返す。
		*/
		Iterator insert(Iterator iterator, T&& element)
		{
			if (iterator == this->end())
			{
				this->push_back(::std::move(element));
				return this->end() - 1;
			}

			if (this->end_ >= this->capacity_)
			{
				Size elementNo = iterator - this->begin();
				this->grow();
				iterator = this->begin() + elementNo;
			}

			::new((void*)this->end())T(::std::move(this->back()));
			std::move_backward(iterator, this->end() - 1, this->end());
			this->set_end(this->end() + 1);

			T* elementPtr = &element;
			if (iterator <= elementPtr && elementPtr < this->end_)
			{
				++elementPtr;
			}

			*iterator = std::move(*elementPtr);
			return iterator;
		}

		/**
		* [EN]
		* Inserts a copy of element at iterator, shifting subsequent
		* elements up by one (growing the buffer first if needed).
		* Returns an iterator to the inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator の位置に element のコピーを挿入し、それ以降の要素を
		* 1つ後ろへずらす（必要なら先にバッファを成長させる）。挿入された
		* 要素へのイテレータを返す。
		*/
		Iterator insert(Iterator iterator, const T& element)
		{
			if (iterator == this->end())
			{
				this->push_back(element);
				return this->end() - 1;
			}

			if (this->end_ >= this->capacity_)
			{
				Size elementNamber = iterator - this->begin();
				this->grow();
				iterator = this->begin() + elementNamber;
			}

			::new((void*)this->end())T(std::move(this->back()));
			std::move_backward(iterator, this->end() - 1, this->end());
			this->set_end(this->end() + 1);

			const T* elementPtr = &element;
			if (iterator <= elementPtr && elementPtr < this->end_)
			{
				++elementPtr;
			}

			*iterator = *elementPtr;
			return iterator;
		}

		/**
		* [EN]
		* Inserts numberInsert copies of element at iterator, shifting
		* subsequent elements up (reserving capacity first). Returns an
		* iterator to the first inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator の位置に element のコピーを numberInsert 個挿入し、
		* それ以降の要素を後ろへずらす（先に容量を確保する）。最初に
		* 挿入された要素へのイテレータを返す。
		*/
		Iterator insert(Iterator iterator, size_type numberInsert, const T& element)
		{
			Size insertElement = iterator - this->begin();

			if (iterator == this->end())
			{
				append(numberInsert, element);
				return this->begin() + insertElement;
			}

			reserve(this->size() + numberInsert);

			iterator = this->begin() + insertElement;

			if (Size(this->end() - iterator) >= numberInsert)
			{
				T* oldEnd = this->end();
				append(std::move_iterator<Iterator>(this->end() - numberInsert), std::move_iterator<Iterator>(this->end()));

				std::move_backward(iterator, oldEnd - numberInsert, oldEnd);

				std::fill_n(iterator, numberInsert, element);
				return iterator;
			}

			T* oldEnd = this->end();
			this->set_end(this->end() + numberInsert);
			Size numberOverwritten = oldEnd - iterator;
			this->uninitialized_move(iterator, oldEnd, this->end() - numberOverwritten);

			std::fill_n(iterator, numberOverwritten, element);

			std::uninitialized_fill_n(oldEnd, numberInsert - numberOverwritten, element);
			return iterator;
		}

		/**
		* [EN]
		* Inserts copies of every element in [from, to) at iterator,
		* shifting subsequent elements up (reserving capacity first).
		* Returns an iterator to the first inserted element.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator の位置に [from, to) の全要素のコピーを挿入し、それ
		* 以降の要素を後ろへずらす（先に容量を確保する）。最初に挿入された
		* 要素へのイテレータを返す。
		*/
		template<typename Ty>
		Iterator insert(Iterator iterator, Ty from, Ty to)
		{
			Size insertElement = iterator - this->begin();

			if (iterator == this->end())
			{
				append(from, to);
				return this->begin() + insertElement;
			}

			Size numberInsert = std::distance(from, to);

			reserve(this->size() + numberInsert);

			iterator = this->begin() + insertElement;

			if (Size(this->end() - iterator) >= numberInsert)
			{
				T* oldEnd = this->end();
				append(std::move_iterator<Iterator>(this->end() - numberInsert), std::move_iterator<Iterator>(this->end()));

				std::move_backward(iterator, oldEnd - numberInsert, oldEnd);

				std::copy(from, to, iterator);
				return iterator;
			}

			T* oldEnd = this->end();
			this->set_end(this->end() + numberInsert);
			Size numberOverwritten = oldEnd - iterator;
			this->uninitialized_move(iterator, oldEnd, this->end() - numberOverwritten);

			for (T* index = iterator;numberOverwritten > 0;--numberOverwritten)
			{
				*index = *from;
				++index;
				++from;
			}

			this->uninitialized_copy(from, to, oldEnd);
			return iterator;
		}

		/**
		* [EN]
		* Inserts a copy of initialList at iterator (see the from/to insert overload).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator の位置に initialList のコピーを挿入する
		* （from/to を取る insert オーバーロードを参照）。
		*/
		void insert(Iterator iterator, std::initializer_list<T> initialList)
		{
			insert(iterator, initialList.begin(), initialList.end());
		}

		/**
		* [EN]
		* Constructs a new element in place at the end from args, growing
		* the buffer first if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* args から末尾に新しい要素をその場で構築する。必要なら先に
		* バッファを成長させる。
		*/
		template<typename... Args>
		void emplace_back(Args&&... args)
		{
			if ((this->end_ >= this->capacity_)) [[unlikely]]
			{
				this->grow();
			}

			::new((void*)this->end())T(std::forward<Args>(args)...);
			this->set_end(this->end() + 1);
		}

		/**
		* [EN]
		* Copy-assigns from rhs: reuses live elements' storage via
		* operator= where possible, destroying any excess or
		* copy-constructing any shortfall (growing first if needed).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からコピー代入する: 可能な限り既存要素のストレージを
		* operator= で再利用し、超過分は破棄、不足分はコピー構築する
		* （必要なら先に成長する）。
		*/
		JobVectorImplementation<T>& operator=(const JobVectorImplementation& rhs)
		{
			if (this == &rhs)
			{
				return *this;
			}

			Size rhsSize = rhs.size();
			Size currentSize = this->size();
			if (currentSize >= rhsSize)
			{
				Iterator newEnd;
				if (rhsSize)
				{
					newEnd = std::copy(rhs.begin(), rhs.begin() + rhsSize, this->begin());
				}
				else
				{
					newEnd = this->begin();
				}

				this->destroy_range(newEnd, this->end());

				this->set_end(newEnd);
				return *this;
			}

			if (this->capacity() < rhsSize)
			{
				this->destroy_range(this->begin(), this->end());
				this->set_end(this->begin());
				currentSize = 0;
				this->grow(rhsSize);
			}
			else if (currentSize)
			{
				std::copy(rhs.begin(), rhs.begin() + currentSize, this->begin());
			}

			this->uninitialized_copy(rhs.begin() + currentSize, rhs.end(), this->begin() + currentSize);

			this->set_end(this->begin() + rhsSize);
			return *this;
		}

		/**
		* [EN]
		* Move-assigns from rhs: if rhs has grown onto the heap, steals
		* its buffer directly; otherwise falls back to an element-wise
		* move exchange (reusing storage via operator= where possible),
		* then clears rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からムーブ代入する: rhs が既にヒープへ成長していれば、
		* そのバッファを直接奪う。そうでなければ要素ごとのムーブ交換
		* （可能な限り operator= でストレージを再利用）にフォールバックし、
		* その後 rhs をクリアする。
		*/
		JobVectorImplementation<T>& operator=(JobVectorImplementation&& rhs)
		{
			if (this == &rhs)
			{
				return *this;
			}

			if (!rhs.is())
			{
				this->destroy_range(this->begin(), this->end());
				if (!this->is())
				{
					std::free(this->begin());
				}
				this->begin_ = rhs.begin_;
				this->end_ = rhs.end_;
				this->capacity_ = rhs.capacity_;
				rhs.reset();
				return *this;
			}

			Size rhsSize = rhs.size();
			Size currentSize = this->size();
			if (currentSize >= rhsSize)
			{
				Iterator newEnd = this->begin();
				if (rhsSize)
				{
					newEnd = std::move(rhs.begin(), rhs.end(), newEnd);
				}
				this->destroy_range(newEnd, this->end());
				this->set_end(newEnd);

				rhs.clear();

				return *this;
			}

			if (this->capacity() < rhsSize)
			{
				this->destroy_range(this->begin(), this->end());
				this->set_end(this->begin());
				currentSize = 0;
				this->grow(rhsSize);
			}
			else if (currentSize)
			{
				std::move(rhs.begin(), rhs.begin() + currentSize, this->begin());
			}

			this->uninitialized_move(rhs.begin() + currentSize, rhs.end(), this->begin() + currentSize);

			this->set_end(this->begin() + rhsSize);

			rhs.clear();
			return *this;
		}

		/**
		* [EN]
		* Returns whether this and rhs have equal size and elementwise-equal contents.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* this と rhs のサイズが等しく、要素ごとの内容も等しいかを返す。
		*/
		Bool operator==(const JobVectorImplementation& rhs)const
		{
			if (this->size() != rhs.size())
			{
				return false;
			}
			return std::equal(this->begin(), this->end(), rhs.begin());
		}

		/**
		* [EN]
		* Negation of operator==.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator== の否定。
		*/
		Bool operator!=(const JobVectorImplementation& rhs)const
		{
			return !(*this == rhs);
		}

		/**
		* [EN]
		* Returns whether this is lexicographically less than rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* this が rhs より辞書式に小さいかを返す。
		*/
		Bool operator<(const JobVectorImplementation& rhs)const
		{
			return std::lexicographical_compare(this->begin(), this->end(), rhs.begin(), rhs.end());
		}

		/**
		* [EN]
		* Sets the logical size directly without constructing/destroying
		* anything; intended only for advanced callers that have already
		* placed valid elements up to n themselves.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 何も構築/破棄せずに論理サイズを直接設定する。既に n 個分の
		* 有効な要素を自ら配置済みの上級者向け呼び出し専用。
		*/
		void set_size(size_type n)
		{
			this->set_end(this->begin(), n);
		}
	};

	/**
	* [EN]
	* Inline storage for N-1 extra elements beyond
	* JobVectorTemplateCommon's single firstElement slot (so a
	* JobVector<T, N> holds N elements inline in total).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobVectorTemplateCommon の単一 firstElement スロットに加えて、
	* N-1 個分の追加要素のインラインストレージ（これにより
	* JobVector<T, N> は合計 N 要素をインラインに保持する）。
	*/
	template<typename T, Unsigned N>
	struct JobVectorStorage
	{
		/// [EN] The N-1 additional inline element slots.
		/// [JP] N-1 個分の追加インライン要素スロット。
		typename JobVectorTemplateCommon<T>::U InlineElements[N - 1];
	};

	/**
	* [EN]
	* Specialization for N=1: no extra slots needed beyond firstElement.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* N=1 の特殊化: firstElement 以外に追加スロットは不要。
	*/
	template<typename T>
	struct JobVectorStorage<T, 1>
	{
		/// No Code
	};

	/**
	* [EN]
	* Specialization for N=0: no inline storage at all.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* N=0 の特殊化: インラインストレージを一切持たない。
	*/
	template<typename T>
	struct JobVectorStorage<T, 0>
	{
		/// No Code
	};

	/**
	* [EN]
	* Small-buffer-optimized vector: behaves like a std::vector<T> but
	* holds up to N elements inline (no heap allocation) before
	* transparently spilling onto the heap once it grows past that. This
	* is the project-wide HybridArray alias target (see FoundationEngine/Utility/Array.h).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 小バッファ最適化されたベクタ: std::vector<T> のように振る舞うが、
	* N 要素まではインラインに（ヒープ確保なしで）保持し、それを超えて
	* 成長すると透過的にヒープへ溢れる。プロジェクト共通の HybridArray
	* エイリアスの実体（FoundationEngine/Utility/Array.h を参照）。
	*/
	template<typename T, Unsigned N = 2>
	class JobVector :public JobVectorImplementation<T>
	{
	private:
		/// [EN] The N (or N-1, see JobVectorStorage) inline element slots beyond firstElement.
		/// [JP] firstElement に加えた N（または N-1、JobVectorStorage を参照）個分のインライン要素スロット。
		JobVectorStorage<T, N> storage_;

	public:
		/**
		* [EN]
		* Constructs an empty vector with N elements of inline capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* N 要素分のインライン容量を持つ、空のベクタを構築する。
		*/
		JobVector() :JobVectorImplementation<T>(N)
		{
			/// No Code
		}

		/**
		* [EN]
		* Constructs with size copies of value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーを size 個持つベクタを構築する。
		*/
		explicit JobVector(Size size, const T& value = T()) :JobVectorImplementation<T>(N)
		{
			this->assign(size, value);
		}

		/**
		* [EN]
		* Constructs from copies of every element in [storage, element).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [storage, element) の全要素のコピーから構築する。
		*/
		template<typename Ty>
		JobVector(Ty storage, Ty element) : JobVectorImplementation<T>(N)
		{
			this->append(storage, element);
		}

		/**
		* [EN]
		* Constructs from a copy of initialList.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* initialList のコピーから構築する。
		*/
		JobVector(std::initializer_list<T> initialList) :JobVectorImplementation<T>(N)
		{
			this->assign(initialList);
		}

		/**
		* [EN]
		* Copy-constructs from rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からコピー構築する。
		*/
		JobVector(const JobVector& rhs) :JobVectorImplementation<T>(N)
		{
			if (!rhs.empty())
			{
				JobVectorImplementation<T>::operator=(rhs);
			}
		}

		/**
		* [EN]
		* Move-constructs from rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からムーブ構築する。
		*/
		JobVector(JobVector&& rhs) :JobVectorImplementation<T>(N)
		{
			if (!rhs.empty())
			{
				JobVectorImplementation<T>::operator=(::std::move(rhs));
			}
		}

		/**
		* [EN]
		* Copy-assigns from rhs (see JobVectorImplementation::operator=).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からコピー代入する（JobVectorImplementation::operator= を参照）。
		*/
		const JobVector& operator=(const JobVector& rhs)
		{
			JobVectorImplementation<T>::operator=(rhs);
			return *this;
		}

		/**
		* [EN]
		* Move-assigns from rhs (see JobVectorImplementation::operator=).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs からムーブ代入する（JobVectorImplementation::operator= を参照）。
		*/
		const JobVector& operator=(JobVector&& rhs)
		{
			JobVectorImplementation<T>::operator=(::std::move(rhs));
			return *this;
		}

		/**
		* [EN]
		* Move-constructs from a plain JobVectorImplementation<T> rhs
		* (e.g. one with a different N).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 素の JobVectorImplementation<T> である rhs（例えば異なる N を持つもの）
		* からムーブ構築する。
		*/
		JobVector(JobVectorImplementation<T>&& rhs) :JobVectorImplementation<T>(N)
		{
			if (!rhs.empty())
			{
				JobVectorImplementation<T>::operator=(::std::move(rhs));
			}
		}

		/**
		* [EN]
		* Move-assigns from a plain JobVectorImplementation<T> rhs
		* (e.g. one with a different N).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 素の JobVectorImplementation<T> である rhs（例えば異なる N を持つもの）
		* からムーブ代入する。
		*/
		const JobVector& operator=(JobVectorImplementation<T>&& rhs)
		{
			JobVectorImplementation<T>::operator=(::std::move(rhs));
			return *this;
		}

		/**
		* [EN]
		* Assigns from a copy of initialList (see JobVectorImplementation::assign).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* initialList のコピーから代入する（JobVectorImplementation::assign を参照）。
		*/
		const JobVector& operator=(std::initializer_list<T> initialList)
		{
			this->assign(initialList);
			return *this;
		}

		/**
		* [EN]
		* Returns x's total allocated capacity in bytes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* x の確保済み総容量をバイト単位で返す。
		*/
		inline Size capacity_in_byte(const JobVector<T, N>& x)
		{
			return x.capacity_in_byte();
		}
	};
}

namespace std
{
	/// [EN] ADL swap for JobVectorImplementation, forwarding to its member swap().
	/// [JP] JobVectorImplementation 向けの ADL swap。メンバの swap() へ転送する。
	template<typename T>
	inline void swap(SeedCore::JobVectorImplementation<T>& lhs, SeedCore::JobVectorImplementation<T>& rhs)
	{
		lhs.swap(rhs);
	}

	/// [EN] ADL swap for JobVector, forwarding to its inherited swap().
	/// [JP] JobVector 向けの ADL swap。継承した swap() へ転送する。
	template<typename T, SeedCore::Unsigned N>
	inline void swap(SeedCore::JobVector<T, N>& lhs, SeedCore::JobVector<T, N>& rhs)
	{
		lhs.swap(rhs);
	}
}
