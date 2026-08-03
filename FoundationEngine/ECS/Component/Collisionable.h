#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T uniform OnCollisionEnter/Stay/Exit() entry
	* points that forward to T::CollisionEnter/Stay/Exit(). Lets
	* ComponentBase call these through type-erased function pointers
	* without needing to know whether T actually implements them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnCollisionEnter/Stay/Exit() エントリポイントを
	* 与える CRTP ミックスイン。T::CollisionEnter/Stay/Exit() へ転送する。
	* ComponentBase が、T が実際にこれらを実装しているかを知らずとも、
	* 型消去された関数ポインタ経由で呼び出せるようにする。
	*/
	template<typename T>
	class Collisionable
	{
	public:
		/**
		* [EN]
		* Forwards to T::CollisionEnter(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::CollisionEnter(other) へ転送する。
		*/
		void OnCollisionEnter(Entity other)
		{
			static_cast<T*>(this)->CollisionEnter(other);
		}

		/**
		* [EN]
		* Forwards to T::CollisionStay(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::CollisionStay(other) へ転送する。
		*/
		void OnCollisionStay(Entity other)
		{
			static_cast<T*>(this)->CollisionStay(other);
		}

		/**
		* [EN]
		* Forwards to T::CollisionExit(other).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::CollisionExit(other) へ転送する。
		*/
		void OnCollisionExit(Entity other)
		{
			static_cast<T*>(this)->CollisionExit(other);
		}
	};

	/// [EN] Satisfied when T::OnCollisionEnter(Entity) is callable and returns void (i.e. T implements CollisionEnter() via the Collisionable mixin).
	/// [JP] T::OnCollisionEnter(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Collisionable ミックスイン経由で CollisionEnter() を実装している）。
	template<typename T>
	concept HasCollisionEnter = requires(T& a, Entity other)
	{
		{ a.OnCollisionEnter(other) } -> std::same_as<void>;
	};

	/// [EN] Satisfied when T::OnCollisionStay(Entity) is callable and returns void (i.e. T implements CollisionStay() via the Collisionable mixin).
	/// [JP] T::OnCollisionStay(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Collisionable ミックスイン経由で CollisionStay() を実装している）。
	template<typename T>
	concept HasCollisionStay = requires(T& a, Entity other)
	{
		{ a.OnCollisionStay(other) } -> std::same_as<void>;
	};

	/// [EN] Satisfied when T::OnCollisionExit(Entity) is callable and returns void (i.e. T implements CollisionExit() via the Collisionable mixin).
	/// [JP] T::OnCollisionExit(Entity) が呼び出し可能で void を返す場合に満たされる（すなわち T が Collisionable ミックスイン経由で CollisionExit() を実装している）。
	template<typename T>
	concept HasCollisionExit = requires(T& a, Entity other)
	{
		{ a.OnCollisionExit(other) } -> std::same_as<void>;
	};
}
