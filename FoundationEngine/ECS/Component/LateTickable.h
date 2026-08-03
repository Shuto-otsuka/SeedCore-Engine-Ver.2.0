#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnLateTick() entry point that
	* forwards to T::LateTick(). Lets ComponentBase call OnLateTick()
	* through a type-erased function pointer without needing to know
	* whether T actually implements LateTick().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnLateTick() エントリポイントを与える CRTP
	* ミックスイン。T::LateTick() へ転送する。ComponentBase が、T が
	* 実際に LateTick() を実装しているかを知らずとも、型消去された
	* 関数ポインタ経由で OnLateTick() を呼び出せるようにする。
	*/
	template<typename T>
	class LateTickable
	{
	public:
		/**
		* [EN]
		* Forwards to T::LateTick(elapsedTime).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::LateTick(elapsedTime) へ転送する。
		*/
		void OnLateTick(Float elapsedTime)
		{
			static_cast<T*>(this)->LateTick(elapsedTime);
		}
	};

	/// [EN] Satisfied when T::OnLateTick(Float) is callable and returns void (i.e. T implements LateTick() via the LateTickable mixin).
	/// [JP] T::OnLateTick(Float) が呼び出し可能で void を返す場合に満たされる（すなわち T が LateTickable ミックスイン経由で LateTick() を実装している）。
	template<typename T>
	concept HasLateTick = requires(T& a, Float elapsedTime)
	{
		{ a.OnLateTick(elapsedTime) } -> std::same_as<void>;
	};
}
