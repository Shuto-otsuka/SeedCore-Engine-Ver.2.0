#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* Mixin that deletes move construction/assignment while keeping copy
	* construction/assignment. Inherit from this (protected constructor/
	* destructor) to make a derived type copy-only (pinned address on move).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ムーブ構築/代入を削除し、コピー構築/代入は残す mixin。
	* これを継承する（コンストラクタ/デストラクタは protected）ことで、
	* 派生型をコピーオンリーにできる（ムーブ不可でアドレスが固定される）。
	*/
	class NonMovable
	{
	protected:
		constexpr NonMovable() = default;
		~NonMovable() = default;

	public:
		NonMovable(const NonMovable&) = default;
		NonMovable& operator=(const NonMovable&) = default;
		NonMovable(NonMovable&&) = delete;
		NonMovable& operator=(NonMovable&&) = delete;
	};
}