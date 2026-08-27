#include <FoundationEngine/Utility/Bitset.h>
#include <FoundationEngine/Math/Algorithm.h>

#include <algorithm>
#include <bit>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs with room for at least bitCount bits, all initialized to 0.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 少なくとも bitCount ビット分の領域を持って構築する。全て0で初期化される。
	*/
	Bitset::Bitset(Size bitCount)
	{
		resize(bitCount);
	}

	/**
	* [EN]
	* Resizes to bitCount bits. Newly added bits are set to
	* defaultValue; shrinking discards the trailing bits.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* bitCount ビットにリサイズする。新たに追加されたビットは
	* defaultValue に設定される。縮小する場合は末尾のビットが破棄される。
	*/
	void Bitset::resize(Size bitCount, Bool defaultValue)
	{
		const Size oldBlockCount = data_.size();

		bitCount_ = bitCount;

		const Size newBlockCount = get_required_block_count(bitCount_);

		data_.resize(newBlockCount);

		/// [EN] Only the newly-added blocks need filling; existing blocks already hold their previous bits.
		/// [JP] 新しく追加されたブロックのみを埋めればよい。既存のブロックは以前のビットを保持したままである。
		if (newBlockCount > oldBlockCount)
		{
			const Block fillValue = defaultValue ? ~0ULL : 0ULL;

			std::fill(data_.begin() + oldBlockCount, data_.end(), fillValue);
		}

		/// [EN] When filling with 1s, the last block may have been over-filled past bitCount_; trim it back to the logical size.
		/// [JP] 1で埋める場合、最終ブロックが bitCount_ を超えて埋められている可能性がある。論理サイズに合わせて切り詰める。
		if (defaultValue)
		{
			clear_unused_bits();
		}
	}

	/**
	* [EN]
	* Reserves backing storage for at least bitCount bits without
	* changing the current logical size.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の論理サイズを変えずに、少なくとも bitCount ビット分の
	* 裏付けストレージを予約する。
	*/
	void Bitset::reserve(Size bitCount)
	{
		data_.reserve(get_required_block_count(bitCount));
	}

	/**
	* [EN]
	* Sets every bit to 0, without changing the logical size.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 論理サイズを変えずに、全ビットを0に設定する。
	*/
	void Bitset::clear()
	{
		std::ranges::fill(data_, 0ULL);
	}

	/**
	* [EN]
	* Sets every bit to 1, without changing the logical size.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 論理サイズを変えずに、全ビットを1に設定する。
	*/
	void Bitset::fill()
	{
		std::ranges::fill(data_, ~0ULL);

		clear_unused_bits();
	}

	/**
	* [EN]
	* Sets the bit at index to value, growing the bitset if index is
	* out of the current range.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のビットを value に設定する。index が現在の範囲外なら
	* ビットセットを拡張する。
	*/
	void Bitset::set(Size index, Bool value)
	{
		ensure_size(index);

		const Size blockIndex = index / BitsPerBlock;
		const Size bitIndex = index % BitsPerBlock;

		if (value)
		{
			data_[blockIndex] |= (1ULL << bitIndex);
		}
		else
		{
			data_[blockIndex] &= ~(1ULL << bitIndex);
		}
	}

	/**
	* [EN]
	* Clears the bit at index (equivalent to set(index, false)).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のビットをクリアする（set(index, false) と同等）。
	*/
	void Bitset::reset(Size index)
	{
		set(index, false);
	}

	/**
	* [EN]
	* Toggles the bit at index, growing the bitset if index is out of
	* the current range.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のビットを反転する。index が現在の範囲外ならビットセットを
	* 拡張する。
	*/
	void Bitset::flip(Size index)
	{
		ensure_size(index);

		const Size blockIndex = index / BitsPerBlock;
		const Size bitIndex = index % BitsPerBlock;

		data_[blockIndex] ^= (1ULL << bitIndex);
	}

	/**
	* [EN]
	* Returns whether the bit at index is set. Returns false (without
	* growing) if index is out of range.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のビットが立っているかを返す。index が範囲外の場合は
	* （拡張せずに）false を返す。
	*/
	Bool Bitset::test(Size index)const
	{
		if (index >= bitCount_) [[unlikely]]
		{
			return false;
		}

		const Size blockIndex = index / BitsPerBlock;
		const Size bitIndex = index % BitsPerBlock;

		return (data_[blockIndex] & (1ULL << bitIndex)) != 0;
	}

	/**
	* [EN]
	* Returns whether any bit is set.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* いずれかのビットが立っているかを返す。
	*/
	Bool Bitset::any()const
	{
		for (Block block : data_)
		{
			if (block != 0)
			{
				return true;
			}
		}

		return false;
	}

	/**
	* [EN]
	* Returns whether no bit is set (the negation of any()).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* どのビットも立っていないかを返す（any() の否定）。
	*/
	Bool Bitset::none()const
	{
		return !any();
	}

	/**
	* [EN]
	* Returns whether every bit within the logical size is set.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 論理サイズ内の全ビットが立っているかを返す。
	*/
	Bool Bitset::all()const
	{
		if (bitCount_ == 0)
		{
			return true;
		}

		/// [EN] Check every fully-occupied block first (a fast, all-bits comparison).
		/// [JP] まず完全に占有されている全ブロックをチェックする（全ビット比較による高速な処理）。
		const Size fullBlocks = bitCount_ / BitsPerBlock;

		for (Size index = 0; index < fullBlocks; ++index)
		{
			if (data_[index] != ~0ULL)
			{
				return false;
			}
		}

		/// [EN] The final, partially-occupied block (if any) must be masked before comparing, since bits beyond bitCount_ are don't-cares.
		/// [JP] 最終の部分的に占有されたブロック（あれば）は、比較前にマスクする必要がある。bitCount_ を超えるビットは don't-care であるため。
		const Size remainBits = bitCount_ % BitsPerBlock;

		if (remainBits == 0)
		{
			return true;
		}

		const Uint64 mask = (1ULL << remainBits) - 1ULL;

		return (data_[fullBlocks] & mask) == mask;
	}

	/**
	* [EN]
	* Returns the number of set bits (population count).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 立っているビットの数（ポピュレーションカウント）を返す。
	*/
	Size Bitset::count()const
	{
		Size count = 0;

		for (Block block : data_)
		{
			count += std::popcount(block);
		}

		return count;
	}

	/**
	* [EN]
	* Returns the index of the first set bit, or InvalidIndex if none are set.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 最初に立っているビットのインデックスを返す。1つも立っていなければ
	* InvalidIndex を返す。
	*/
	Size Bitset::find_first_set()const
	{
		for (Size index = 0; index < data_.size(); ++index)
		{
			Block block = data_[index];

			if (block != 0) [[unlikely]]
			{
				return index * BitsPerBlock + std::countr_zero(block);
			}
		}

		return InvalidIndex;
	}

	/**
	* [EN]
	* Returns the index of the first set bit at or after startIndex,
	* or InvalidIndex if none are set.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* startIndex 以降で最初に立っているビットのインデックスを返す。
	* 1つも立っていなければ InvalidIndex を返す。
	*/
	Size Bitset::find_next_set(Size startIndex)const
	{
		if (startIndex >= bitCount_) [[unlikely]]
		{
			return InvalidIndex;
		}

		Size blockIndex = startIndex / BitsPerBlock;
		Size bitIndex = startIndex % BitsPerBlock;

		Block block = data_[blockIndex];

		/// [EN] Mask off every bit before startIndex within its own block, so the scan below can't return a match earlier than requested.
		/// [JP] 自身のブロック内で startIndex より前の全ビットをマスクする。これにより、以下の走査が要求より前の一致を返すことがなくなる。
		block &= (~0ULL << bitIndex);

		while (true)
		{
#if defined(__cpp_assume)
			[[assume(true)]];
#else
			__assume(true);
#endif

			if (block != 0) [[unlikely]]
			{
				return blockIndex * BitsPerBlock + std::countr_zero(block);
			}

			if (++blockIndex >= data_.size()) [[unlikely]]
			{
				break;
			}

			block = data_[blockIndex];
		}

		return InvalidIndex;
	}

	/**
	* [EN]
	* Returns the logical bit count (as set by the constructor/resize()).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* （コンストラクタ/resize() で設定された）論理ビット数を返す。
	*/
	Size Bitset::size()const
	{
		return bitCount_;
	}

	/**
	* [EN]
	* Returns whether the logical bit count is 0.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 論理ビット数が0かどうかを返す。
	*/
	Bool Bitset::empty()const
	{
		return bitCount_ == 0;
	}

	/**
	* [EN]
	* Returns a const pointer to the backing block array, for bulk/GPU access.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 一括アクセスやGPUアクセス向けに、裏付けブロック配列への
	* const ポインタを返す。
	*/
	const Bitset::Block* Bitset::data()const
	{
		return data_.data();
	}

	/**
	* [EN]
	* Mutable overload of data().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* data() の可変オーバーロード。
	*/
	Bitset::Block* Bitset::data()
	{
		return data_.data();
	}

	/**
	* [EN]
	* Returns the number of Blocks backing this bitset.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このビットセットを裏付ける Block の個数を返す。
	*/
	Size Bitset::block_count()const
	{
		return data_.size();
	}

	/**
	* [EN]
	* Returns whether the bit at index is set (equivalent to test(index)).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* index のビットが立っているかを返す（test(index) と同等）。
	*/
	Bool Bitset::operator[](Size index)const
	{
		return test(index);
	}

	/**
	* [EN]
	* In-place bitwise AND with other. Blocks beyond other's size are
	* cleared to 0.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのビット単位AND をその場で行う。other のサイズを
	* 超えるブロックは0にクリアされる。
	*/
	Bitset& Bitset::operator&=(const Bitset& other)
	{
		/// [EN] AND only where both bitsets actually have data.
		/// [JP] 両方のビットセットが実際にデータを持つ範囲でのみ AND を行う。
		const Size minBlocks = Min(data_.size(), other.data_.size());

		for (Size index = 0; index < minBlocks; ++index)
		{
			data_[index] &= other.data_[index];
		}

		/// [EN] Any block beyond other's size is implicitly ANDed with 0 (other has no bits there), so clear it directly.
		/// [JP] other のサイズを超えるブロックは、暗黙的に 0 との AND になる（other にはそこにビットが無い）ため、直接クリアする。
		for (Size index = minBlocks; index < data_.size(); ++index)
		{
			data_[index] = 0;
		}

		return *this;
	}

	/**
	* [EN]
	* In-place bitwise OR with other, growing this bitset first if
	* other is logically larger.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのビット単位OR をその場で行う。other の方が論理サイズが
	* 大きければ、先にこのビットセットを拡張する。
	*/
	Bitset& Bitset::operator|=(const Bitset& other)
	{
		if (other.bitCount_ > bitCount_)
		{
			resize(other.bitCount_);
		}

		for (Size index = 0; index < other.data_.size(); ++index)
		{
			data_[index] |= other.data_[index];
		}

		return *this;
	}

	/**
	* [EN]
	* In-place bitwise XOR with other, growing this bitset first if
	* other is logically larger.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* other とのビット単位XOR をその場で行う。other の方が論理サイズが
	* 大きければ、先にこのビットセットを拡張する。
	*/
	Bitset& Bitset::operator^=(const Bitset& other)
	{
		if (other.bitCount_ > bitCount_)
		{
			resize(other.bitCount_);
		}

		for (Size index = 0; index < other.data_.size(); ++index)
		{
			data_[index] ^= other.data_[index];
		}

		return *this;
	}

	/**
	* [EN]
	* Grows the bitset (via resize) if bitIndex is not yet addressable.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* bitIndex がまだアドレス可能でなければ、（resize経由で）
	* ビットセットを拡張する。
	*/
	void Bitset::ensure_size(Size bitIndex)
	{
		if (bitIndex < bitCount_)
		{
			return;
		}

		resize(bitIndex + 1);
	}

	/**
	* [EN]
	* Returns the number of Blocks needed to hold bitCount bits.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* bitCount ビットを保持するのに必要な Block 数を返す。
	*/
	Size Bitset::get_required_block_count(Size bitCount)
	{
		return (bitCount + BitsPerBlock - 1) / BitsPerBlock;
	}

	/**
	* [EN]
	* Zeroes any bits in the last block beyond bitCount_, so bitwise
	* ops/all()/count() don't see stale garbage bits.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 最終ブロック中の bitCount_ を超える部分のビットをゼロにする。
	* これにより、ビット演算/all()/count() が古いゴミビットを見ないようにする。
	*/
	void Bitset::clear_unused_bits()
	{
		const Size remainBits = bitCount_ % BitsPerBlock;

		if (remainBits == 0 || data_.empty())
		{
			return;
		}

		const Uint64 mask = (1ULL << remainBits) - 1ULL;

		data_.back() &= mask;
	}
}