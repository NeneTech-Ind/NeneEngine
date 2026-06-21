#pragma once

#include "Input/InputDevice.h"
#include "Platform/IWindow.h"

#include <string>

namespace NeneEngine
{
	class Win32Window final : public IWindow
	{
	  public:
		Win32Window() = default;
		~Win32Window() override;

		bool Create(uint32_t width, uint32_t height, const std::string& title) override;
		void Destroy() override;

		void PumpMessages() override;
		bool ShouldClose() const override { return m_shouldClose; }

		HWND GetHWND() const override { return m_hwnd; }
		std::string GetTitle() const override { return m_title; }
		uint32_t GetWidth() const override { return m_width; }
		uint32_t GetHeight() const override { return m_height; }
		InputDevice& GetInput() override { return m_input; }
		const InputDevice& GetInput() const override { return m_input; }
		MulticastDelegate<uint32_t, uint32_t>& OnResized() override { return m_resized; }

		static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	  private:
		LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
		void BeginMouseLeaveTracking();
		static KeyCode TranslateKey(WPARAM wParam, LPARAM lParam);
		static KeyCode TranslateMouseButton(UINT message, WPARAM wParam);

		HWND m_hwnd = nullptr;
		std::string m_title;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		bool m_shouldClose = false;
		bool m_isTrackingMouseLeave = false;
		InputDevice m_input;
		MulticastDelegate<uint32_t, uint32_t> m_resized;
	};
} // namespace NeneEngine
