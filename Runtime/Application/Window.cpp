#include <FoundationEngine/Utility/Bootstrap.h>
#include <FoundationEngine/Input/InputSystem.h>
#include <Runtime/Application/Window.h>

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

		HICON icon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
		HICON smallIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
		if (!icon)
		{
			icon = static_cast<HICON>(LoadImageW(nullptr, L"../Runtime/Logo/SeedCore.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
			smallIcon = static_cast<HICON>(LoadImageW(nullptr, L"../Runtime/Logo/SeedCore.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
		}

		WNDCLASSEXW wcex{};
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WindowProcedure;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance_;
		wcex.hIcon = icon;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = boot.WindowDesc_.Title_;
		wcex.hIconSm = smallIcon;
		RegisterClassExW(&wcex);

		windowedStyle_ = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;

		RECT rect{ 0, 0, static_cast<LONG>(boot.WindowDesc_.Width_), static_cast<LONG>(boot.WindowDesc_.Height_) };
		AdjustWindowRect(&rect, windowedStyle_, FALSE);
		hwnd_ = CreateWindowExW(0, boot.WindowDesc_.Title_, boot.WindowDesc_.Title_, windowedStyle_, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance_, nullptr);
		SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		if (boot.WindowDesc_.Fullscreen_)
		{
			GetWindowRect(hwnd_, &windowedRect_);

			MONITORINFO monitorInfo{};
			monitorInfo.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);

			SetWindowLongPtr(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
			SetWindowPos(hwnd_, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

			fullscreen_ = true;
		}

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);

		return hwnd_;
	}

	Timer& Window::GetTimer()
	{
		return timer_;
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

	Bool Window::ConsumeResized(Uint32& width, Uint32& height)
	{
		if (!resized_)
		{
			return false;
		}

		resized_ = false;
		width = clientWidth_;
		height = clientHeight_;
		return true;
	}

	void Window::RestoreAccessibility()
	{
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &originalStickyKeys_, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &originalToggleKeys_, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &originalFilterKeys_, 0);
	}

	LRESULT CALLBACK Window::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		return window ? window->HandleMessage(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
	}

	LRESULT CALLBACK Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
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
			Uint32 width = static_cast<Uint32>(LOWORD(lparam));
			Uint32 height = static_cast<Uint32>(HIWORD(lparam));
			if (wparam != SIZE_MINIMIZED && width > 0 && height > 0 && (width != clientWidth_ || height != clientHeight_))
			{
				clientWidth_ = width;
				clientHeight_ = height;
				resized_ = true;
			}
		}
		break;
		case WM_ACTIVATEAPP:
		{
			InputSystem::SetInputEnabled(wparam != FALSE);
		}
		break;
		case WM_SYSKEYDOWN:
		{
			if (wparam == VK_RETURN && (lparam & (1 << 29)))
			{
				if (!fullscreen_)
				{
					GetWindowRect(hwnd_, &windowedRect_);

					MONITORINFO monitorInfo{};
					monitorInfo.cbSize = sizeof(MONITORINFO);
					GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);

					SetWindowLongPtr(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
					SetWindowPos(hwnd_, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

					fullscreen_ = true;
				}
				else
				{
					SetWindowLongPtr(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowedStyle_ | WS_VISIBLE));
					SetWindowPos(hwnd_, HWND_NOTOPMOST, windowedRect_.left, windowedRect_.top, windowedRect_.right - windowedRect_.left, windowedRect_.bottom - windowedRect_.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

					fullscreen_ = false;
				}
				return 0;
			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		case WM_MENUCHAR:
		{
			return MAKELRESULT(0, MNC_CLOSE);
		}
		case WM_MOUSEWHEEL:
		{
			InputSystem::PushMouseWheel(static_cast<Float>(GET_WHEEL_DELTA_WPARAM(wparam)) / 120.0f);
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
