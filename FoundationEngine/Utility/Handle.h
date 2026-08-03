#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Generic versioned handle to a T stored in a slot-based container
	* (e.g. StablePool). Combines a slot index_ with a generation_
	* counter so that a handle to a destroyed-and-reused slot can be
	* detected as stale rather than aliasing a different object.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スロットベースのコンテナ（StablePool など）に格納された T への
	* 汎用バージョン付きハンドル。スロット index_ と generation_
	* カウンタを組み合わせることで、破棄・再利用されたスロットへの
	* ハンドルを、別オブジェクトへのエイリアスではなく古いものとして
	* 検出できる。
	*/
	template <typename T>
	struct Handle
	{
		/// [EN] Slot index into the owning container. max() means "null".
		/// [JP] 所有コンテナ内のスロットインデックス。max() は「null」を意味する。
		Uint64 index_ = std::numeric_limits<Uint64>::max();

		/// [EN] Generation of the slot at the time this handle was issued.
		/// [JP] このハンドルが発行された時点でのスロットの世代。
		Uint64 generation_ = 0;

		/**
		* [EN]
		* Returns a null (non-existent) handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* null（存在しない）ハンドルを返す。
		*/
		static constexpr Handle null()noexcept
		{
			return {};
		}

		/**
		* [EN]
		* Returns whether this handle refers to a slot (i.e. is not null).
		* Does not verify the generation still matches; that check happens
		* on the owning container's Get/Destroy.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このハンドルが（nullではなく）あるスロットを指しているかを返す。
		* 世代が現在も一致しているかは検証しない。その検証は所有コンテナの
		* Get/Destroy 側で行われる。
		*/
		constexpr Bool exists()const noexcept
		{
			return index_ != std::numeric_limits<Uint64>::max();
		}

		/**
		* [EN]
		* Returns whether this handle is null (the negation of exists()).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このハンドルが null かどうかを返す（exists() の否定）。
		*/
		constexpr Bool empty()const noexcept
		{
			return !exists();
		}

		/**
		* [EN]
		* Handles compare equal iff both index_ and generation_ match.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* index_ と generation_ の両方が一致する場合にのみ等しいとみなす。
		*/
		friend Bool operator==(const Handle&, const Handle&) = default;
	};
}