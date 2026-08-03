#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnDestroy() entry point that
	* forwards to T::Destroy(). Lets ComponentBase call OnDestroy()
	* through a type-erased function pointer without needing to know
	* whether T actually implements Destroy().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnDestroy() エントリポイントを与える CRTP
	* ミックスイン。T::Destroy() へ転送する。ComponentBase が、T が
	* 実際に Destroy() を実装しているかを知らずとも、型消去された
	* 関数ポインタ経由で OnDestroy() を呼び出せるようにする。
	*/
	template<typename T>
	class Destroyable
	{
	public:
		/**
		* [EN]
		* Forwards to T::Destroy().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::Destroy() へ転送する。
		*/
		void OnDestroy()
		{
			static_cast<T*>(this)->Destroy();
		}
	};

	/// [EN] Satisfied when T::OnDestroy() is callable and returns void (i.e. T implements Destroy() via the Destroyable mixin).
	/// [JP] T::OnDestroy() が呼び出し可能で void を返す場合に満たされる（すなわち T が Destroyable ミックスイン経由で Destroy() を実装している）。
	template<typename T>
	concept HasDestroy = requires(T& a)
	{
		{ a.OnDestroy() } -> std::same_as<void>;
	};
}
