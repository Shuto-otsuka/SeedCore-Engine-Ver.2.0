#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Open-addressing hash map (linear probing) with an STL-like interface.
	* Capacity is always a power of two so bucket indices can be computed
	* with a bitmask instead of a modulo. Deleted slots are tombstoned
	* (SlotState::Deleted) rather than compacted immediately, so probing
	* keeps working after erase.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* オープンアドレス法（線形探査）によるハッシュマップ。STLライクな
	* インターフェースを持つ。容量は常に2の冪とし、バケットインデックスを
	* 剰余ではなくビットマスクで計算できるようにしている。削除済みスロットは
	* 即座に詰め直さずトゥームストーン（SlotState::Deleted）とするため、
	* erase 後も探査が正しく機能する。
	*/
	template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
	class FlatMap
	{
	public:
		/// [EN] Standard container typedefs, mirroring std::unordered_map's interface.
		/// [JP] std::unordered_map のインターフェースに準じた標準コンテナ型定義。
		using value_type = std::pair<Key, Value>;
		using key_type = Key;
		using mapped_type = Value;
		using size_type = Size;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using reference = value_type&;
		using const_reference = const value_type&;

	private:
		/// [EN] Occupancy state of a single bucket slot.
		/// [JP] 単一バケットスロットの占有状態。
		enum class SlotState :Uint8
		{
			/// [EN] Never used, or reset by clear(): probing stops here.
			/// [JP] 未使用、または clear() でリセット済み: 探査はここで停止する。
			Empty = 0,

			/// [EN] Holds a live key/value pair.
			/// [JP] 有効なキー/値ペアを保持している。
			Occupied = 1,

			/// [EN] Tombstone left by erase(): probing continues past it.
			/// [JP] erase() が残したトゥームストーン: 探査はこれを通過して続行する。
			Deleted = 2
		};

		/**
		* [EN]
		* A single bucket: its occupancy state plus the stored pair (if any).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単一バケット。占有状態と（あれば）格納されたペアを保持する。
		*/
		struct Slot
		{
			/// [EN] Current occupancy state.
			/// [JP] 現在の占有状態。
			SlotState state_ = SlotState::Empty;

			/// [EN] The stored key/value pair, engaged only when state_ is Occupied.
			/// [JP] 格納されたキー/値ペア。state_ が Occupied のときのみ値を持つ。
			std::optional<value_type> keyValue_;
		};

		/// [EN] Initial bucket count for a default-constructed map.
		/// [JP] デフォルト構築されたマップの初期バケット数。
		static constexpr size_type DefaultCapacity = 16;

		/// [EN] Occupancy ratio above which the table is grown (see ensure_capacity).
		/// [JP] これを超えるとテーブルを拡張する占有率のしきい値（ensure_capacity を参照）。
		static constexpr Float MaxLoadFactor = 0.75f;

		/// [EN] Backing bucket array; size is always a power of two.
		/// [JP] バケット配列本体。サイズは常に2の冪。
		DynamicArray<Slot> buckets_;

		/// [EN] Number of live (Occupied) entries.
		/// [JP] 有効（Occupied）なエントリ数。
		size_type size_ = 0;

		/// [EN] Number of non-Empty slots (Occupied + Deleted), used against MaxLoadFactor.
		/// [JP] 非Emptyなスロット数（Occupied + Deleted）。MaxLoadFactor との比較に使う。
		size_type occupied_ = 0;

		/// [EN] Hash functor.
		/// [JP] ハッシュ関数オブジェクト。
		Hash hasher_;

		/// [EN] Key equality functor.
		/// [JP] キー等価比較関数オブジェクト。
		KeyEqual keyEqual_;

		/**
		* [EN]
		* Returns the starting bucket index for key (its hash masked to
		* the current capacity).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key の探査開始バケットインデックスを返す（そのハッシュ値を
		* 現在の容量でマスクしたもの）。
		*/
		size_type bucket_index(const Key& key)const noexcept
		{
			return hasher_(key) & (buckets_.size() - 1);
		}

		/**
		* [EN]
		* Linearly probes from bucket_index(key) for a slot matching key.
		* Returns {index, true} if key is present at index. Returns
		* {index, false} with index pointing at the first Deleted slot seen
		* (or the first Empty slot / probe start if none), suitable for
		* inserting key.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* bucket_index(key) から key に一致するスロットを線形探査する。
		* key が index に存在すれば {index, true} を返す。存在しなければ
		* {index, false} を返し、index は挿入に適したスロット
		* （最初に見つかった Deleted スロット。なければ最初の Empty
		* スロット/探査開始位置）を指す。
		*/
		std::pair<size_type, Bool> probe(const Key& key)const noexcept
		{
			const size_type capacity = buckets_.size();
			const size_type bucketIndex = bucket_index(key);
			size_type firstDeleted = capacity;

			size_type index = bucketIndex;
			for (size_type step = 0; step < capacity; ++step)
			{
				const auto& slot = buckets_[index];

				switch (slot.state_)
				{
				case SlotState::Occupied:
					if (keyEqual_(slot.keyValue_->first, key))
					{
						return { index,true };
					}
					break;
				case SlotState::Deleted:
					if (firstDeleted == capacity)
					{
						firstDeleted = index;
					}
					break;
				case SlotState::Empty:
					[[fallthrough]];
				default:
					return { (firstDeleted != capacity) ? firstDeleted : index,false };
					break;
				}

				index = (index + 1) & (capacity - 1);
			}

			return { (firstDeleted != capacity) ? firstDeleted : bucketIndex,false };
		}

		/**
		* [EN]
		* Reallocates the bucket array to newCapacity and reinserts every
		* live entry (dropping tombstones), rebuilding size_/occupied_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バケット配列を newCapacity で再確保し、有効な全エントリを
		* （トゥームストーンを捨てて）再挿入し、size_/occupied_ を再構築する。
		*/
		void rehash(size_type newCapacity)
		{
			DynamicArray<Slot> oldBackets(newCapacity);
			std::swap(buckets_, oldBackets);
			size_ = 0;
			occupied_ = 0;

			for (auto& slot : oldBackets)
			{
				if (slot.state_ == SlotState::Occupied)
				{
					insert_implementation(std::move(*slot.keyValue_));
				}
			}
		}

		/**
		* [EN]
		* Shared worker for every insertion path: probes for keyValue's
		* key and, if absent, places it at the probed slot. Returns
		* {index, false} if the key already existed (keyValue is discarded),
		* or {index, true} if it was newly inserted.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全ての挿入経路が共有するワーカー。keyValue のキーを探査し、
		* 存在しなければ探査で見つかったスロットに配置する。キーが既に
		* 存在すれば {index, false}（keyValue は破棄される）、新規挿入なら
		* {index, true} を返す。
		*/
		std::pair<size_type, Bool>insert_implementation(value_type keyValue)
		{
			auto [index, found] = probe(keyValue.first);
			if (found)
			{
				return{ index, false };
			}

			if (buckets_[index].state_ != SlotState::Deleted)
			{
				++occupied_;
			}

			buckets_[index].state_ = SlotState::Occupied;
			buckets_[index].keyValue_ = std::move(keyValue);
			++size_;
			return { index,true };
		}

		/**
		* [EN]
		* Grows the table (doubling capacity via rehash) if inserting one
		* more entry would exceed MaxLoadFactor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* もう1エントリ挿入すると MaxLoadFactor を超える場合、テーブルを
		* （rehash で容量を倍にして）拡張する。
		*/
		void ensure_capacity()
		{
			if (static_cast<Float>(occupied_ + 1) > MaxLoadFactor * static_cast<Float>(buckets_.size()))
			{
				rehash(buckets_.size() * 2);
			}
		}

	public:
		struct ConstIterator;

		/**
		* [EN]
		* Forward iterator over FlatMap entries, skipping Empty/Deleted
		* slots. Iteration order follows bucket order, not insertion order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* FlatMap の要素を走査する前方イテレータ。Empty/Deleted スロットは
		* スキップする。走査順はバケット順であり、挿入順ではない。
		*/
		struct Iterator
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = FlatMap::value_type;
			using difference_type = FlatMap::difference_type;
			using pointer = value_type*;
			using reference = value_type&;

		private:
			using SlotVector = DynamicArray<Slot>;

			/// [EN] Backing bucket array this iterator walks.
			/// [JP] このイテレータが走査するバケット配列本体。
			SlotVector* slots_ = nullptr;

			/// [EN] Current bucket index.
			/// [JP] 現在のバケットインデックス。
			size_type index_ = 0;

			/**
			* [EN]
			* Advances index_ past any Empty/Deleted slots.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* index_ を Empty/Deleted スロットの先へ進める。
			*/
			void skip()
			{
				while (index_ < slots_->size() && (*slots_)[index_].state_ != SlotState::Occupied)
				{
					++index_;
				}
			}

		public:
			Iterator() = default;

			/**
			* [EN]
			* Constructs an iterator over slots starting at index,
			* immediately skipping to the first Occupied slot at or after it.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* slots に対する、index から始まるイテレータを構築する。
			* 直ちに index 以降で最初の Occupied スロットまでスキップする。
			*/
			explicit Iterator(SlotVector* slots, size_type index) :slots_(slots), index_(index)
			{
				skip();
			}

			/**
			* [EN]
			* Returns a reference to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの参照を返す。
			*/
			reference operator*()const
			{
				return *(*slots_)[index_].keyValue_;
			}

			/**
			* [EN]
			* Returns a pointer to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへのポインタを返す。
			*/
			pointer operator->()const
			{
				return &*(*slots_)[index_].keyValue_;
			}

			/**
			* [EN]
			* Advances to the next Occupied slot (pre-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 次の Occupied スロットへ進める（前置インクリメント）。
			*/
			Iterator& operator++()
			{
				++index_;
				skip();
				return *this;
			}

			/**
			* [EN]
			* Advances to the next Occupied slot (post-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 次の Occupied スロットへ進める（後置インクリメント）。
			*/
			Iterator operator++(Int)
			{
				auto temporary = *this;
				++(*this);
				return temporary;
			}

			/**
			* [EN]
			* Returns whether both iterators point to the same bucket index.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 両イテレータが同じバケットインデックスを指しているかを返す。
			*/
			Bool operator==(const Iterator& other)const noexcept
			{
				return index_ == other.index_;
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
			Bool operator!=(const Iterator& other)const noexcept
			{
				return !(*this == other);
			}

			/**
			* [EN]
			* Returns whether this iterator is dereferenceable (not past-the-end).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* このイテレータが逆参照可能か（終端を過ぎていないか）を返す。
			*/
			explicit operator Bool()const noexcept
			{
				return index_ < slots_->size();
			}

			/**
			* [EN]
			* Converts to a ConstIterator at the same position.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 同じ位置を指す ConstIterator に変換する。
			*/
			operator ConstIterator()const
			{
				return ConstIterator(slots_, index_);
			}
		};

		/**
		* [EN]
		* Const forward iterator over FlatMap entries (see Iterator).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* FlatMap の要素を走査する const 前方イテレータ（Iterator を参照）。
		*/
		struct ConstIterator
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = FlatMap::value_type;
			using difference_type = FlatMap::difference_type;
			using pointer = const value_type*;
			using reference = const value_type&;

		private:
			using SlotVector = const DynamicArray<Slot>;

			/// [EN] Backing bucket array this iterator walks.
			/// [JP] このイテレータが走査するバケット配列本体。
			SlotVector* slots_ = nullptr;

			/// [EN] Current bucket index.
			/// [JP] 現在のバケットインデックス。
			size_type index_ = 0;

			/**
			* [EN]
			* Advances index_ past any Empty/Deleted slots.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* index_ を Empty/Deleted スロットの先へ進める。
			*/
			void skip()
			{
				while (index_ < slots_->size() && (*slots_)[index_].state_ != SlotState::Occupied)
				{
					++index_;
				}
			}

		public:
			ConstIterator() = default;

			/**
			* [EN]
			* Constructs an iterator over slots starting at index,
			* immediately skipping to the first Occupied slot at or after it.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* slots に対する、index から始まるイテレータを構築する。
			* 直ちに index 以降で最初の Occupied スロットまでスキップする。
			*/
			explicit ConstIterator(SlotVector* slots, size_type index) :slots_(slots), index_(index)
			{
				skip();
			}

			/**
			* [EN]
			* Returns a const reference to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの const 参照を返す。
			*/
			reference operator*()const
			{
				return *(*slots_)[index_].keyValue_;
			}

			/**
			* [EN]
			* Returns a const pointer to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの const ポインタを返す。
			*/
			pointer operator->()const
			{
				return &*(*slots_)[index_].keyValue_;
			}

			/**
			* [EN]
			* Advances to the next Occupied slot (pre-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 次の Occupied スロットへ進める（前置インクリメント）。
			*/
			ConstIterator& operator++()
			{
				++index_;
				skip();
				return *this;
			}

			/**
			* [EN]
			* Advances to the next Occupied slot (post-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 次の Occupied スロットへ進める（後置インクリメント）。
			*/
			ConstIterator operator++(Int)
			{
				auto temporary = *this;
				++(*this);
				return temporary;
			}

			/**
			* [EN]
			* Returns whether both iterators point to the same bucket index.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 両イテレータが同じバケットインデックスを指しているかを返す。
			*/
			Bool operator==(const ConstIterator& other)const noexcept
			{
				return index_ == other.index_;
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
			Bool operator!=(const ConstIterator& other)const noexcept
			{
				return !(*this == other);
			}

			/**
			* [EN]
			* Returns whether this iterator is dereferenceable (not past-the-end).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* このイテレータが逆参照可能か（終端を過ぎていないか）を返す。
			*/
			explicit operator Bool()const noexcept
			{
				return index_ < slots_->size();
			}
		};

	public:
		FlatMap() :buckets_(DefaultCapacity)
		{
			/// No Code
		}

		/**
		* [EN]
		* Constructs from an initializer list, sizing the initial bucket
		* array to comfortably hold every entry, then inserting them in list order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 初期化子リストから構築する。初期バケット配列を全エントリを
		* 余裕を持って収容できるサイズにし、リスト順に挿入する。
		*/
		FlatMap(std::initializer_list<value_type> init) :buckets_(Max(DefaultCapacity, static_cast<size_type>(init.size() * 2)))
		{
			for (auto& keyValue : init)
			{
				insert(keyValue);
			}
		}

		/**
		* [EN]
		* Constructs from any input range of value_type-convertible
		* elements, inserting them in range order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value_type に変換可能な要素の任意の入力範囲から構築する。
		* 範囲の順に挿入する。
		*/
		template<std::ranges::input_range R>
			requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
		explicit FlatMap(R&& range) :buckets_(DefaultCapacity)
		{
			for (auto&& keyValue : range)
			{
				insert(std::forward<decltype(keyValue)>(keyValue));
			}
		}

		FlatMap(const FlatMap&) = default;
		FlatMap(FlatMap&&) = default;
		FlatMap& operator=(const FlatMap&) = default;
		FlatMap& operator=(FlatMap&&) = default;

		~FlatMap() = default;

		/**
		* [EN]
		* Returns an iterator to the first Occupied bucket.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初の Occupied バケットを指すイテレータを返す。
		*/
		Iterator begin()
		{
			return Iterator(&buckets_, 0);
		}

		/**
		* [EN]
		* Returns the past-the-end iterator.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 終端の次を指すイテレータを返す。
		*/
		Iterator end()
		{
			return Iterator(&buckets_, buckets_.size());
		}

		/**
		* [EN]
		* Const overload of begin().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* begin() の const オーバーロード。
		*/
		ConstIterator begin()const
		{
			return ConstIterator(&buckets_, 0);
		}

		/**
		* [EN]
		* Const overload of end().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* end() の const オーバーロード。
		*/
		ConstIterator end()const
		{
			return ConstIterator(&buckets_, buckets_.size());
		}

		/**
		* [EN]
		* Returns an explicitly const iterator to the first Occupied bucket.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初の Occupied バケットを指す、明示的な const イテレータを返す。
		*/
		ConstIterator cbegin()const
		{
			return begin();
		}

		/**
		* [EN]
		* Returns the explicitly const past-the-end iterator.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 終端の次を指す、明示的な const イテレータを返す。
		*/
		ConstIterator cend()const
		{
			return end();
		}

		/**
		* [EN]
		* Returns the number of stored entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 格納されている要素数を返す。
		*/
		size_type size()const noexcept
		{
			return size_;
		}

		/**
		* [EN]
		* Returns whether the map has no entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マップに要素が1つもないかを返す。
		*/
		Bool empty()const noexcept
		{
			return size_ == 0;
		}

		/**
		* [EN]
		* Returns the current bucket array size (always a power of two).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のバケット配列サイズを返す（常に2の冪）。
		*/
		size_type capacity()const noexcept
		{
			return buckets_.size();
		}

		/**
		* [EN]
		* Grows the bucket array (if needed) so that n entries fit without
		* exceeding MaxLoadFactor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n 個のエントリが MaxLoadFactor を超えずに収まるよう、必要なら
		* バケット配列を拡張する。
		*/
		void reserve(size_type n)
		{
			size_type required = static_cast<size_type>(static_cast<Float>(n) / MaxLoadFactor) + 1;
			size_type capacity = DefaultCapacity;

			while (capacity < required)
			{
				capacity *= 2;
			}

			if (capacity > buckets_.size())
			{
				rehash(capacity);
			}
		}

		/**
		* [EN]
		* Inserts a copy of keyValue if its key is not already present.
		* Returns an iterator to the entry (new or existing) and whether
		* insertion actually happened.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* keyValue のキーが未登録であれば、そのコピーを挿入する。
		* エントリ（新規または既存）へのイテレータと、実際に挿入が
		* 行われたかを返す。
		*/
		std::pair<Iterator, Bool> insert(const value_type& keyValue)
		{
			ensure_capacity();
			auto [index, inserted] = insert_implementation(keyValue);
			return { Iterator(&buckets_,index),inserted };
		}

		/**
		* [EN]
		* Move overload of insert.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* insert のムーブオーバーロード。
		*/
		std::pair<Iterator, Bool> insert(value_type&& keyValue)
		{
			ensure_capacity();
			auto [index, inserted] = insert_implementation(std::move(keyValue));
			return { Iterator(&buckets_,index),inserted };
		}

		/**
		* [EN]
		* Constructs a value_type from args and inserts it (see insert).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* args から value_type を構築して挿入する（insert を参照）。
		*/
		template<typename... Args>
		std::pair<Iterator, Bool> emplace(Args&&... args)
		{
			return insert(value_type(std::forward<Args>(args)...));
		}

		/**
		* [EN]
		* Inserts every element of range in range order (see insert).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* range の全要素を範囲の順に挿入する（insert を参照）。
		*/
		template<std::ranges::input_range R>
			requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
		void insert_range(R&& range)
		{
			for (auto&& keyValue : range)
			{
				insert(std::forward<decltype(keyValue)>(keyValue));
			}
		}

		/**
		* [EN]
		* Returns a reference to the value mapped to key, default-constructing
		* and inserting one (via a copy of key) if not already present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応する値への参照を返す。未登録であれば（key のコピーを
		* 用いて）デフォルト構築した値を挿入する。
		*/
		Value& operator[](const Key& key)
		{
			ensure_capacity();
			auto [index, found] = probe(key);
			if (!found)
			{
				if (buckets_[index].state_ != SlotState::Deleted)
				{
					++occupied_;
				}
				buckets_[index].state_ = SlotState::Occupied;
				buckets_[index].keyValue_ = value_type(key, Value{});
				++size_;
			}
			return buckets_[index].keyValue_->second;
		}

		/**
		* [EN]
		* Move overload of operator[].
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator[] のムーブオーバーロード。
		*/
		Value& operator[](const Key&& key)
		{
			ensure_capacity();
			auto [index, found] = probe(key);
			if (!found)
			{
				if (buckets_[index].state_ != SlotState::Deleted)
				{
					++occupied_;
				}
				buckets_[index].state_ = SlotState::Occupied;
				buckets_[index].keyValue_ = value_type(std::move(key), Value{});
				++size_;
			}
			return buckets_[index].keyValue_->second;
		}

		/**
		* [EN]
		* Returns a reference to the value mapped to key. Throws
		* std::out_of_range if not found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応する値への参照を返す。見つからなければ
		* std::out_of_range を送出する。
		*/
		Value& at(const Key& key)
		{
			auto [index, found] = probe(key);
			if (!found)
			{
				throw std::out_of_range("FlatMap::at: key not found");
			}
			return buckets_[index].keyValue_->second;
		}

		/**
		* [EN]
		* Const overload of at.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* at の const オーバーロード。
		*/
		const Value& at(const Key& key)const
		{
			auto [index, found] = probe(key);
			if (!found)
			{
				throw std::out_of_range("FlatMap::at: key not found");
			}
			return buckets_[index].keyValue_->second;
		}

		/**
		* [EN]
		* Removes the entry for key if present (tombstoning its slot).
		* Returns whether an entry was removed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応するエントリが存在すれば削除する（そのスロットを
		* トゥームストーンにする）。エントリが削除されたかを返す。
		*/
		Bool erase(const Key& key)
		{
			auto [index, found] = probe(key);
			if (!found)
			{
				return false;
			}

			buckets_[index].state_ = SlotState::Deleted;
			buckets_[index].keyValue_.reset();
			--size_;
			return true;
		}

		/**
		* [EN]
		* Removes the entry pointed to by iterator, returning an iterator
		* to the next entry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* iterator が指すエントリを削除し、次のエントリへのイテレータを返す。
		*/
		Iterator erase(Iterator iterator)
		{
			const Key& key = iterator->first;
			erase(key);
			++iterator;
			return iterator;
		}

		/**
		* [EN]
		* Removes every entry, resetting all buckets to Empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全エントリを削除し、全バケットを Empty にリセットする。
		*/
		void clear()noexcept
		{
			for (auto& slot : buckets_)
			{
				slot.state_ = SlotState::Empty;
				slot.keyValue_.reset();
			}
			size_ = 0;
			occupied_ = 0;
		}

		/**
		* [EN]
		* Returns an iterator to the entry for key, or end() if not found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応するエントリへのイテレータを返す。見つからなければ end()。
		*/
		Iterator find(const Key& key)
		{
			auto [index, found] = probe(key);
			if (!found)
			{
				return end();
			}
			return Iterator(&buckets_, index);
		}

		/**
		* [EN]
		* Const overload of find.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* find の const オーバーロード。
		*/
		ConstIterator find(const Key& key)const
		{
			auto [index, found] = probe(key);
			if (!found)
			{
				return end();
			}
			return ConstIterator(&buckets_, index);
		}

		/**
		* [EN]
		* Returns whether key is present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key が存在するかを返す。
		*/
		Bool contains(const Key& key)const noexcept
		{
			auto [index, found] = probe(key);
			return found;
		}

		/**
		* [EN]
		* Returns 1 if key is present, 0 otherwise (set-like map, so never more than 1).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key が存在すれば1、存在しなければ0を返す（集合的なマップなので
		* 2以上になることはない）。
		*/
		size_type count(const Key& key)const noexcept
		{
			return contains(key) ? 1 : 0;
		}

		/**
		* [EN]
		* Returns the current occupancy ratio (occupied slots / capacity).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の占有率（占有スロット数 / 容量）を返す。
		*/
		Float load_factor()const noexcept
		{
			return static_cast<Float>(occupied_) / static_cast<Float>(buckets_.size());
		}
	};
}