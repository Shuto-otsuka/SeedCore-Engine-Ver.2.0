#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Delegate.h>
#include <atomic>

namespace SeedCore
{
	/**
	* [EN]
	* Returns a freshly generated, globally unique handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 新たに発行された、グローバルに一意なハンドルを返す。
	*/
	DelegateHandle DelegateHandle::Generate()
	{
		/// [EN] Process-wide counter shared by every DelegateHandle::Generate() call,
		///      starting at 1 so that 0 stays reserved for "invalid".
		/// [JP] DelegateHandle::Generate() の全呼び出しで共有される、プロセス全体の
		///      カウンタ。0を「無効」用に予約するため1から開始する。
		static std::atomic<Uint64> nextId = 1;

		DelegateHandle handle;
		handle.id_ = nextId.fetch_add(1, std::memory_order_relaxed);
		return handle;
	}

	/**
	* [EN]
	* Returns whether this handle was produced by Generate() (as
	* opposed to being default-constructed).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このハンドルが（デフォルト構築ではなく）Generate() によって
	* 発行されたものかどうかを返す。
	*/
	Bool DelegateHandle::IsValid()const
	{
		return id_ != 0;
	}
}
