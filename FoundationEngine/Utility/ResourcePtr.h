#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <FoundationEngine/Log/Assert.h>
#include <type_traits>
#include <utility>

namespace SeedCore
{
	/**
	* [EN]
	* Sole-ownership smart pointer, custom-built as a replacement for
	* std::unique_ptr. Move-only: copying is disabled so ownership of the
	* pointee can never be ambiguous. Supports moving/converting from a
	* ResourcePtr<U> to a ResourcePtr<T> when U* is implicitly convertible
	* to T* (e.g. Derived -> Base), so factories can return a derived
	* instance through a base-typed ResourcePtr.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* std::unique_ptr の代替として自作した単独所有スマートポインタ。
	* ムーブ専用（コピーは禁止）で、所有権の所在が曖昧にならないように
	* している。U* が T* へ暗黙変換可能（例: 派生 -> 基底）な場合、
	* ResourcePtr<U> から ResourcePtr<T> へのムーブ/変換に対応しており、
	* ファクトリが派生インスタンスを基底型の ResourcePtr で返せる。
	*/
	template <typename T>
	class ResourcePtr
	{
	public:
		/**
		* [EN]
		* Default-constructs a null ResourcePtr (owns nothing).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 何も所有していない null な ResourcePtr を構築する。
		*/
		constexpr ResourcePtr()noexcept = default;

		/**
		* [EN]
		* Constructs a null ResourcePtr from nullptr, for `= nullptr`-style
		* initialization at call sites.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nullptr から null な ResourcePtr を構築する。呼び出し側での
		* `= nullptr` 形式の初期化のために用意している。
		*/
		constexpr ResourcePtr(std::nullptr_t)noexcept
		{
			/// No Code
		}

		/**
		* [EN]
		* Takes ownership of an already-allocated raw pointer. Explicit,
		* since implicitly wrapping a raw pointer is exactly the footgun
		* (double ownership via two separate wrappers) this type avoids;
		* prefer MakePtr() at call sites instead.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既に確保済みの生ポインタの所有権を引き取る。生ポインタを暗黙に
		* ラップできてしまうと（別々のラッパーが同じポインタを二重所有する）
		* この型がまさに避けたい地雷になるため explicit にしている。
		* 呼び出し側では基本的に MakePtr() を使うこと。
		*/
		explicit ResourcePtr(T* pointer)noexcept: pointer_(pointer)
		{
			/// No Code
		}

		/**
		* [EN]
		* Copy construction is disabled — ownership must never be shared.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー構築は禁止。所有権が複数箇所で共有されることはない。
		*/
		ResourcePtr(const ResourcePtr&) = delete;

		/**
		* [EN]
		* Copy assignment is disabled — ownership must never be shared.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー代入は禁止。所有権が複数箇所で共有されることはない。
		*/
		ResourcePtr& operator=(const ResourcePtr&) = delete;

		/**
		* [EN]
		* Transfers ownership from other, leaving other null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* other から所有権を引き継ぎ、other は null になる。
		*/
		ResourcePtr(ResourcePtr&& other)noexcept: pointer_(other.pointer_)
		{
			other.pointer_ = nullptr;
		}

		/**
		* [EN]
		* Converting move constructor: transfers ownership from a
		* ResourcePtr<U> when U* implicitly converts to T* (e.g. taking
		* ownership of a Derived instance through a Base-typed ResourcePtr).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換ムーブコンストラクタ。U* が T* へ暗黙変換可能な場合に
		* ResourcePtr<U> から所有権を引き継ぐ（例: 派生インスタンスの
		* 所有権を基底型の ResourcePtr として受け取る）。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourcePtr(ResourcePtr<U>&& other)noexcept: pointer_(other.pointer_)
		{
			other.pointer_ = nullptr;
		}

		/**
		* [EN]
		* Releases the currently-owned pointee (if any) and transfers
		* ownership from other, leaving other null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在所有しているオブジェクトを（あれば）解放し、other から
		* 所有権を引き継ぐ。other は null になる。
		*/
		ResourcePtr& operator=(ResourcePtr&& other)noexcept
		{
			if (this != &other)
			{
				reset();
				pointer_ = other.pointer_;
				other.pointer_ = nullptr;
			}
			return *this;
		}

		/**
		* [EN]
		* Converting move assignment: same as the converting move
		* constructor, but for an already-constructed ResourcePtr.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換ムーブ代入。変換ムーブコンストラクタと同様だが、既に構築済みの
		* ResourcePtr に対して行う。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourcePtr& operator=(ResourcePtr<U>&& other)noexcept
		{
			reset();
			pointer_ = other.pointer_;
			other.pointer_ = nullptr;
			return *this;
		}

		/**
		* [EN]
		* Resets to null, releasing the currently-owned pointee (if any).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* null に戻し、現在所有しているオブジェクトを（あれば）解放する。
		*/
		ResourcePtr& operator=(std::nullptr_t)noexcept
		{
			reset();
			return *this;
		}

		/**
		* [EN]
		* Destroys the owned pointee (if any) via reset().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* reset() を通じて、所有しているオブジェクトを（あれば）破棄する。
		*/
		~ResourcePtr()
		{
			reset();
		}

		/**
		* [EN]
		* Destroys the owned pointee (if any) and becomes null. Safe to
		* call on an already-null ResourcePtr (no-op).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 所有しているオブジェクトを（あれば）破棄し、null になる。
		* 既に null な ResourcePtr に対して呼んでも安全（何もしない）。
		*/
		void reset()noexcept
		{
			if (pointer_ != nullptr)
			{
				delete pointer_;
				pointer_ = nullptr;
			}
		}

		/**
		* [EN]
		* Returns the raw owned pointer without releasing ownership.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 所有権を手放さずに、所有している生ポインタを返す。
		*/
		T* get()const noexcept
		{
			return pointer_;
		}

		/**
		* [EN]
		* Dereferences the owned pointee. Asserts in debug builds if
		* this ResourcePtr is null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 所有しているオブジェクトを間接参照する。この ResourcePtr が
		* null の場合、デバッグビルドではアサートする。
		*/
		T* operator->()const
		{
			SC_ASSERT(pointer_ != nullptr, "Dereferencing a null ResourcePtr.");
			return pointer_;
		}

		/**
		* [EN]
		* Same as operator->(), but returns a reference instead of a pointer.
		* Asserts in debug builds if this ResourcePtr is null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator->() と同様だが、ポインタではなく参照を返す。この
		* ResourcePtr が null の場合、デバッグビルドではアサートする。
		*/
		T& operator*()const
		{
			SC_ASSERT(pointer_ != nullptr, "Dereferencing a null ResourcePtr.");
			return *pointer_;
		}

		/**
		* [EN]
		* Returns whether this ResourcePtr owns a non-null pointee.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この ResourcePtr が null でないオブジェクトを所有しているかを返す。
		*/
		explicit operator bool()const noexcept
		{
			return pointer_ != nullptr;
		}

		/**
		* [EN]
		* Compares the owned raw pointers of two ResourcePtr instances for
		* equality.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つの ResourcePtr が所有している生ポインタ同士を比較する。
		*/
		friend Bool operator==(const ResourcePtr& lhs, const ResourcePtr& rhs)noexcept
		{
			return lhs.pointer_ == rhs.pointer_;
		}

		/**
		* [EN]
		* Returns whether this ResourcePtr is null.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この ResourcePtr が null かどうかを返す。
		*/
		friend Bool operator==(const ResourcePtr& lhs, std::nullptr_t)noexcept
		{
			return lhs.pointer_ == nullptr;
		}

	private:
		/// [EN] Grants every ResourcePtr<U> instantiation access to this
		///      instantiation's private members, needed by the converting
		///      constructor/assignment above.
		/// [JP] あらゆる ResourcePtr<U> のインスタンス化に対して、この
		///      インスタンス化の private メンバへのアクセスを許可する。
		///      上記の変換コンストラクタ/代入に必要。
		template <typename U>
		friend class ResourcePtr;

		/// [EN] The owned raw pointer; nullptr when this ResourcePtr owns nothing.
		/// [JP] 所有している生ポインタ。何も所有していない場合は nullptr。
		T* pointer_ = nullptr;
	};

	/**
	* [EN]
	* Constructs a T in place and returns it wrapped in a ResourcePtr,
	* analogous to std::make_unique. The sole way to obtain a
	* newly-allocated ResourcePtr, so every ResourcePtr<T> in the codebase
	* traces back to exactly one `new T`.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T をその場で構築し、ResourcePtr でラップして返す。std::make_unique
	* に相当する。新規に確保された ResourcePtr を得る唯一の手段であり、
	* コードベース中のあらゆる ResourcePtr<T> は必ずただ一つの `new T`
	* に由来する。
	*/
	template <typename T, typename... Args>
	[[nodiscard]] ResourcePtr<T> MakePtr(Args&&... args)
	{
		return ResourcePtr<T>(new T(std::forward<Args>(args)...));
	}
}
