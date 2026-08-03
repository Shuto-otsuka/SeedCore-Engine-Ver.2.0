#include <FoundationEngine/JobSystem/JobVector.h>
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs pointing at firstElement (typically the derived class's
	* inline storage) with size bytes of initial capacity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* firstElement（通常は派生クラスのインラインストレージ）を指し、
	* size バイト分の初期容量で構築する。
	*/
	JobVectorBase::JobVectorBase(void* firstElement, Size size) :begin_(firstElement), end_(firstElement), capacity_((Char*)firstElement + size)
	{
		/// No Code
	}

	/**
	* [EN]
	* POD-path buffer growth: reallocates (or, if still on inline
	* storage identified by firstElement, mallocs+copies) to at least
	* minSizeInBytes, growing geometrically by roughly 2x + size otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* POD経路でのバッファ成長: 少なくとも minSizeInBytes まで再確保する
	* （まだインラインストレージ上であれば、firstElement で判定し
	* malloc+コピーする）。それ以外の場合は概ね2倍+size で幾何的に
	* 成長する。
	*/
	void JobVectorBase::grow_pod(void* firstElement, Size minSizeInBytes, Size size)
	{
		Size currentSizeBytes = size_in_byte();
		Size newCapacityInBytes = 2 * capacity_in_byte() + size;
		if (newCapacityInBytes < minSizeInBytes)
		{
			newCapacityInBytes = minSizeInBytes;
		}

		void* newElement;
		if (begin_ == firstElement)
		{
			/// [EN] Still on inline storage: cannot realloc it, so malloc fresh storage and copy the live bytes over.
			/// [JP] まだインラインストレージ上のため realloc できない。新しいストレージを malloc し、有効なバイトをコピーする。
			newElement = malloc(newCapacityInBytes);
			memcpy(newElement, this->begin_, currentSizeBytes);
		}
		else
		{
			/// [EN] Already heap-allocated: realloc in place.
			/// [JP] 既にヒープ確保済み: その場で realloc する。
			newElement = realloc(this->begin_, newCapacityInBytes);
		}

		this->end_ = (Char*)newElement + currentSizeBytes;
		this->begin_ = newElement;
		this->capacity_ = (Char*)this->begin_ + newCapacityInBytes;
	}

	/**
	* [EN]
	* Returns the number of live bytes currently stored (end_ - begin_).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在格納されている有効バイト数を返す（end_ - begin_）。
	*/
	Size JobVectorBase::size_in_byte()const
	{
		return Size((Char*)end_ - (Char*)begin_);
	}

	/**
	* [EN]
	* Returns the total allocated capacity in bytes (capacity_ - begin_).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 確保済みの総容量をバイト単位で返す（capacity_ - begin_）。
	*/
	Size JobVectorBase::capacity_in_byte()const
	{
		return Size((Char*)capacity_ - (Char*)begin_);
	}

	/**
	* [EN]
	* Returns whether there are currently no live elements.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在有効な要素が無いかどうかを返す。
	*/
	Bool JobVectorBase::empty()const
	{
		return begin_ == end_;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Rounds array up to the next power of two (used to size new backing storage).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* array を次の2の冪へ切り上げる（新しい裏付けストレージのサイズ
	* 決定に使う）。
	*/
	Uint64 ArrayNextCapacity(Uint64 array)
	{
		/// [EN] Bit-OR cascade sets every bit below the highest set bit,
		///      producing (2^n - 1); adding 1 rounds up to the next power of two.
		/// [JP] ビットOR連鎖により、最上位ビットより下の全ビットを1にする
		///      （2^n - 1 になる）。1を加算することで次の2の冪へ切り上げる。
		array |= (array >> 1);
		array |= (array >> 2);
		array |= (array >> 4);
		array |= (array >> 8);
		array |= (array >> 16);
		array |= (array >> 32);
		return array + 1;
	}
}
