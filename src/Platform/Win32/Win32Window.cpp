#include "Platform/Win32/Win32Window.h"
#include "Win32ResourceIds.h"

#include <windowsx.h>

namespace NeneEngine
{
	namespace
	{
		constexpr wchar_t kWindowClassName[] = L"NeneEngineWindowClass";

		HICON LoadAppIcon(int systemMetric)
		{
			return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
			                                     GetSystemMetrics(systemMetric), GetSystemMetrics(systemMetric),
			                                     LR_DEFAULTCOLOR));
		}

		ATOM EnsureWindowClassRegistered()
		{
			static ATOM atom = []()
			{
				WNDCLASSEXW windowClass{};
				windowClass.cbSize = sizeof(windowClass);
				windowClass.style = CS_HREDRAW | CS_VREDRAW;
				windowClass.lpfnWndProc = Win32Window::WndProc;
				windowClass.hInstance = GetModuleHandleW(nullptr);
				windowClass.hIcon = LoadAppIcon(SM_CXICON);
				windowClass.hIconSm = LoadAppIcon(SM_CXSMICON);
				windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
				windowClass.lpszClassName = kWindowClassName;
				return RegisterClassExW(&windowClass);
			}();

			return atom;
		}

		void EnsureProcessDpiAwareness()
		{
			static const bool isDpiAware =
			    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
			(void)isDpiAware;
		}

		int GetResizeFrameThickness(HWND hwnd, int frameMetric)
		{
			const UINT dpi = GetDpiForWindow(hwnd);
			return GetSystemMetricsForDpi(frameMetric, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
		}

		std::wstring ToWideString(const std::string& value)
		{
			if (value.empty()) return {};

			const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
			if (length <= 0) return std::wstring(value.begin(), value.end());

			std::wstring wideValue(static_cast<size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wideValue.data(), length);
			wideValue.pop_back();
			return wideValue;
		}
	} // namespace

	Win32Window::~Win32Window()
	{
		Destroy();
	}

	bool Win32Window::Create(uint32_t width, uint32_t height, const std::string& title)
	{
		EnsureProcessDpiAwareness();
		if (!EnsureWindowClassRegistered()) return false;

		Destroy();

		m_title = title;
		m_width = width;
		m_height = height;
		m_shouldClose = false;
		m_isMinimized = false;

		const UINT dpi = GetDpiForSystem();
		const int scaledWidth = MulDiv(static_cast<int>(width), static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
		const int scaledHeight = MulDiv(static_cast<int>(height), static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
		m_width = static_cast<uint32_t>(scaledWidth);
		m_height = static_cast<uint32_t>(scaledHeight);

		RECT windowRect{0, 0, scaledWidth, scaledHeight};
		const DWORD style = WS_OVERLAPPEDWINDOW;
		AdjustWindowRectExForDpi(&windowRect, style, FALSE, 0, dpi);

		const std::wstring wideTitle = ToWideString(title);
		m_hwnd = CreateWindowExW(0, kWindowClassName, wideTitle.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
		                         windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr,
		                         nullptr, GetModuleHandleW(nullptr), this);

		if (!m_hwnd) return false;

		if (HICON largeIcon = LoadAppIcon(SM_CXICON))
			SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));

		if (HICON smallIcon = LoadAppIcon(SM_CXSMICON))
			SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));

		ShowWindow(m_hwnd, SW_SHOW);
		UpdateWindow(m_hwnd);
		return true;
	}

	void Win32Window::Destroy()
	{
		if (!m_hwnd) return;

		HWND hwnd = m_hwnd;
		m_hwnd = nullptr;
		m_input.ResetState();
		DestroyWindow(hwnd);
	}

	void Win32Window::PumpMessages()
	{
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}

	void Win32Window::WaitForMessages(uint32_t timeoutMs)
	{
		MsgWaitForMultipleObjects(0, nullptr, FALSE, timeoutMs, QS_ALLINPUT);
	}

	void Win32Window::SetTitleSuffix(const std::string& suffix)
	{
		if (!m_hwnd) return;

		const std::wstring wideTitle = ToWideString(m_title + suffix);
		SetWindowTextW(m_hwnd, wideTitle.c_str());
	}

	void Win32Window::SetCustomTitleBar(TitleBarHitTest hitTest)
	{
		m_titleBarHitTest = std::move(hitTest);
		if (!m_hwnd) return;

		SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
		             SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void Win32Window::Minimize()
	{
		if (m_hwnd) ShowWindow(m_hwnd, SW_MINIMIZE);
	}

	void Win32Window::ToggleMaximize()
	{
		if (m_hwnd) ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
	}

	bool Win32Window::IsWindowMaximized() const
	{
		return m_hwnd && IsZoomed(m_hwnd);
	}

	void Win32Window::RequestClose()
	{
		if (m_hwnd) PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
	}

	LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_NCCREATE)
		{
			const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
			auto* window = static_cast<Win32Window*>(createStruct->lpCreateParams);
			// Store the instance pointer so the static Win32 callback can forward messages to this object.
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
			window->m_hwnd = hwnd;
		}

		auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (!window) return DefWindowProcW(hwnd, message, wParam, lParam);

		return window->HandleMessage(message, wParam, lParam);
	}

	LRESULT Win32Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (m_messageHook)
		{
			intptr_t hookResult = 0;
			if (m_messageHook(m_hwnd, message, wParam, lParam, hookResult)) return hookResult;
		}

		switch (message)
		{
		case WM_SETFOCUS:
			m_input.SetFocused(true);
			return 0;

		case WM_KILLFOCUS:
			m_input.SetFocused(false);
			return 0;

		case WM_ACTIVATE:
			m_input.SetFocused(LOWORD(wParam) != WA_INACTIVE);
			return 0;

		case WM_CLOSE:
			if (m_closeRequestHandler && !m_closeRequestHandler()) return 0;
			m_shouldClose = true;
			Destroy();
			return 0;

		case WM_DESTROY:
			m_shouldClose = true;
			m_inSizeMove = false;
			m_hwnd = nullptr;
			m_input.ResetState();
			m_isTrackingMouseLeave = false;
			return 0;

		case WM_ENTERSIZEMOVE:
			m_inSizeMove = true;
			if (m_sizeMoveHandler) m_sizeMoveHandler(SizeMoveEvent::Begin);
			return 0;

		case WM_EXITSIZEMOVE:
			m_inSizeMove = false;
			return 0;

		case WM_SIZE:
		{
			const auto width = static_cast<uint32_t>(LOWORD(lParam));
			const auto height = static_cast<uint32_t>(HIWORD(lParam));
			const bool sizeChanged = width != m_width || height != m_height;
			m_width = width;
			m_height = height;
			m_isMinimized = wParam == SIZE_MINIMIZED;
			if (!m_isMinimized)
			{
				m_resized.Broadcast(width, height);
				if (m_inSizeMove && sizeChanged && width != 0 && height != 0 && m_sizeMoveHandler)
					m_sizeMoveHandler(SizeMoveEvent::Redraw);
			}
			return 0;
		}

		case WM_NCCALCSIZE:
			if (wParam == TRUE && m_titleBarHitTest) return CalcCustomFrameClientRect(wParam, lParam);
			break;

		case WM_NCHITTEST:
			if (m_titleBarHitTest) return HitTestCustomFrame(wParam, lParam);
			break;

		case WM_NCMOUSEMOVE:
			if (!m_titleBarHitTest) break;
			m_isMaximizeButtonHovered = wParam == HTMAXBUTTON;
			if (!m_isMaximizeButtonHovered) m_isMaximizeButtonPressed = false;
			BeginNonClientMouseLeaveTracking();
			if (m_isMaximizeButtonHovered) return 0;
			break;

		case WM_NCMOUSELEAVE:
			m_isTrackingNonClientMouseLeave = false;
			m_isMaximizeButtonHovered = false;
			m_isMaximizeButtonPressed = false;
			break;

		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONDBLCLK:
			if (!m_titleBarHitTest || wParam != HTMAXBUTTON) break;
			m_isMaximizeButtonPressed = true;
			return 0;

		case WM_NCLBUTTONUP:
			if (!m_titleBarHitTest || wParam != HTMAXBUTTON) break;
			if (m_isMaximizeButtonPressed) ToggleMaximize();
			m_isMaximizeButtonPressed = false;
			return 0;

		case WM_DPICHANGED:
		{
			const RECT* suggestedRect = reinterpret_cast<const RECT*>(lParam);
			SetWindowPos(m_hwnd, nullptr, suggestedRect->left, suggestedRect->top,
			             suggestedRect->right - suggestedRect->left, suggestedRect->bottom - suggestedRect->top,
			             SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}

		case WM_MOUSEMOVE:
			m_isMaximizeButtonHovered = false;
			m_isMaximizeButtonPressed = false;
			m_isTrackingNonClientMouseLeave = false;
			BeginMouseLeaveTracking();
			m_input.NotifyMouseMove(
			    {static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))});
			return 0;

		case WM_MOUSELEAVE:
			m_isTrackingMouseLeave = false;
			m_input.ResetMouseState();
			return 0;

		case WM_MOUSEWHEEL:
			m_input.NotifyMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
			                         static_cast<float>(WHEEL_DELTA));
			return 0;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
		{
			const KeyCode key = TranslateMouseButton(message, wParam);
			if (key != KeyCode::None) m_input.NotifyKeyDown(key);
			return 0;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			const KeyCode key = TranslateMouseButton(message, wParam);
			if (key != KeyCode::None) m_input.NotifyKeyUp(key);
			return 0;
		}

		case WM_KEYDOWN:
			// Bit 30 marks autorepeat; filtering it keeps IsKeyPressed edge-based.
			if ((lParam & (1LL << 30)) == 0) m_input.NotifyKeyDown(TranslateKey(wParam, lParam));
			return 0;

		case WM_KEYUP:
			m_input.NotifyKeyUp(TranslateKey(wParam, lParam));
			return 0;

		case WM_SYSKEYDOWN:
			if ((lParam & (1LL << 30)) == 0) m_input.NotifyKeyDown(TranslateKey(wParam, lParam));
			break;

		case WM_SYSKEYUP:
			m_input.NotifyKeyUp(TranslateKey(wParam, lParam));
			break;

		case WM_SYSCHAR:
			if (wParam != VK_SPACE) return 0;
			break;

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_KEYMENU && lParam != VK_SPACE) return 0;
			break;
		}

		return DefWindowProcW(m_hwnd, message, wParam, lParam);
	}

	KeyCode Win32Window::TranslateKey(WPARAM wParam, LPARAM lParam)
	{
		switch (wParam)
		{
		case VK_SHIFT:
			return (MapVirtualKeyW((lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT) ? KeyCode::RightShift
			                                                                                : KeyCode::LeftShift;
		case VK_CONTROL:
			return (lParam & (1LL << 24)) ? KeyCode::RightControl : KeyCode::LeftControl;
		case VK_MENU:
			return (lParam & (1LL << 24)) ? KeyCode::RightAlt : KeyCode::LeftAlt;
		default:
			break;
		}

		if (wParam >= static_cast<WPARAM>(KeyCode::Backspace) && wParam <= static_cast<WPARAM>(KeyCode::RightAlt))
			return static_cast<KeyCode>(wParam);

		return KeyCode::None;
	}

	LRESULT Win32Window::CalcCustomFrameClientRect(WPARAM wParam, LPARAM lParam)
	{
		auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
		const LONG windowTop = params->rgrc[0].top;
		const LRESULT result = DefWindowProcW(m_hwnd, WM_NCCALCSIZE, wParam, lParam);
		if (result != 0) return result;

		params->rgrc[0].top = windowTop;
		if (IsZoomed(m_hwnd)) params->rgrc[0].top += GetResizeFrameThickness(m_hwnd, SM_CYFRAME);
		return 0;
	}

	LRESULT Win32Window::HitTestCustomFrame(WPARAM wParam, LPARAM lParam)
	{
		const LRESULT hit = DefWindowProcW(m_hwnd, WM_NCHITTEST, wParam, lParam);

		POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		ScreenToClient(m_hwnd, &point);
		const bool isOnTopEdge = !IsZoomed(m_hwnd) && point.y < GetResizeFrameThickness(m_hwnd, SM_CYFRAME);

		if (isOnTopEdge && hit == HTLEFT) return HTTOPLEFT;
		if (isOnTopEdge && hit == HTRIGHT) return HTTOPRIGHT;
		if (hit != HTCLIENT) return hit;
		if (isOnTopEdge) return HTTOP;

		switch (m_titleBarHitTest(point.x, point.y))
		{
		case WindowHitArea::Caption:
			return HTCAPTION;
		case WindowHitArea::MaximizeButton:
			return HTMAXBUTTON;
		default:
			return HTCLIENT;
		}
	}

	void Win32Window::BeginMouseLeaveTracking()
	{
		if (m_isTrackingMouseLeave || !m_hwnd) return;

		TRACKMOUSEEVENT trackMouseEvent{};
		trackMouseEvent.cbSize = sizeof(trackMouseEvent);
		trackMouseEvent.dwFlags = TME_LEAVE;
		trackMouseEvent.hwndTrack = m_hwnd;
		m_isTrackingMouseLeave = TrackMouseEvent(&trackMouseEvent) == TRUE;
	}

	void Win32Window::BeginNonClientMouseLeaveTracking()
	{
		if (m_isTrackingNonClientMouseLeave || !m_hwnd) return;

		TRACKMOUSEEVENT trackMouseEvent{};
		trackMouseEvent.cbSize = sizeof(trackMouseEvent);
		trackMouseEvent.dwFlags = TME_LEAVE | TME_NONCLIENT;
		trackMouseEvent.hwndTrack = m_hwnd;
		m_isTrackingNonClientMouseLeave = TrackMouseEvent(&trackMouseEvent) == TRUE;
	}

	KeyCode Win32Window::TranslateMouseButton(UINT message, WPARAM wParam)
	{
		switch (message)
		{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			return KeyCode::MouseLeft;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			return KeyCode::MouseRight;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			return KeyCode::MouseMiddle;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			return HIWORD(wParam) == XBUTTON1 ? KeyCode::MouseX1 : KeyCode::MouseX2;
		default:
			return KeyCode::None;
		}
	}
} // namespace NeneEngine
