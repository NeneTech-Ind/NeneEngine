// DiligentImGuiBackend.cpp

#include "Graphics/Backend/DiligentImGuiBackend.h"

#include "Graphics/Backend/DiligentDX12Adapter.h"

#include <Windows.h>

#include "../external/DiligentEngine/DiligentTools/Imgui/interface/ImGuiImplWin32.hpp"

#include <imgui.h>

namespace NeneEngine
{

	using namespace Diligent;

	DiligentImGuiBackend::DiligentImGuiBackend(DiligentDX12Adapter& adapter, HWND hwnd,
	                                           const std::filesystem::path& iniFilePath)
	    : m_adapter(adapter), m_iniFilePath(iniFilePath.string())
	{
		const SwapChainDesc& swapChainDesc = m_adapter.GetSwapChain()->GetDesc();
		m_impl = eastl::make_unique<ImGuiImplWin32>(ImGuiDiligentCreateInfo{m_adapter.GetDevice(),
		                                                                    swapChainDesc.ColorBufferFormat,
		                                                                    swapChainDesc.DepthBufferFormat},
		                                            hwnd);

		ImGui::GetIO().IniFilename = m_iniFilePath.c_str();
	}

	DiligentImGuiBackend::~DiligentImGuiBackend() = default;

	void DiligentImGuiBackend::BeginFrame()
	{
		if (m_isFrameStarted) m_impl->EndFrame();

		const SwapChainDesc& swapChainDesc = m_adapter.GetSwapChain()->GetDesc();
		m_impl->NewFrame(swapChainDesc.Width, swapChainDesc.Height, SURFACE_TRANSFORM_IDENTITY);
		m_isFrameStarted = true;
	}

	void DiligentImGuiBackend::Render()
	{
		if (!m_isFrameStarted) return;

		m_impl->Render(m_adapter.GetImmediateContext());
		m_isFrameStarted = false;
	}

	bool DiligentImGuiBackend::HandleWindowMessage(HWND hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam,
	                                               intptr_t& result)
	{
		const LRESULT handlerResult = m_impl->Win32_ProcHandler(hwnd, message, wParam, lParam);

		if (message == WM_SETCURSOR && handlerResult != 0)
		{
			result = TRUE;
			return true;
		}

		return false;
	}

} // namespace NeneEngine
