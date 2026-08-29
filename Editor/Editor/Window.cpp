#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <FoundationEngine/Utility/Bootstrap.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <Editor/Editor/Window.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>

namespace SeedCore
{
	HWND Window::Create(const Bootstrap& boot)
	{
		if (!instance_)
		{
			instance_ = GetModuleHandle(nullptr);
		}

		if (hwnd_)
		{
			return hwnd_;
		}

#if defined(DEBUG) || defined(_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
		//_CrtSetBreakAlloc(152156);
#endif

		originalStickyKeys_.cbSize = sizeof(STICKYKEYS);
		originalToggleKeys_.cbSize = sizeof(TOGGLEKEYS);
		originalFilterKeys_.cbSize = sizeof(FILTERKEYS);
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &originalStickyKeys_, 0);
		SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &originalToggleKeys_, 0);
		SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &originalFilterKeys_, 0);

		STICKYKEYS stickyKeys{ sizeof(STICKYKEYS), 0 };
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stickyKeys, 0);

		TOGGLEKEYS toggleKeys{ sizeof(TOGGLEKEYS), 0 };
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &toggleKeys, 0);

		FILTERKEYS filterKeys{ sizeof(FILTERKEYS), 0 };
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &filterKeys, 0);

		WNDCLASSEXW wcex{};
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WindowProcedure;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance_;
		wcex.hIcon = static_cast<HICON>(LoadImageW(nullptr, L"../Runtime/Logo/SeedCore.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = boot.WindowDesc_.Title_;
		wcex.hIconSm = static_cast<HICON>(LoadImageW(nullptr, L"../Runtime/Logo/SeedCore.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
		RegisterClassExW(&wcex);

		RECT rect{ 0, 0, static_cast<LONG>(boot.WindowDesc_.Width_), static_cast<LONG>(boot.WindowDesc_.Height_) };
		DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		hwnd_ = CreateWindowExW(0, boot.WindowDesc_.Title_, L"SeedCore Engine", style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, instance_, NULL);
		title_ = String::intern(boot.WindowDesc_.Title_);
		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);
		SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

		return hwnd_;
	}

	Timer& Window::GetTimer()
	{
		return timer_;
	}

	void Window::CalculateFrameStats()
	{
		if (++framesPerSecond_, (timer_.Total() - countBySeconds_) >= 1.0f)
		{
			fps_ = static_cast<Float>(framesPerSecond_);
			framesPerSecond_ = 0;
			countBySeconds_ += 1.0f;
		}
	}

	Bool Window::ConsumeCloseRequested()
	{
		if (closeRequested_)
		{
			closeRequested_ = false;
			return true;
		}
		return false;
	}

	void Window::RestoreAccessibility()
	{
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &originalStickyKeys_, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &originalToggleKeys_, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &originalFilterKeys_, 0);
	}

	LRESULT CALLBACK Window::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		Window* p{ reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)) };
		return p ? p->HandleMessage(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
	}

	LRESULT CALLBACK Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (ImGuiRenderer::HandleMessage(hwnd, msg, wparam, lparam))
		{
			return true;
		}

		switch (msg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
		}
		break;
		case WM_CLOSE:
		{
			closeRequested_ = true;
			return 0;
		}
		case WM_DESTROY:
		{
			RestoreAccessibility();
			PostQuitMessage(0);
		}
		break;
		case WM_CREATE:
		{
			/// No Code
		}
		break;
		case WM_ENTERSIZEMOVE:
		{
			timer_.Stop();
		}
		break;
		case WM_EXITSIZEMOVE:
		{
			timer_.Start();
		}
		break;
		case WM_SIZE:
		{
			RECT clientRect{};
			GetClientRect(hwnd, &clientRect);
		}
		break;
		case WM_MOUSEWHEEL:
		{
			InputSystem::PushMouseWheel(static_cast<Float>(GET_WHEEL_DELTA_WPARAM(wparam)) / 120.0f);
		}
		break;
		case WM_SETFOCUS:
		{
			ImGuiRenderer::ClearInputKeys();
		}
		break;
		default:
		{
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		}
		return 0;
	}
}