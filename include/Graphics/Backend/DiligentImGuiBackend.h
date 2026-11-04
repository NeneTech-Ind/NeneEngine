// DiligentImGuiBackend.h

#pragma once

#include "Graphics/Backend/IImGuiBackend.h"

#include "../external/DiligentEngine/DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Texture.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/unordered_map.h>
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
		[[nodiscard]] float GetDpiScale() const override;
		ImGuiTexture RegisterTexture(const std::filesystem::path& path) override;
		void ReleaseTexture(const ImGuiTexture& texture) override;

	  private:
		DiligentDX12Adapter& m_adapter;
		HWND m_hwnd = nullptr;
		std::string m_iniFilePath;
		eastl::unique_ptr<Diligent::ImGuiImplWin32> m_impl;
		eastl::unordered_map<uint64_t, Diligent::RefCntAutoPtr<Diligent::ITexture>> m_textures;
		bool m_isFrameStarted = false;
	};

} // namespace NeneEngine
