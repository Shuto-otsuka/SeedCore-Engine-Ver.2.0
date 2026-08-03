#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <FoundationEngine/Utility/Array.h>

namespace SeedCore
{
	/**
	* [EN]
	* Dynamically resizable bitset, backed by an array of 64-bit blocks.
	* Unlike std::bitset (fixed size at compile time) or std::vector<bool>
	* (no bitwise-op operators), this grows on demand and supports
	* AND/OR/XOR combined with fast set-bit iteration (find_first_set/
	* find_next_set) for ECS-style component signature masks.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 64ビットブロックの配列を裏付けとする、動的にリサイズ可能な
	* ビットセット。std::bitset（コンパイル時固定サイズ）や
	* std::vector<bool>（ビット演算子なし）と異なり、必要に応じて
	* 拡張でき、AND/OR/XORと高速な立っているビットの走査
	* （find_first_set/find_next_set）を、ECS的なコンポーネント
	* シグネチャマスク向けにサポートする。
	*/
	class SEEDCORE_API Bitset
	{
	public:
		/// [EN] Storage unit type for each block of bits.
		/// [JP] 各ビットブロックのストレージ単位となる型。
		using Block = Uint64;

		/// [EN] Number of bits held per Block.
		/// [JP] 1つの Block が保持するビット数。
		static constexpr Size BitsPerBlock = sizeof(Block) * 8;

	public:
		Bitset() = default;

		/**
		* [EN]
		* Constructs with room for at least bitCount bits, all initialized to 0.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 少なくとも bitCount ビット分の領域を持って構築する。全て0で初期化される。
		*/
		explicit Bitset(Size bitCount);

	public:
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
		void resize(Size bitCount, Bool defaultValue = false);

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
		void reserve(Size bitCount);

		/**
		* [EN]
		* Sets every bit to 0, without changing the logical size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 論理サイズを変えずに、全ビットを0に設定する。
		*/
		void clear();

		/**
		* [EN]
		* Sets every bit to 1, without changing the logical size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 論理サイズを変えずに、全ビットを1に設定する。
		*/
		void fill();

	public:
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
		void set(Size index, Bool value = true);

		/**
		* [EN]
		* Clears the bit at index (equivalent to set(index, false)).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index のビットをクリアする（set(index, false) と同等）。
		*/
		void reset(Size index);

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
		void flip(Size index);

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
		Bool test(Size index)const;

	public:
		/**
		* [EN]
		* Returns whether any bit is set.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* いずれかのビットが立っているかを返す。
		*/
		Bool any()const;

		/**
		* [EN]
		* Returns whether no bit is set (the negation of any()).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* どのビットも立っていないかを返す（any() の否定）。
		*/
		Bool none()const;

		/**
		* [EN]
		* Returns whether every bit within the logical size is set.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 論理サイズ内の全ビットが立っているかを返す。
		*/
		Bool all()const;

	public:
		/**
		* [EN]
		* Returns the number of set bits (population count).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 立っているビットの数（ポピュレーションカウント）を返す。
		*/
		Size count()const;

	public:
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
		Size find_first_set()const;

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
		Size find_next_set(Size startIndex)const;

	public:
		/**
		* [EN]
		* Returns the logical bit count (as set by the constructor/resize()).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* （コンストラクタ/resize() で設定された）論理ビット数を返す。
		*/
		Size size()const;

		/**
		* [EN]
		* Returns whether the logical bit count is 0.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 論理ビット数が0かどうかを返す。
		*/
		Bool empty()const;

	public:
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
		const Block* data()const;

		/**
		* [EN]
		* Mutable overload of data().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* data() の可変オーバーロード。
		*/
		Block* data();

		/**
		* [EN]
		* Returns the number of Blocks backing this bitset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このビットセットを裏付ける Block の個数を返す。
		*/
		Size block_count()const;

	public:
		/**
		* [EN]
		* Returns whether the bit at index is set (equivalent to test(index)).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index のビットが立っているかを返す（test(index) と同等）。
		*/
		Bool operator[](Size index)const;

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
		Bitset& operator&=(const Bitset& other);

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
		Bitset& operator|=(const Bitset& other);

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
		Bitset& operator^=(const Bitset& other);

	public:
		/**
		* [EN]
		* Returns the bitwise AND of lhs and rhs (see operator&=).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* lhs と rhs のビット単位ANDを返す（operator&= を参照）。
		*/
		friend Bitset operator&(Bitset lhs, const Bitset& rhs)
		{
			lhs &= rhs;
			return lhs;
		}

		/**
		* [EN]
		* Returns the bitwise OR of lhs and rhs (see operator|=).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* lhs と rhs のビット単位ORを返す（operator|= を参照）。
		*/
		friend Bitset operator|(Bitset lhs, const Bitset& rhs)
		{
			lhs |= rhs;
			return lhs;
		}

		/**
		* [EN]
		* Returns the bitwise XOR of lhs and rhs (see operator^=).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* lhs と rhs のビット単位XORを返す（operator^= を参照）。
		*/
		friend Bitset operator^(Bitset lhs, const Bitset& rhs)
		{
			lhs ^= rhs;
			return lhs;
		}

		/**
		* [EN]
		* Returns whether lhs and rhs hold the same bit pattern, comparing
		* block-by-block up to the larger of the two block counts (missing
		* blocks on the shorter side are treated as 0).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* lhs と rhs が同じビットパターンを持つかを返す。両者のブロック数の
		* うち大きい方まで、ブロック単位で比較する（短い方に存在しない
		* ブロックは0として扱う）。
		*/
		friend Bool operator==(const Bitset& lhs, const Bitset& rhs)
		{
			Size maxBlocks = (lhs.data_.size() > rhs.data_.size()) ? lhs.data_.size() : rhs.data_.size();
			for (Size index = 0; index < maxBlocks; ++index)
			{
				Block a = (index < lhs.data_.size()) ? lhs.data_[index] : 0;
				Block b = (index < rhs.data_.size()) ? rhs.data_[index] : 0;
				if (a != b)
				{
					return false;
				}
			}
			return true;
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
		friend Bool operator!=(const Bitset& lhs, const Bitset& rhs)
		{
			return !(lhs == rhs);
		}

	public:
		/// [EN] Sentinel returned by find_first_set/find_next_set when no bit is set.
		/// [JP] 立っているビットがない場合に find_first_set/find_next_set が返す番兵値。
		static constexpr Size InvalidIndex = static_cast<Size>(-1);

	private:
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
		void ensure_size(Size bitIndex);

		/**
		* [EN]
		* Returns the number of Blocks needed to hold bitCount bits.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* bitCount ビットを保持するのに必要な Block 数を返す。
		*/
		static Size get_required_block_count(Size bitCount);

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
		void clear_unused_bits();

	private:
		/// [EN] Backing block storage.
		/// [JP] 裏付けとなるブロックストレージ。
		DynamicArray<Block> data_;

		/// [EN] Logical bit count, as last set by the constructor/resize().
		/// [JP] コンストラクタ/resize() で最後に設定された論理ビット数。
		Size bitCount_ = 0;
	};
}