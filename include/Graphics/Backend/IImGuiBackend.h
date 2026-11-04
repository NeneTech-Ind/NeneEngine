// IImGuiBackend.h

#pragma once

#include <cstdint>
#include <filesystem>
#include <wtypes.h>

namespace NeneEngine
{

	struct ImGuiTexture
	{
		uint64_t id = 0;
		uint32_t width = 0;
		uint32_t height = 0;

		[[nodiscard]] bool IsValid() const { return id != 0; }
	};

	class IImGuiBackend
	{
	  public:
		virtual ~IImGuiBackend() = default;

		virtual void BeginFrame() = 0;
		virtual void Render() = 0;

		virtual bool HandleWindowMessage(HWND hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam,
		                                 intptr_t& result) = 0;

		virtual ImGuiTexture RegisterTexture(const std::filesystem::path& path) = 0;
		virtual void ReleaseTexture(const ImGuiTexture& texture) = 0;
	};

} // namespace NeneEngine
