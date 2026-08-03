#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnTick() entry point that forwards
	* to T::Tick(). Lets ComponentBase call OnTick() through a
	* type-erased function pointer without needing to know whether T
	* actually implements Tick().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnTick() エントリポイントを与える CRTP ミックスイン。
	* T::Tick() へ転送する。ComponentBase が、T が実際に Tick() を
	* 実装しているかを知らずとも、型消去された関数ポインタ経由で
	* OnTick() を呼び出せるようにする。
	*/
	template<typename T>
	class Tickable
	{
	public:
		/**
		* [EN]
		* Forwards to T::Tick(elapsedTime).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::Tick(elapsedTime) へ転送する。
		*/
		void OnTick(Float elapsedTime)
		{
			static_cast<T*>(this)->Tick(elapsedTime);
		}
	};

	/// [EN] Satisfied when T::OnTick(Float) is callable and returns void (i.e. T implements Tick() via the Tickable mixin).
	/// [JP] T::OnTick(Float) が呼び出し可能で void を返す場合に満たされる（すなわち T が Tickable ミックスイン経由で Tick() を実装している）。
	template<typename T>
	concept HasTick = requires(T& a, Float elapsedTime)
	{
		{ a.OnTick(elapsedTime) } -> std::same_as<void>;
	};
}
