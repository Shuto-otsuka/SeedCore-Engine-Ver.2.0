#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnFixedTick() entry point that
	* forwards to T::FixedTick(). Lets ComponentBase call OnFixedTick()
	* through a type-erased function pointer without needing to know
	* whether T actually implements FixedTick().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnFixedTick() エントリポイントを与える CRTP
	* ミックスイン。T::FixedTick() へ転送する。ComponentBase が、T が
	* 実際に FixedTick() を実装しているかを知らずとも、型消去された
	* 関数ポインタ経由で OnFixedTick() を呼び出せるようにする。
	*/
	template<typename T>
	class FixedTickable
	{
	public:
		/**
		* [EN]
		* Forwards to T::FixedTick(elapsedTime).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::FixedTick(elapsedTime) へ転送する。
		*/
		void OnFixedTick(Float elapsedTime)
		{
			static_cast<T*>(this)->FixedTick(elapsedTime);
		}
	};

	/// [EN] Satisfied when T::OnFixedTick(Float) is callable and returns void (i.e. T implements FixedTick() via the FixedTickable mixin).
	/// [JP] T::OnFixedTick(Float) が呼び出し可能で void を返す場合に満たされる（すなわち T が FixedTickable ミックスイン経由で FixedTick() を実装している）。
	template<typename T>
	concept HasFixedTick = requires(T& a, Float elapsedTime)
	{
		{ a.OnFixedTick(elapsedTime) } -> std::same_as<void>;
	};
}
