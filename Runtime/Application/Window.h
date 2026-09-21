#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Time/Timer.h>

namespace SeedCore
{
	struct Bootstrap;

	class Window :public NonTransferable
	{
	public:
		Window() = default;
		~Window() = default;

		HWND Create(const Bootstrap& boot);

		Timer& GetTimer();

		Bool ConsumeCloseRequested();

		Bool ConsumeResized(Uint32& width, Uint32& height);

		void RestoreAccessibility();

	private:
		static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	private:
		HINSTANCE instance_ = nullptr;

		HWND hwnd_ = nullptr;

		Timer timer_;

		STICKYKEYS originalStickyKeys_{};
		TOGGLEKEYS originalToggleKeys_{};
		FILTERKEYS originalFilterKeys_{};

		DWORD windowedStyle_ = 0;

		RECT windowedRect_{};

		Bool fullscreen_ = false;

		Bool closeRequested_ = false;

		Bool resized_ = false;

		Uint32 clientWidth_ = 0;

		Uint32 clientHeight_ = 0;
	};
}
