#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a pointer+tag pair from raw values.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ポインタとタグの生の値からペアを構築する。
	*/
	SynchronizedPointer::SynchronizedPointer(PointerType ptr, TagType tag)noexcept :ptr_(ptr), tag_(tag)
	{
		/// No Code
	}
}