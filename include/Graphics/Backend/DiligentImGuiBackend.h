// DiligentImGuiBackend.h

#pragma once

#include "Graphics/Backend/IImGuiBackend.h"

#include <EASTL/unique_ptr.h>
#include <filesystem>
#include <string>

namespace Diligent
{
	class ImGuiImplWin32;
}

namespace NeneEngine
{
	class DiligentDX12Adapter;

	class DiligentImGuiBackend final : public IImGuiBackend
	{
	  public:
		DiligentImGuiBackend(DiligentDX12Adapter& adapter, HWND hwnd, const std::filesystem::path& iniFilePath);
		~DiligentImGuiBackend() override;

		DiligentImGuiBackend(const DiligentImGuiBackend&) = delete;
		DiligentImGuiBackend& operator=(const DiligentImGuiBackend&) = delete;

		void BeginFrame() override;
		void Render() override;
		bool HandleWindowMessage(HWND hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam,
		                         intptr_t& result) override;

	  private:
		DiligentDX12Adapter& m_adapter;
		std::string m_iniFilePath;
		eastl::unique_ptr<Diligent::ImGuiImplWin32> m_impl;
		bool m_isFrameStarted = false;
	};

} // namespace NeneEngine
