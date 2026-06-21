#include "Platform/Win32/Win32Window.h"
#include "Resource.h"

#include <windowsx.h>

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
			windowClass.lpfnWndProc = NeneEngine::Win32Window::WndProc;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.hIcon = LoadAppIcon(SM_CXICON);
			windowClass.hIconSm = LoadAppIcon(SM_CXSMICON);
			windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
			windowClass.lpszClassName = kWindowClassName;
			return RegisterClassExW(&windowClass);
		}();

		return atom;
	}
} // namespace

namespace NeneEngine
{
	namespace
	{
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
		if (!EnsureWindowClassRegistered()) return false;

		Destroy();

		m_title = title;
		m_width = width;
		m_height = height;
		m_shouldClose = false;

		RECT windowRect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
		const DWORD style = WS_OVERLAPPEDWINDOW;
		AdjustWindowRect(&windowRect, style, FALSE);

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
			m_shouldClose = true;
			m_input.ResetState();
			DestroyWindow(m_hwnd);
			return 0;

		case WM_DESTROY:
			m_shouldClose = true;
			m_input.ResetState();
			m_isTrackingMouseLeave = false;
			return 0;

		case WM_SIZE:
			m_width = static_cast<uint32_t>(LOWORD(lParam));
			m_height = static_cast<uint32_t>(HIWORD(lParam));
			if (wParam != SIZE_MINIMIZED) m_resized.Broadcast(m_width, m_height);
			return 0;

		case WM_MOUSEMOVE:
			BeginMouseLeaveTracking();
			m_input.NotifyMouseMove(
			    {static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))});
			return 0;

		case WM_MOUSELEAVE:
			m_isTrackingMouseLeave = false;
			m_input.ResetState();
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
		case WM_SYSKEYDOWN:
			// Bit 30 marks autorepeat; filtering it keeps IsKeyPressed edge-based.
			if ((lParam & (1LL << 30)) == 0) m_input.NotifyKeyDown(TranslateKey(wParam, lParam));
			return 0;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			m_input.NotifyKeyUp(TranslateKey(wParam, lParam));
			return 0;
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

	void Win32Window::BeginMouseLeaveTracking()
	{
		if (m_isTrackingMouseLeave || !m_hwnd) return;

		TRACKMOUSEEVENT trackMouseEvent{};
		trackMouseEvent.cbSize = sizeof(trackMouseEvent);
		trackMouseEvent.dwFlags = TME_LEAVE;
		trackMouseEvent.hwndTrack = m_hwnd;
		m_isTrackingMouseLeave = TrackMouseEvent(&trackMouseEvent) == TRUE;
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
