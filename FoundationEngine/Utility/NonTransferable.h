#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* Mixin that deletes both copy and move construction/assignment.
	* Inherit from this (protected constructor/destructor) to make a
	* derived type immovable and non-copyable (fixed identity/address).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コピーとムーブの両方の構築/代入を削除する mixin。
	* これを継承する（コンストラクタ/デストラクタは protected）ことで、
	* 派生型をコピー・ムーブ不可（アイデンティティ/アドレス固定）にできる。
	*/
	class NonTransferable
	{
	protected:
		constexpr NonTransferable() = default;
		~NonTransferable() = default;

	public:
		NonTransferable(const NonTransferable&) = delete;
		NonTransferable& operator=(const NonTransferable&) = delete;
		NonTransferable(NonTransferable&&) = delete;
		NonTransferable& operator=(NonTransferable&&) = delete;
	};
}