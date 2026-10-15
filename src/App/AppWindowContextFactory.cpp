// AppWindowContextFactory.cpp

#include "App/AppWindowContextFactory.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Graphics/Backend/DiligentDX12Adapter.h"
#include "Graphics/Backend/DiligentImGuiBackend.h"
#include "Platform/Win32/Win32Window.h"

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kImGuiIniFileName = "imgui.ini";
	} // namespace

	eastl::optional<AppWindowContext> AppWindowContextFactory::Create(uint32_t width, uint32_t height,
	                                                                  const std::string& title,
	                                                                  ECS::Entity cameraEntity, bool isMain)
	{
		AppWindowContext windowContext{};
		windowContext.title = title;
		windowContext.cameraEntity = cameraEntity;
		windowContext.isMain = isMain;
		windowContext.window = eastl::make_unique<Win32Window>();
		if (!windowContext.window->Create(width, height, title))
		{
			NENE_LOG_ERROR("Failed to create window '{}'", title);
			return eastl::nullopt;
		}

		if (!m_sharedDevice) m_sharedDevice = eastl::make_shared<DiligentDX12SharedDevice>();

		auto renderer = eastl::make_unique<DiligentDX12Adapter>(m_sharedDevice);
		if (!renderer->Init(windowContext.window->GetHWND(), width, height))
		{
			NENE_LOG_ERROR("Failed to initialize renderer for window '{}'", title);
			return eastl::nullopt;
		}

		if (isMain)
		{
			windowContext.imguiBackend = eastl::make_unique<DiligentImGuiBackend>(
			    *renderer, windowContext.window->GetHWND(), GetExecutableDirectory() / kImGuiIniFileName);
			windowContext.window->SetMessageHook(
			    [imguiBackend = windowContext.imguiBackend.get()](HWND hwnd, uint32_t message, uintptr_t wParam,
			                                                      intptr_t lParam, intptr_t& result)
			    { return imguiBackend->HandleWindowMessage(hwnd, message, wParam, lParam, result); });
		}

		windowContext.renderer = std::move(renderer);
		windowContext.inputManager.SetInputDevice(&windowContext.window->GetInput());
		windowContext.renderSystem = eastl::make_unique<ECS::RenderSystem>(windowContext.renderer.get(), cameraEntity);
		return windowContext;
	}

} // namespace NeneEngine
