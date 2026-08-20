#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <FoundationEngine/Utility/Array.h>
#include <functional>

namespace SeedCore
{
	/**
	* [EN]
	* Opaque token identifying one listener bound into a MulticastDelegate,
	* returned by MulticastDelegate::Bind() and later passed to
	* MulticastDelegate::Unbind() to remove that specific listener.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* MulticastDelegate にバインドされた1つのリスナーを識別する不透明な
	* トークン。MulticastDelegate::Bind() が返し、後で
	* MulticastDelegate::Unbind() に渡すことでそのリスナーだけを削除できる。
	*/
	struct SEEDCORE_API DelegateHandle
	{
	public:
		DelegateHandle() = default;

	public:
		/**
		* [EN]
		* Returns a freshly generated, globally unique handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新たに発行された、グローバルに一意なハンドルを返す。
		*/
		static DelegateHandle Generate();

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
		Bool IsValid()const;

	public:
		/// [EN] Equality compares the underlying id_ only.
		/// [JP] 内部の id_ のみを比較する。
		friend Bool operator==(const DelegateHandle&, const DelegateHandle&) = default;

	private:
		/// [EN] Zero means "invalid / default-constructed"; Generate() never returns 0.
		/// [JP] 0は「無効/デフォルト構築」を意味する。Generate() が0を返すことはない。
		Uint64 id_ = 0;
	};

	/**
	* [EN]
	* Binds exactly one callable of signature Ret(Args...). Rebinding
	* (Bind()) replaces any previously bound callable; there is no handle
	* to manage since only one slot exists.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シグネチャ Ret(Args...) を持つ呼び出し可能オブジェクトを1つだけ
	* バインドする。Bind() の再呼び出しは以前バインドされていたものを
	* 置き換える。スロットが1つしかないためハンドルによる管理は不要。
	*/
	template <typename Signature>
	class SinglecastDelegate;

	template <typename Ret, typename... Args>
	class SinglecastDelegate<Ret(Args...)>
	{
	public:
		SinglecastDelegate() = default;

	public:
		/**
		* [EN]
		* Binds callable, replacing whatever was previously bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable をバインドする。以前バインドされていたものは置き換えられる。
		*/
		template <typename Callable>
		void Bind(Callable&& callable)
		{
			function_ = std::forward<Callable>(callable);
		}

		/**
		* [EN]
		* Clears the bound callable, if any.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バインドされている呼び出し可能オブジェクトがあれば解除する。
		*/
		void Unbind()
		{
			function_ = nullptr;
		}

		/**
		* [EN]
		* Returns whether a callable is currently bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在呼び出し可能オブジェクトがバインドされているかを返す。
		*/
		Bool IsBound()const
		{
			return static_cast<Bool>(function_);
		}

		/**
		* [EN]
		* Invokes the bound callable with args. The callable must be bound;
		* use ExecuteIfBound() when that is not guaranteed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バインドされている呼び出し可能オブジェクトを args で呼び出す。
		* バインド済みであることが前提。保証されない場合は
		* ExecuteIfBound() を使うこと。
		*/
		Ret Execute(Args... args)const
		{
			return function_(args...);
		}

		/**
		* [EN]
		* Invokes the bound callable with args if one is bound. Returns a
		* default-constructed Ret (and does nothing) otherwise.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バインドされている場合のみ、呼び出し可能オブジェクトを args で
		* 呼び出す。バインドされていない場合は何もせず、デフォルト構築
		* された Ret を返す。
		*/
		Ret ExecuteIfBound(Args... args)const
		{
			if (function_)
			{
				return function_(args...);
			}
			return Ret();
		}

	private:
		/// [EN] Currently bound callable, or empty if none is bound.
		/// [JP] 現在バインドされている呼び出し可能オブジェクト。未バインドなら空。
		std::function<Ret(Args...)> function_;
	};

	/**
	* [EN]
	* Broadcasts to any number of listeners of signature void(Args...).
	* Each Bind() call adds one listener and returns a DelegateHandle that
	* Unbind() later accepts to remove just that listener. Broadcast()
	* invokes every currently-bound listener in bind order.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シグネチャ void(Args...) の任意個のリスナーへブロードキャストする。
	* Bind() を呼ぶたびに1つのリスナーが追加され、後で Unbind() に渡す
	* ことでそのリスナーだけを削除できる DelegateHandle を返す。
	* Broadcast() はバインドされている全リスナーをバインド順に呼び出す。
	*/
	template <typename... Args>
	class MulticastDelegate
	{
	public:
		MulticastDelegate() = default;

	public:
		/**
		* [EN]
		* Adds callable as a new listener and returns a handle that can
		* later be passed to Unbind() to remove it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable を新しいリスナーとして追加し、後で Unbind() に渡して
		* 削除するためのハンドルを返す。
		*/
		template <typename Callable>
		DelegateHandle Bind(Callable&& callable)
		{
			DelegateHandle handle = DelegateHandle::Generate();
			listeners_.push_back(Listener{ handle, std::function<void(Args...)>(std::forward<Callable>(callable)) });
			return handle;
		}

		/**
		* [EN]
		* Removes the listener identified by handle, if it is still bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* handle が示すリスナーがまだバインドされていれば削除する。
		*/
		void Unbind(const DelegateHandle& handle)
		{
			for (Size index = 0; index < listeners_.size(); ++index)
			{
				if (listeners_[index].handle_ == handle)
				{
					listeners_.erase(listeners_.begin() + index);
					return;
				}
			}
		}

		/**
		* [EN]
		* Removes every currently-bound listener.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在バインドされている全リスナーを削除する。
		*/
		void Clear()
		{
			listeners_.clear();
		}

		/**
		* [EN]
		* Returns whether at least one listener is currently bound.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在少なくとも1つのリスナーがバインドされているかを返す。
		*/
		Bool IsBound()const
		{
			return !listeners_.empty();
		}

		/**
		* [EN]
		* Invokes every currently-bound listener with args, in bind order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バインドされている全リスナーを、バインド順に args で呼び出す。
		*/
		void Broadcast(Args... args)const
		{
			for (const Listener& listener : listeners_)
			{
				listener.function_(args...);
			}
		}

	private:
		/**
		* [EN]
		* One bound listener: the handle identifying it, paired with the
		* callable to invoke on Broadcast().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バインドされた1つのリスナー。それを識別するハンドルと、
		* Broadcast() 時に呼び出す呼び出し可能オブジェクトの組。
		*/
		struct Listener
		{
			/// [EN] Handle identifying this listener, as returned by Bind().
			/// [JP] Bind() が返す、このリスナーを識別するハンドル。
			DelegateHandle handle_;

			/// [EN] Callable invoked on Broadcast().
			/// [JP] Broadcast() 時に呼び出される呼び出し可能オブジェクト。
			std::function<void(Args...)> function_;
		};

	private:
		/// [EN] Currently bound listeners, in bind order.
		/// [JP] 現在バインドされているリスナー。バインド順に格納される。
		DynamicArray<Listener> listeners_;
	};
}
