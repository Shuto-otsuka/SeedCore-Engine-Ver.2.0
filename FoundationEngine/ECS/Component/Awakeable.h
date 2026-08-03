#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnAwake() entry point that forwards
	* to T::Awake(). Lets ComponentBase call OnAwake() through a
	* type-erased function pointer without needing to know whether T
	* actually implements Awake().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnAwake() エントリポイントを与える CRTP ミックスイン。
	* T::Awake() へ転送する。ComponentBase が、T が実際に Awake() を
	* 実装しているかを知らずとも、型消去された関数ポインタ経由で
	* OnAwake() を呼び出せるようにする。
	*/
	template<typename T>
	class Awakeable
	{
	public:
		/**
		* [EN]
		* Forwards to T::Awake().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::Awake() へ転送する。
		*/
		void OnAwake()
		{
			static_cast<T*>(this)->Awake();
		}
	};

	/// [EN] Satisfied when T::OnAwake() is callable and returns void (i.e. T implements Awake() via the Awakeable mixin).
	/// [JP] T::OnAwake() が呼び出し可能で void を返す場合に満たされる（すなわち T が Awakeable ミックスイン経由で Awake() を実装している）。
	template<typename T>
	concept HasAwake = requires(T& a)
	{
		{ a.OnAwake() } -> std::same_as<void>;
	};
}
