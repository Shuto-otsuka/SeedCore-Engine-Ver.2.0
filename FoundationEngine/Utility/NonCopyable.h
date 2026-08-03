#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* CRTP-free mixin that deletes copy construction/assignment while
	* keeping move construction/assignment. Inherit from this (protected
	* constructor/destructor) to make a derived type move-only.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コピー構築/代入を削除し、ムーブ構築/代入は残す mixin（CRTP不要）。
	* これを継承する（コンストラクタ/デストラクタは protected）ことで、
	* 派生型をムーブオンリーにできる。
	*/
	class NonCopyable
	{
	protected:
		constexpr NonCopyable() = default;
		~NonCopyable() = default;

	public:
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
		NonCopyable(NonCopyable&&) = default;
		NonCopyable& operator=(NonCopyable&&) = default;
	};
}