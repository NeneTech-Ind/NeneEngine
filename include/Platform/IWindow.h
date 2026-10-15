// IWindow.h

#pragma once

#include "Core/Delegate.h"

#include <EASTL/functional.h>
#include <cstdint>
#include <string>
#include <wtypes.h>

namespace NeneEngine
{
	class InputDevice;

	class IWindow
	{
	  public:
		using MessageHook =
		    eastl::function<bool(HWND hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam, intptr_t& result)>;

		virtual ~IWindow() = default;

		virtual bool Create(uint32_t width, uint32_t height, const std::string& title) = 0;
		virtual void Destroy() = 0;

		virtual void PumpMessages() = 0;
		virtual void WaitForMessages(uint32_t timeoutMs) = 0;
		virtual bool ShouldClose() const = 0;
		virtual bool IsMinimized() const = 0;

		virtual HWND GetHWND() const = 0;
		virtual std::string GetTitle() const = 0;
		virtual void SetTitleSuffix(const std::string& suffix) = 0;
		virtual void SetMessageHook(MessageHook hook) = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual InputDevice& GetInput() = 0;
		virtual const InputDevice& GetInput() const = 0;
		virtual MulticastDelegate<uint32_t, uint32_t>& OnResized() = 0;
	};

} // namespace NeneEngine
