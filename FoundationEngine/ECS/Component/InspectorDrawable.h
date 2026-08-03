#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* CRTP mixin giving T a uniform OnInspectorGUI() entry point that
	* forwards to T::InspectorGUI(). Lets ComponentBase call
	* OnInspectorGUI() through a type-erased function pointer without
	* needing to know whether T actually implements InspectorGUI().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T に統一された OnInspectorGUI() エントリポイントを与える CRTP
	* ミックスイン。T::InspectorGUI() へ転送する。ComponentBase が、T が
	* 実際に InspectorGUI() を実装しているかを知らずとも、型消去された
	* 関数ポインタ経由で OnInspectorGUI() を呼び出せるようにする。
	*/
	template<typename T>
	class InspectorDrawable
	{
	public:
		/**
		* [EN]
		* Forwards to T::InspectorGUI().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::InspectorGUI() へ転送する。
		*/
		void OnInspectorGUI()
		{
			static_cast<T*>(this)->InspectorGUI();
		}
	};

	/// [EN] Satisfied when T::OnInspectorGUI() is callable and returns void (i.e. T implements InspectorGUI() via the InspectorDrawable mixin).
	/// [JP] T::OnInspectorGUI() が呼び出し可能で void を返す場合に満たされる（すなわち T が InspectorDrawable ミックスイン経由で InspectorGUI() を実装している）。
	template<typename T>
	concept HasInspectorGUI = requires(T& a)
	{
		{ a.OnInspectorGUI() } -> std::same_as<void>;
	};
}
