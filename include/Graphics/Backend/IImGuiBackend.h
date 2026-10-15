// IImGuiBackend.h

#pragma once

#include <cstdint>
#include <wtypes.h>

namespace NeneEngine
{

	class IImGuiBackend
	{
	  public:
		virtual ~IImGuiBackend() = default;

		virtual void BeginFrame() = 0;
		virtual void Render() = 0;

		virtual bool HandleWindowMessage(HWND hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam,
		                                 intptr_t& result) = 0;
	};

} // namespace NeneEngine
