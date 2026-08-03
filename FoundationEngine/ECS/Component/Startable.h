#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnStart() entry point that forwards
	* to T::Start(). Lets ComponentBase call OnStart() through a
	* type-erased function pointer without needing to know whether T
	* actually implements Start().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnStart() エントリポイントを与える CRTP ミックスイン。
	* T::Start() へ転送する。ComponentBase が、T が実際に Start() を
	* 実装しているかを知らずとも、型消去された関数ポインタ経由で
	* OnStart() を呼び出せるようにする。
	*/
	template<typename T>
	class Startable
	{
	public:
		/**
		* [EN]
		* Forwards to T::Start().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::Start() へ転送する。
		*/
		void OnStart()
		{
			static_cast<T*>(this)->Start();
		}
	};

	/// [EN] Satisfied when T::OnStart() is callable and returns void (i.e. T implements Start() via the Startable mixin).
	/// [JP] T::OnStart() が呼び出し可能で void を返す場合に満たされる（すなわち T が Startable ミックスイン経由で Start() を実装している）。
	template<typename T>
	concept HasStart = requires(T& a)
	{
		{ a.OnStart() } -> std::same_as<void>;
	};
}
