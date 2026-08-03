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

		void CalculateFrameStats();

		Bool ConsumeCloseRequested();

		void RestoreAccessibilitySettings();

	private:
		static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		
		LRESULT CALLBACK HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	private:
		HINSTANCE instance_ = nullptr;

		HWND hwnd_ = nullptr;

		String title_;

		Timer timer_;

		Float fps_ = 0.0f;

		Uint32 framesPerSecond_ = 0;

		Float countBySeconds_ = 0.0f;

		STICKYKEYS originalStickyKeys_{};
		TOGGLEKEYS originalToggleKeys_{};
		FILTERKEYS originalFilterKeys_{};

		Bool closeRequested_ = false;
	};
}