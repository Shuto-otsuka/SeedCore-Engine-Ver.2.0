#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <FoundationEngine/Log/Assert.h>
#include <atomic>
#include <new>
#include <type_traits>
#include <utility>

namespace SeedCore
{
	template <typename T>
	class ResourceRef;

	/**
	* [EN]
	* Constructs a T in place and returns it wrapped in an owning
	* ResourceRef, analogous to std::make_shared. The sole way to obtain
	* a newly-allocated ResourceRef; the object and its control block are
	* allocated together in one heap allocation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T をその場で構築し、所有モードの ResourceRef でラップして返す。
	* std::make_shared に相当する。新規に確保された ResourceRef を得る
	* 唯一の手段であり、オブジェクト本体とコントロールブロックは1回の
	* ヒープ確保にまとめられる。
	*/
	template <typename T, typename... Args>
	[[nodiscard]] ResourceRef<T> MakeRef(Args&&... args);

	/**
	* [EN]
	* Creates a non-owning (observing) ResourceRef pointing at the same
	* object as owner, without incrementing the strong (owning) count.
	* Folds weak_ptr-equivalent behavior into the same ResourceRef type:
	* if the owner side has since released the object, get()/operator->
	* on the resulting ResourceRef safely report "gone" instead of
	* dangling. Intended for breaking ownership cycles: the edge that
	* should not keep the object alive is created via MakeObserve()
	* instead of a copy.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* owner と同じオブジェクトを指す、非所有（観測用）の ResourceRef を
	* 作る。所有側のカウントは増やさない。weak_ptr 相当の挙動を同じ
	* ResourceRef 型の中に内包しており、所有側が既にオブジェクトを
	* 解放していた場合、生成された ResourceRef の get()/operator-> は
	* ダングリングせず安全に「消滅済み」を報告する。所有権の循環を
	* 切りたい辺で、コピーの代わりに MakeObserve() を使うことを想定
	* している。
	*/
	template <typename T>
	[[nodiscard]] ResourceRef<T> MakeObserve(const ResourceRef<T>& owner)noexcept;

	/// [EN] Whether a ResourceRef keeps its pointee alive (Owning) or merely observes it without affecting its lifetime (Observing).
	/// [JP] ResourceRef が対象の生存期間を握っている（Owning）か、生存期間には関与せず観測するだけ（Observing）かを表す。
	enum class RefMode : Uint8
	{
		/// [EN] Counts toward the strong (owning) count; keeps the pointee alive.
		/// [JP] strong（所有）カウントに加算される。対象を生かし続ける。
		Owning,

		/// [EN] Counts toward the observer count only; never keeps the pointee alive.
		/// [JP] observer カウントにのみ加算される。対象を生かし続けることはない。
		Observing,
	};

	namespace Detail
	{
		/**
		* [EN]
		* Type-erased base of the control block: holds the reference
		* counts and lifetime flag independently of the concrete object
		* type T, so a ResourceRef<Derived> can be moved/converted into a
		* ResourceRef<Base> while still sharing the same control block.
		* DestroyObject() is virtual so the correct concrete destructor
		* runs regardless of which ResourceRef<U> ends up triggering it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コントロールブロックの型消去された基底クラス。参照カウントと
		* 生存フラグを、具体的なオブジェクト型 T から独立して保持する。
		* これにより ResourceRef<Derived> を ResourceRef<Base> へムーブ/
		* 変換しても同じコントロールブロックを共有し続けられる。
		* DestroyObject() は仮想関数にしてあり、どの ResourceRef<U> が
		* 破棄を引き起こしても正しい具象型のデストラクタが走る。
		*/
		struct RefControlBase
		{
			/// [EN] Number of owning (strong) ResourceRef instances currently referring to this block. Atomic: ResourceRef is safe to copy/destroy across threads.
			/// [JP] このブロックを参照している所有（strong）モードの ResourceRef の数。atomic。ResourceRef はスレッドをまたいだコピー/破棄が安全。
			std::atomic<Uint32> strongCount_{ 1 };

			/// [EN] Number of observing ResourceRef instances referring to this block, plus one implicit reference held by the strong side until every owning ResourceRef is gone (the standard shared/weak dual-count trick: whichever side's decrement brings this to zero is the one that deallocates, so it only ever happens once).
			/// [JP] このブロックを参照している観測（observing）モードの ResourceRef の数に加えて、所有側が持つ暗黙の参照を1つ含む（strongが尽きるまで保持される）。shared/weak二重カウントの定石で、これを0まで減らした側だけが解放を行うため、解放は必ず1回だけ起きる。
			std::atomic<Uint32> observerCount_{ 1 };

			/// [EN] Whether the pointee has not yet been destroyed. Cleared by DestroyObject(). Atomic so an Observing ResourceRef on another thread can safely query it.
			/// [JP] 対象がまだ破棄されていないかどうか。DestroyObject() でfalseになる。別スレッドの Observing な ResourceRef が安全に問い合わせられるよう atomic にしている。
			std::atomic<Bool> alive_{ true };

			/**
			* [EN]
			* Virtual destructor so `delete` through a RefControlBase*
			* correctly deallocates the full derived RefControlBlock<T>.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* RefControlBase* 経由での `delete` が、派生側の
			* RefControlBlock<T> 全体を正しく解放できるようにするための
			* 仮想デストラクタ。
			*/
			virtual ~RefControlBase() = default;

			/**
			* [EN]
			* Destroys the held object in place (without deallocating the
			* control block itself) and marks the block as no longer alive.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 保持しているオブジェクトをその場で破棄し（コントロール
			* ブロック自体は解放しない）、ブロックを「生存していない」
			* 状態にする。
			*/
			virtual void DestroyObject() = 0;
		};

		/**
		* [EN]
		* Concrete control block for T: embeds storage for exactly one T
		* alongside the base's ref counts, so MakeRef() only needs a
		* single heap allocation for both the object and its bookkeeping.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T 用の具象コントロールブロック。基底クラスの参照カウントと
		* 並べて、ちょうど1つ分の T のストレージを内包している。これに
		* より MakeRef() はオブジェクトと管理情報の両方をヒープ1回の
		* 確保だけで済ませられる。
		*/
		template <typename T>
		struct RefControlBlock : RefControlBase
		{
			/// [EN] Raw storage for the T instance, placement-new'd by MakeRef() and destroyed in place by DestroyObject().
			/// [JP] T インスタンスの生ストレージ。MakeRef() でplacement newされ、DestroyObject() でその場で破棄される。
			alignas(T) Byte storage_[sizeof(T)];

			/**
			* [EN]
			* Returns a pointer to the T instance living inside storage_.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* storage_ 内に存在する T インスタンスへのポインタを返す。
			*/
			T* GetPointer()
			{
				return reinterpret_cast<T*>(storage_);
			}

			/**
			* [EN]
			* Calls the T destructor in place and marks the block as no
			* longer alive. Does not deallocate storage_ itself.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* T のデストラクタをその場で呼び出し、ブロックを「生存して
			* いない」状態にする。storage_ 自体の解放は行わない。
			*/
			void DestroyObject()override
			{
				GetPointer()->~T();
				alive_.store(false, std::memory_order_release);
			}
		};
	}

	/**
	* [EN]
	* Shared-ownership reference, custom-built as a replacement for
	* std::shared_ptr + std::weak_ptr combined into a single type. Every
	* ResourceRef is either Owning (keeps the pointee alive, like
	* shared_ptr) or Observing (does not, like weak_ptr, but exposed
	* through the exact same interface instead of a separate class — see
	* MakeObserve()). Reference counts are atomic, so a ResourceRef is
	* safe to copy/destroy/query from multiple threads concurrently (the
	* same guarantee std::shared_ptr's control block gives). Supports
	* moving/converting from a ResourceRef<U> to a ResourceRef<T> when U*
	* is implicitly convertible to T* (e.g. Derived -> Base).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* std::shared_ptr と std::weak_ptr を1つの型に統合する形で自作した
	* 共有参照。あらゆる ResourceRef は Owning（shared_ptr のように対象を
	* 生かし続ける）か Observing（weak_ptr のように生かし続けないが、
	* 別クラスではなく全く同じインターフェースで扱える — MakeObserve()
	* 参照）のどちらか。参照カウントは atomic なので、複数スレッドから
	* 同時にコピー/破棄/問い合わせしても安全（std::shared_ptr の
	* コントロールブロックと同等の保証）。U* が T* へ暗黙変換可能
	* （例: 派生 -> 基底）な場合、ResourceRef<U> から ResourceRef<T> への
	* ムーブ/変換に対応している。
	*/
	template <typename T>
	class ResourceRef
	{
	public:
		/**
		* [EN]
		* Default-constructs an empty ResourceRef (refers to nothing).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 何も参照していない空の ResourceRef を構築する。
		*/
		constexpr ResourceRef()noexcept = default;

		/**
		* [EN]
		* Constructs an empty ResourceRef from nullptr, for `= nullptr`
		* -style initialization at call sites.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nullptr から空の ResourceRef を構築する。呼び出し側での
		* `= nullptr` 形式の初期化のために用意している。
		*/
		constexpr ResourceRef(std::nullptr_t)noexcept
		{
			/// No Code
		}

		/**
		* [EN]
		* Copies other, sharing the same control block and preserving
		* other's mode (an Owning copy stays Owning, an Observing copy
		* stays Observing).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* other をコピーし、同じコントロールブロックを共有する。other の
		* モードはそのまま引き継ぐ（Owning のコピーは Owning のまま、
		* Observing のコピーは Observing のまま）。
		*/
		ResourceRef(const ResourceRef& other)noexcept: control_(other.control_), pointer_(other.pointer_), mode_(other.mode_)
		{
			AddRef();
		}

		/**
		* [EN]
		* Transfers other's reference (and its mode) to this instance,
		* leaving other empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* other の参照（とそのモード）をこのインスタンスへ移し、other は
		* 空になる。
		*/
		ResourceRef(ResourceRef&& other)noexcept: control_(other.control_), pointer_(other.pointer_), mode_(other.mode_)
		{
			other.control_ = nullptr;
			other.pointer_ = nullptr;
		}

		/**
		* [EN]
		* Converting copy constructor: shares ownership with a
		* ResourceRef<U> when U* implicitly converts to T* (e.g. observing
		* or co-owning a Derived instance through a Base-typed
		* ResourceRef).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換コピーコンストラクタ。U* が T* へ暗黙変換可能な場合に
		* ResourceRef<U> と所有権を共有する（例: 派生インスタンスを
		* 基底型の ResourceRef として共同所有/観測する）。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourceRef(const ResourceRef<U>& other)noexcept: control_(other.control_), pointer_(other.pointer_), mode_(other.mode_)
		{
			AddRef();
		}

		/**
		* [EN]
		* Converting move constructor: transfers a ResourceRef<U>'s
		* reference (and its mode) to this instance when U* implicitly
		* converts to T*, leaving other empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換ムーブコンストラクタ。U* が T* へ暗黙変換可能な場合に、
		* ResourceRef<U> の参照（とそのモード）をこのインスタンスへ移し、
		* other は空になる。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourceRef(ResourceRef<U>&& other)noexcept: control_(other.control_), pointer_(other.pointer_), mode_(other.mode_)
		{
			other.control_ = nullptr;
			other.pointer_ = nullptr;
		}

		/**
		* [EN]
		* Releases this instance's reference (if any) via Release().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Release() を通じて、このインスタンスの参照を（あれば）解放する。
		*/
		~ResourceRef()
		{
			Release();
		}

		/**
		* [EN]
		* Releases the current reference (if any), then copies other,
		* sharing its control block and mode.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の参照を（あれば）解放し、other をコピーして、その
		* コントロールブロックとモードを共有する。
		*/
		ResourceRef& operator=(const ResourceRef& other)noexcept
		{
			if (this != &other)
			{
				Release();
				control_ = other.control_;
				pointer_ = other.pointer_;
				mode_ = other.mode_;
				AddRef();
			}
			return *this;
		}

		/**
		* [EN]
		* Releases the current reference (if any), then transfers other's
		* reference (and its mode) to this instance, leaving other empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の参照を（あれば）解放し、other の参照（とそのモード）を
		* このインスタンスへ移す。other は空になる。
		*/
		ResourceRef& operator=(ResourceRef&& other)noexcept
		{
			if (this != &other)
			{
				Release();
				control_ = other.control_;
				pointer_ = other.pointer_;
				mode_ = other.mode_;
				other.control_ = nullptr;
				other.pointer_ = nullptr;
			}
			return *this;
		}

		/**
		* [EN]
		* Converting copy assignment: same as the converting copy
		* constructor, but for an already-constructed ResourceRef.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換コピー代入。変換コピーコンストラクタと同様だが、既に
		* 構築済みの ResourceRef に対して行う。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourceRef& operator=(const ResourceRef<U>& other)noexcept
		{
			Release();
			control_ = other.control_;
			pointer_ = other.pointer_;
			mode_ = other.mode_;
			AddRef();
			return *this;
		}

		/**
		* [EN]
		* Converting move assignment: same as the converting move
		* constructor, but for an already-constructed ResourceRef.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 変換ムーブ代入。変換ムーブコンストラクタと同様だが、既に
		* 構築済みの ResourceRef に対して行う。
		*/
		template <typename U>
			requires std::is_convertible_v<U*, T*>
		ResourceRef& operator=(ResourceRef<U>&& other)noexcept
		{
			Release();
			control_ = other.control_;
			pointer_ = other.pointer_;
			mode_ = other.mode_;
			other.control_ = nullptr;
			other.pointer_ = nullptr;
			return *this;
		}

		/**
		* [EN]
		* Resets to empty, releasing the current reference (if any).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空に戻し、現在の参照を（あれば）解放する。
		*/
		ResourceRef& operator=(std::nullptr_t)noexcept
		{
			Release();
			return *this;
		}

		/**
		* [EN]
		* Resets to empty, releasing the current reference (if any). Safe
		* to call on an already-empty ResourceRef (no-op).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空に戻し、現在の参照を（あれば）解放する。既に空な ResourceRef
		* に対して呼んでも安全（何もしない）。
		*/
		void reset()
		{
			Release();
		}

		/**
		* [EN]
		* Returns the raw pointer to the pointee, or nullptr if this
		* ResourceRef is empty or the pointee has already been destroyed
		* (relevant for an Observing ResourceRef whose owner has released
		* it).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 対象への生ポインタを返す。この ResourceRef が空、または対象が
		* 既に破棄されている場合（所有側が解放済みの Observing な
		* ResourceRef で起こりうる）は nullptr を返す。
		*/
		T* get()const noexcept
		{
			return (control_ != nullptr && control_->alive_.load(std::memory_order_acquire)) ? pointer_ : nullptr;
		}

		/**
		* [EN]
		* Dereferences the pointee. Asserts in debug builds if this
		* ResourceRef is empty or the pointee has already been destroyed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 対象を間接参照する。この ResourceRef が空、または対象が既に
		* 破棄されている場合、デバッグビルドではアサートする。
		*/
		T* operator->()const
		{
			SC_ASSERT(control_ != nullptr && control_->alive_.load(std::memory_order_acquire), "Dereferencing an empty or expired ResourceRef.");
			return pointer_;
		}

		/**
		* [EN]
		* Same as operator->(), but returns a reference instead of a
		* pointer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator->() と同様だが、ポインタではなく参照を返す。
		*/
		T& operator*()const
		{
			SC_ASSERT(control_ != nullptr && control_->alive_.load(std::memory_order_acquire), "Dereferencing an empty or expired ResourceRef.");
			return *pointer_;
		}

		/**
		* [EN]
		* Returns the current owning (strong) reference count, or 0 if
		* this ResourceRef is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の所有（strong）参照カウントを返す。この ResourceRef が
		* 空の場合は 0 を返す。
		*/
		Uint32 use_count()const noexcept
		{
			return (control_ != nullptr) ? control_->strongCount_.load(std::memory_order_relaxed) : 0;
		}

		/**
		* [EN]
		* Returns whether this ResourceRef is in Observing mode (does not
		* keep its pointee alive).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この ResourceRef が Observing モード（対象を生かし続けない）
		* かどうかを返す。
		*/
		Bool observing()const noexcept
		{
			return mode_ == RefMode::Observing;
		}

		/**
		* [EN]
		* Returns whether this ResourceRef refers to a still-alive pointee.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この ResourceRef がまだ生存している対象を参照しているかを返す。
		*/
		Bool exists()const noexcept
		{
			return control_ != nullptr && control_->alive_.load(std::memory_order_acquire);
		}

		/**
		* [EN]
		* Same as exists().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* exists() と同じ。
		*/
		explicit operator bool()const noexcept
		{
			return exists();
		}

		/**
		* [EN]
		* Compares the control blocks of two ResourceRef instances for
		* equality (i.e. whether they refer to the same underlying
		* object).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つの ResourceRef のコントロールブロック同士を比較する
		* （＝同じ実体を参照しているかどうか）。
		*/
		friend Bool operator==(const ResourceRef& lhs, const ResourceRef& rhs)noexcept
		{
			return lhs.control_ == rhs.control_;
		}

		/**
		* [EN]
		* Returns whether this ResourceRef is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この ResourceRef が空かどうかを返す。
		*/
		friend Bool operator==(const ResourceRef& lhs, std::nullptr_t)noexcept
		{
			return !lhs.exists();
		}

	private:
		/// [EN] Grants every ResourceRef<U> instantiation access to this instantiation's private members, needed by the converting constructors/assignments above.
		/// [JP] あらゆる ResourceRef<U> のインスタンス化に対して、この インスタンス化の private メンバへのアクセスを許可する。上記の変換コンストラクタ/代入に必要。
		template <typename U>
		friend class ResourceRef;

		template <typename U, typename... Args>
		friend ResourceRef<U> MakeRef(Args&&... args);

		template <typename U>
		friend ResourceRef<U> MakeObserve(const ResourceRef<U>& owner)noexcept;

		/**
		* [EN]
		* Private constructor used internally by MakeRef()/MakeObserve()
		* to wrap an already-set-up control block and pointer into a
		* ResourceRef with the given mode.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* MakeRef()/MakeObserve() が内部的に使う private コンストラクタ。
		* 既にセットアップ済みのコントロールブロックとポインタを、指定
		* されたモードの ResourceRef としてラップする。
		*/
		ResourceRef(Detail::RefControlBase* control, T* pointer, RefMode mode)noexcept: control_(control), pointer_(pointer), mode_(mode)
		{
			/// No Code
		}

		/**
		* [EN]
		* Increments the appropriate count on the control block (strong
		* for Owning, observer for Observing) according to this
		* instance's mode_. No-op if this ResourceRef is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このインスタンスの mode_ に応じて、コントロールブロックの
		* 適切なカウント（Owning なら strong、Observing なら observer）を
		* 増やす。この ResourceRef が空の場合は何もしない。
		*/
		void AddRef()noexcept
		{
			if (control_ == nullptr)
			{
				return;
			}
			if (mode_ == RefMode::Owning)
			{
				control_->strongCount_.fetch_add(1, std::memory_order_relaxed);
			}
			else
			{
				control_->observerCount_.fetch_add(1, std::memory_order_relaxed);
			}
		}

		/**
		* [EN]
		* Decrements the appropriate count on the control block according
		* to this instance's mode_. When the strong count reaches zero,
		* destroys the pointee via DestroyObject() and releases the
		* implicit observer reference the strong side has held since
		* construction (see RefControlBase::observerCount_). The control
		* block is deallocated by whichever side's decrement is the one
		* that brings observerCount_ to zero, guaranteeing exactly one
		* deallocation regardless of thread interleaving. No-op if this
		* ResourceRef is already empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このインスタンスの mode_ に応じて、コントロールブロックの
		* 適切なカウントを減らす。strong カウントが0になったら
		* DestroyObject() 経由で対象を破棄し、構築時から strong 側が
		* 保持していた暗黙の observer 参照を手放す
		* （RefControlBase::observerCount_ 参照）。コントロールブロック自体の
		* 解放は、observerCount_ を0まで減らした側が行うため、スレッドの
		* 実行順序に関わらず解放は必ず1回だけ起きる。この ResourceRef が
		* 既に空の場合は何もしない。
		*/
		void Release()
		{
			if (control_ == nullptr)
			{
				return;
			}

			if (mode_ == RefMode::Owning)
			{
				if (control_->strongCount_.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					control_->DestroyObject();
					if (control_->observerCount_.fetch_sub(1, std::memory_order_acq_rel) == 1)
					{
						delete control_;
					}
				}
			}
			else
			{
				if (control_->observerCount_.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					delete control_;
				}
			}
			control_ = nullptr;
			pointer_ = nullptr;
		}

		/// [EN] Type-erased control block shared with every ResourceRef (of any related type) referring to the same object; nullptr when this ResourceRef is empty.
		/// [JP] 同じオブジェクトを参照する（関連する型も含む）あらゆる ResourceRef と共有する、型消去されたコントロールブロック。この ResourceRef が空の場合は nullptr。
		Detail::RefControlBase* control_ = nullptr;

		/// [EN] Direct pointer to the pointee (as T*), cached alongside control_ so get()/operator-> don't need a virtual call.
		/// [JP] 対象への直接ポインタ（T* として）。get()/operator-> が仮想呼び出しを介さずに済むよう、control_ と並べてキャッシュしている。
		T* pointer_ = nullptr;

		/// [EN] Whether this instance is Owning or Observing; determines which count AddRef()/Release() touch.
		/// [JP] このインスタンスが Owning か Observing か。AddRef()/Release() がどちらのカウントを操作するかを決める。
		RefMode mode_ = RefMode::Owning;
	};

	template <typename T, typename... Args>
	[[nodiscard]] ResourceRef<T> MakeRef(Args&&... args)
	{
		Detail::RefControlBlock<T>* block = new Detail::RefControlBlock<T>();
		try
		{
			new (block->storage_) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			delete block;
			throw;
		}
		return ResourceRef<T>(block, block->GetPointer(), RefMode::Owning);
	}

	template <typename T>
	[[nodiscard]] ResourceRef<T> MakeObserve(const ResourceRef<T>& owner)noexcept
	{
		ResourceRef<T> observer(owner.control_, owner.pointer_, RefMode::Observing);
		observer.AddRef();
		return observer;
	}
}
