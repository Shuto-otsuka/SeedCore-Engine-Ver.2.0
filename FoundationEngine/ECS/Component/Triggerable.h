#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T uniform OnTriggerEnter/Stay/Exit() entry
	* points that forward to T::TriggerEnter/Stay/Exit(). Lets
	* ComponentBase call these through type-erased function pointers
	* without needing to know whether T actually implements them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnTriggerEnter/Stay/Exit() エントリポイントを
	* 与える CRTP ミックスイン。T::TriggerEnter/Stay/Exit() へ転送する。
	* ComponentBase が、T が実際にこれらを実装しているかを知らずとも、
	* 型消去された関数ポインタ経由で呼び出せるようにする。
	*/
	template<typename T>
	class Triggerable
	{
	public:
		/**
		* [EN]
		* Forwards to T::TriggerEnter(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::TriggerEnter(other) へ転送する。
		*/
		void OnTriggerEnter(Entity other)
		{
			static_cast<T*>(this)->TriggerEnter(other);
		}

		/**
		* [EN]
		* Forwards to T::TriggerStay(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::TriggerStay(other) へ転送する。
		*/
		void OnTriggerStay(Entity other)
		{
			static_cast<T*>(this)->TriggerStay(other);
		}

		/**
		* [EN]
		* Forwards to T::TriggerExit(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::TriggerExit(other) へ転送する。
		*/
		void OnTriggerExit(Entity other)
		{
			static_cast<T*>(this)->TriggerExit(other);
		}
	};

	/// [EN] Satisfied when T::OnTriggerEnter(Entity) is callable and returns void (i.e. T implements TriggerEnter() via the Triggerable mixin).
	/// [JP] T::OnTriggerEnter(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Triggerable ミックスイン経由で TriggerEnter() を実装している）。
	template<typename T>
	concept HasTriggerEnter = requires(T& a, Entity other)
	{
		{ a.OnTriggerEnter(other) } -> std::same_as<void>;
	};

	/// [EN] Satisfied when T::OnTriggerStay(Entity) is callable and returns void (i.e. T implements TriggerStay() via the Triggerable mixin).
	/// [JP] T::OnTriggerStay(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Triggerable ミックスイン経由で TriggerStay() を実装している）。
	template<typename T>
	concept HasTriggerStay = requires(T& a, Entity other)
	{
		{ a.OnTriggerStay(other) } -> std::same_as<void>;
	};

	/// [EN] Satisfied when T::OnTriggerExit(Entity) is callable and returns void (i.e. T implements TriggerExit() via the Triggerable mixin).
	/// [JP] T::OnTriggerExit(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Triggerable ミックスイン経由で TriggerExit() を実装している）。
	template<typename T>
	concept HasTriggerExit = requires(T& a, Entity other)
	{
		{ a.OnTriggerExit(other) } -> std::same_as<void>;
	};
}
