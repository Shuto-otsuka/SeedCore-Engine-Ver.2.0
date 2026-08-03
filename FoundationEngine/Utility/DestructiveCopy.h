#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* Wraps a T so that copying it actually moves the underlying value
	* out of the source (the "copy" constructor takes a const& but moves
	* via the mutable member, so it compiles where only copy is allowed
	* but behaves like a move). Useful for move-only payloads that must be
	* stored in copy-only containers/callables.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T をラップし、コピーすると実際には内部の値が移動元から
	* ムーブされるようにする（「コピー」コンストラクタは const& を
	* 取るが、mutable メンバ経由でムーブするため、コピーしか
	* 許されない場面でもコンパイルでき、挙動はムーブになる）。
	* コピーオンリーなコンテナ/呼び出し可能オブジェクトに格納する必要が
	* あるムーブオンリーなペイロードに使う。
	*/
	template<typename T>
	struct DestructiveCopy
	{
		/**
		* [EN]
		* Constructs by moving from an rvalue T.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 右辺値の T からムーブして構築する。
		*/
		DestructiveCopy(T&& rhs) :object_(std::move(rhs))
		{
			/// No Code
		}

		/**
		* [EN]
		* "Copy" constructor that actually moves other.object_ out,
		* leaving other in a moved-from state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 「コピー」コンストラクタだが、実際には other.object_ を
		* ムーブして取り出す。other はムーブ済み状態になる。
		*/
		DestructiveCopy(const T& other) :object_(std::move(other.object_))
		{
			/// No Code
		}

		/**
		* [EN]
		* Returns a reference to the wrapped object.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ラップされたオブジェクトへの参照を返す。
		*/
		T& Get()
		{
			return object_;
		}

		/// [EN] The wrapped object; mutable so it can be moved-from even
		///      through a const& in the "copy" constructor.
		/// [JP] ラップされたオブジェクト。「コピー」コンストラクタの
		///      const& 経由でもムーブアウトできるよう mutable。
		mutable T object_;
	};
}