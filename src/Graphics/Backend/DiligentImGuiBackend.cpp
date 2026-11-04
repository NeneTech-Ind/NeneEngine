// DiligentImGuiBackend.cpp

#include "Graphics/Backend/DiligentImGuiBackend.h"

#include "Core/NeneLogger.h"
#include "Graphics/Backend/DiligentDX12Adapter.h"

#include <Windows.h>

#include "../external/DiligentEngine/DiligentTools/Imgui/interface/ImGuiImplWin32.hpp"
#include "../external/DiligentEngine/DiligentTools/TextureLoader/interface/TextureLoader.h"

#include <imgui.h>

namespace NeneEngine
{

	using namespace Diligent;

	DiligentImGuiBackend::DiligentImGuiBackend(DiligentDX12Adapter& adapter, HWND hwnd,
	                                           const std::filesystem::path& iniFilePath)
	    : m_adapter(adapter), m_hwnd(hwnd), m_iniFilePath(iniFilePath.string())
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

	float DiligentImGuiBackend::GetDpiScale() const
	{
		const UINT dpi = m_hwnd != nullptr ? GetDpiForWindow(m_hwnd) : 0;
		return dpi != 0 ? static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI) : 1.0f;
	}

	ImGuiTexture DiligentImGuiBackend::RegisterTexture(const std::filesystem::path& path)
	{
		const std::u8string utf8Path = path.u8string();
		const std::string pathString(utf8Path.begin(), utf8Path.end());

		TextureLoadInfo loadInfo{};
		loadInfo.Name = pathString.c_str();
		loadInfo.IsSRGB = false;

		RefCntAutoPtr<ITextureLoader> textureLoader;
		CreateTextureLoaderFromFile(pathString.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, loadInfo, &textureLoader);
		if (!textureLoader)
		{
			NENE_LOG_ERROR("DiligentImGuiBackend: failed to load texture '{}'", pathString);
			return {};
		}

		RefCntAutoPtr<ITexture> texture;
		textureLoader->CreateTexture(m_adapter.GetDevice(), &texture);
		ITextureView* view = texture ? texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
		if (view == nullptr)
		{
			NENE_LOG_ERROR("DiligentImGuiBackend: failed to create GPU texture for '{}'", pathString);
			return {};
		}

		const TextureDesc& desc = texture->GetDesc();
		const ImGuiTexture result{reinterpret_cast<uint64_t>(view), desc.Width, desc.Height};
		m_textures.emplace(result.id, std::move(texture));
		return result;
	}

	void DiligentImGuiBackend::ReleaseTexture(const ImGuiTexture& texture)
	{
		m_textures.erase(texture.id);
	}

} // namespace NeneEngine
